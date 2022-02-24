#include "RemoteClient.h"
#include "ServerApplication.h"

#include "../Logger.h"
#include "../Packets/NetPacket.h"
#include "../Packets/Protocol.h"

#include <memory>

ClientProcessingResult RemoteClient::ReadData()
{
	// First, we read the initial 4 bytes (32-bit integer) of the packet, which represent the whole packet's length
	if (this->currentPacketLengthPosIndex < 4)
	{
		// Even these 4 bytes might come in several recv() calls, we must handle that.
		int remainingPacketLengthBytes = 4 - this->currentPacketLengthPosIndex;
		int retval = recv(this->s, (char*)this->currentPacketLengthBytes + this->currentPacketLengthPosIndex, remainingPacketLengthBytes, 0);
		if (retval == 0)
		{
			return ClientProcessingResult::CloseConnection;
		}
		else if (retval == SOCKET_ERROR)
		{
			// If actual error occurred, terminate the connection
			if (WSAGetLastError() != WSAEWOULDBLOCK)
			{
				LogWarning("Failed to read from %s [error %u]", this->ipAddress.c_str(), WSAGetLastError());
				return ClientProcessingResult::TerminateConnection;
			}
			// if disconnection is scheduled, and there is no pending incoming packet, and everything has been sent, then we disconnect
			else if (this->disconnectionInProgress && this->currentPacketLengthPosIndex == 0 && this->outgoingDataBuffer.empty())
			{
				shutdown(this->s, SD_BOTH);
				return ClientProcessingResult::CloseConnection;
			}
		}
		else if (retval > 0)
		{
			this->currentPacketLengthPosIndex += retval;
		}

		// If we just received the full information about packet size, we must prepare for reading the actual packet
		if (this->currentPacketLengthPosIndex == 4)
		{
			static_assert(sizeof(int) == 4, "sizeof(int) is not 4");

			int packetLength = 0;
			memcpy(&packetLength, this->currentPacketLengthBytes, 4);

			this->currentPacketPosIndex = 0;
			this->currentPacketBytes.clear();
			this->currentPacketBytes.resize(packetLength);
		}
	}

	// Then, once we received the whole packet length, we read the packet
	if (this->currentPacketLengthPosIndex == 4)
	{
		int remainingPacketBytes = this->currentPacketBytes.size() - this->currentPacketPosIndex;
		int retval = recv(this->s, (char*)this->currentPacketBytes.data() + this->currentPacketPosIndex, remainingPacketBytes, 0);
		if (retval == 0)
		{
			return ClientProcessingResult::CloseConnection;
		}
		else if (retval == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
		{
			LogWarning("Failed to read from %s [error %u]", this->ipAddress.c_str(), WSAGetLastError());
			return ClientProcessingResult::TerminateConnection;
		}
		else if (retval > 0)
		{
			this->currentPacketPosIndex += retval;
		}

		// If we received the entire packet, reset all progress variables (prepare for the next packet) and process the current one
		if (this->currentPacketBytes.size() == (size_t)this->currentPacketPosIndex)
		{
			this->currentPacketLengthPosIndex = 0;

			// if disconnection is ongoing, don't accept any more data, instead we just wait for recv() to return 0
			if (!this->disconnectionInProgress)
			{
				std::unique_ptr<NetPacket> currentPacket = std::make_unique<NetPacket>(this->currentPacketBytes.data(), this->currentPacketBytes.size());

				try
				{
					ClientProcessingResult packetResult = this->ProcessPacket(currentPacket.get());
					if (packetResult == ClientProcessingResult::CloseConnection)
					{
						// don't disconnect immediately, we'll wait for data to be sent first
						this->disconnectionInProgress = true;
						return ClientProcessingResult::Continue;
					}
					else
					{
						return packetResult;
					}
				}
				catch (const std::exception& ex)
				{
					LogError("Packet processing failed: %s", ex.what());
					return ClientProcessingResult::TerminateConnection;
				}
			}
		}
	}

	// Otherwise, if we're still waiting for the packet, let the connection continue.
	return ClientProcessingResult::Continue;
}

ClientProcessingResult RemoteClient::SendPendingData()
{
	if (this->outgoingDataBuffer.empty())
	{
		if (this->disconnectionInProgress)
		{
			shutdown(this->s, SD_SEND);
		}

		return ClientProcessingResult::Continue;
	}

	int remainingPacketBytes = this->outgoingDataBuffer.size() - this->outgoingDataPosIndex;
	if (remainingPacketBytes > 0)
	{
		int retval = send(this->s, (const char*)this->outgoingDataBuffer.data() + this->outgoingDataPosIndex, remainingPacketBytes, 0);
		if (retval == 0)
		{
			return ClientProcessingResult::CloseConnection;
		}
		else if (retval == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
		{
			LogWarning("Failed to send to %s [error %u]", this->ipAddress.c_str(), WSAGetLastError());
			return ClientProcessingResult::TerminateConnection;
		}
		else if (retval > 0)
		{
			this->outgoingDataPosIndex += retval;
		}

		// we have successfully sent all data so far, so we clear the buffer
		if (this->outgoingDataBuffer.size() == (size_t)this->outgoingDataPosIndex)
		{
			this->outgoingDataPosIndex = 0;
			this->outgoingDataBuffer.clear();
		}
	}

	return ClientProcessingResult::Continue;
}

void RemoteClient::SendChatList()
{
	if (!this->IsLoggedIn())
	{
		return;
	}

	DatabaseChatRoomList chatList;
	sApp->GetDB()->GetChatsForUser(this->GetUserID(), &chatList);

	this->SetActiveChatID(INVALID_CHAT_ID);

	PKT_S2C_ReplaceChatList pkt;
	pkt.rooms = chatList.chats;

	this->SendPacket(pkt.Serialize());
}

void RemoteClient::SendParticipantList()
{
	if (!this->IsLoggedIn() || this->GetActiveChatID() == INVALID_CHAT_ID)
	{
		return;
	}

	DatabaseUserInfoList userList;
	sApp->GetDB()->GetUsersInChat(this->GetActiveChatID(), &userList);

	for (size_t i = 0; i < userList.users.size(); ++i)
	{
		// Replace online users' "last seen time" with a special value that indicates that they're online
		if (sApp->GetLoggedInClient(userList.users[i].userId))
		{
			userList.users[i].lastSeen = CURRENTLY_ONLINE;
		}
	}

	PKT_S2C_ReplaceParticipantList pkt;
	pkt.users = userList.users;

	this->SendPacket(pkt.Serialize());
}

void RemoteClient::SendPacket(std::unique_ptr<NetPacket> pkt)
{
	if (disconnectionInProgress)
	{
		return;
	}

	uint32_t packetLength = pkt->GetLength();

	uint8_t packetLengthBytes[sizeof(packetLength)];
	memcpy(packetLengthBytes, &packetLength, sizeof(packetLength));

	this->outgoingDataBuffer.insert(this->outgoingDataBuffer.end(), packetLengthBytes, packetLengthBytes + sizeof(packetLength));
	this->outgoingDataBuffer.insert(this->outgoingDataBuffer.end(), pkt->GetData(), pkt->GetData() + pkt->GetLength());
}

ClientProcessingResult RemoteClient::Update()
{
	ClientProcessingResult recvResult = this->ReadData();
	if (recvResult == ClientProcessingResult::TerminateConnection)
	{
		return recvResult;
	}

	if (this->SendPendingData() == ClientProcessingResult::TerminateConnection)
	{
		return ClientProcessingResult::TerminateConnection;
	}

	return recvResult;
}

void RemoteClient::ShowMessageBox(const std::string& message, bool shouldDisconnect)
{
	PKT_S2C_MessageBox pkt;
	pkt.message = message;
	pkt.isDisconnection = shouldDisconnect;

	this->SendPacket(pkt.Serialize());

	if (shouldDisconnect)
	{
		this->disconnectionInProgress = true;
	}
}

void RemoteClient::ResetConnectionOnClose()
{
	linger l;
	l.l_onoff = 1;
	l.l_linger = 0;

	setsockopt(this->s, SOL_SOCKET, SO_LINGER, (const char*)&l, sizeof(l));
}

RemoteClient::RemoteClient(SOCKET s, sockaddr_in6* sockaddr_s)
{
	this->s = s;
	memcpy(&this->sockaddr_s, sockaddr_s, sizeof(sockaddr_in6));

	char ipBuf[64];
	this->ipAddress = inet_ntop(sockaddr_s->sin6_family, &sockaddr_s->sin6_addr, ipBuf, sizeof(ipBuf));

	this->userId = INVALID_USER_ID;
	this->openChatId = INVALID_CHAT_ID;

	this->currentPacketLengthPosIndex = 0;
	this->currentPacketPosIndex = 0;
	this->outgoingDataPosIndex = 0;
	this->disconnectionInProgress = false;

	LogInfo("New incoming connection from %s", this->ipAddress.c_str());
}

RemoteClient::~RemoteClient()
{
	closesocket(this->s);

	LogInfo("Connection from %s closed", this->ipAddress.c_str());
}
