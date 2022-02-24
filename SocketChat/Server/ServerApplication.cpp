#include "ServerApplication.h"
#include "RemoteClient.h"
#include "DatabaseInterface.h"
#include "../Logger.h"
#include "../Packets/NetPacket.h"
#include "../Packets/Protocol.h"

#include <WS2tcpip.h>
#include <bcrypt.h>
#include <winternl.h>

#include <algorithm>

ServerSocketApp* sApp;

ServerSocketApp::ServerSocketApp()
{
	this->dbConnection = std::make_unique<DatabaseInterface>("E:\\chatserver.db");
	LogInfo("Database loaded");

	// Create a server socket; an IPv6 socket can handle both IPv4 and IPv6 connections
	this->serverSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

	if (this->serverSocket == INVALID_SOCKET)
	{
		LogWarning("Failed to create socket [error %u]", WSAGetLastError());
		return;
	}

	addrinfo addr = { 0 };
	addr.ai_family = AF_INET6;
	addr.ai_socktype = SOCK_STREAM;
	addr.ai_protocol = IPPROTO_TCP;
	addr.ai_flags = AI_PASSIVE; // AI_PASSIVE = get the address for listening on all interfaces

	addrinfo* result;

	// get the addrinfo of "::" address (listening everywhere)
	if (getaddrinfo(nullptr, std::to_string(ServerPort).c_str(), &addr, &result) != 0)
	{
		LogWarning("getaddrinfo() failed");
	}

	// disable IPV6_V6ONLY to enable the IPv6 socket to accept IPv4 connections as well
	int ipv6Only = 0;
	if (setsockopt(this->serverSocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&ipv6Only, sizeof(ipv6Only)) != 0)
	{
		LogWarning("setsockopt() failed [error %u]", WSAGetLastError());
	}

	this->SetSocketNonBlocking(this->serverSocket);

	if (bind(this->serverSocket, result->ai_addr, result->ai_addrlen) != 0)
	{
		LogWarning("bind() failed [error %u]", WSAGetLastError());
	}

	if (listen(this->serverSocket, SOMAXCONN) != 0)
	{
		LogWarning("listen() failed [error %u]", WSAGetLastError());
	}

	LogInfo("Socket listening on port %u", ServerPort);

	// global instance of this application
	sApp = this;
}

ServerSocketApp::~ServerSocketApp()
{
}

RemoteClient* ServerSocketApp::GetLoggedInClient(uint64_t userId)
{
	for (const auto& client : this->connectedClients)
	{
		if (client->IsLoggedIn() && client->GetUserID() == userId)
		{
			return client.get();
		}
	}

	return nullptr;
}

void ServerSocketApp::AcceptIncomingConnections()
{
	SOCKET clientSocket = INVALID_SOCKET;
	sockaddr_in6 clientAddr = { 0 };
	int clientAddrLen = sizeof(clientAddr);

	while ((clientSocket = accept(this->serverSocket, (sockaddr*)&clientAddr, &clientAddrLen)) != INVALID_SOCKET)
	{
		this->SetSocketNonBlocking(clientSocket);
		connectedClients.push_back(std::make_unique<RemoteClient>(clientSocket, &clientAddr));
	}

	// this "error" simply means that no connections were available and accept() returned nothing
	// anything else is an actual error, though
	if (WSAGetLastError() != WSAEWOULDBLOCK)
	{
		LogError("accept() failed [error %u]", WSAGetLastError());
	}
}

void ServerSocketApp::UpdateConnections()
{
	for (size_t i = 0; i < this->connectedClients.size(); ++i)
	{
		RemoteClient* client = this->connectedClients[i].get();
		ClientProcessingResult result = client->Update();

		if (result == ClientProcessingResult::Continue)
		{
			continue;
		}
		else
		{
			if (result == ClientProcessingResult::TerminateConnection)
			{
				client->ResetConnectionOnClose();
			}

			// This will trigger RemoteClient's destructor and thus close the socket
			this->connectedClients[i].swap(this->connectedClients[this->connectedClients.size() - 1]);
			this->connectedClients.pop_back();
			--i;
		}
	}
}

void ServerSocketApp::UpdateLastSeenTimes()
{
	for (const auto& client : this->connectedClients)
	{
		if (client->IsLoggedIn())
		{
			this->dbConnection->UpdateLastSeenTime(client->GetUserID());
		}
	}
}

void ServerSocketApp::UpdateReadReceipts()
{
	for (const auto& client : this->connectedClients)
	{
		if (client->IsLoggedIn() && client->GetActiveChatID() != INVALID_CHAT_ID)
		{
			this->SetChatReadByUser(client->GetActiveChatID(), client->GetUserID());
		}
	}
}

void ServerSocketApp::UpdateParticipantLists()
{
	for (const auto& client : this->connectedClients)
	{
		if (client->IsLoggedIn() && client->GetActiveChatID() != INVALID_CHAT_ID)
		{
			client->SendParticipantList();
		}
	}
}

void ServerSocketApp::SetSocketNonBlocking(SOCKET s)
{
	// enable TCP_NODELAY to reduce latency
	int noDelay = 1;
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&noDelay, sizeof(noDelay)) != 0)
	{
		LogWarning("setsockopt() failed [error %u]", WSAGetLastError());
	}

	// set the socket as non-blocking - recv(), accept() etc. will never wait and return immediately if there's no data/no incoming connection
	u_long nonBlocking = 1;
	if (ioctlsocket(s, FIONBIO, &nonBlocking) != 0)
	{
		LogWarning("ioctlsocket() failed [error %u]", WSAGetLastError());
	}
}

LoginResult ServerSocketApp::CreateLoginSession(std::string username, uint64_t* userId)
{
	// accept only usernames of length between 3 and 24 characters
	if (username.size() < 3 || username.size() > 24)
	{
		return LoginResult::UsernameWrongLength;
	}

	// disconnect all existing users that are logged in with this username
	for (const auto& client : this->connectedClients)
	{
		if (client->IsLoggedIn() && client->GetUsername() == username)
		{
			client->ShowMessageBox("Logged out by another client", true);
		}
	}

	DatabaseUserInfo userInfo = { 0 };
	if (!this->dbConnection->GetUserByName(username, &userInfo))
	{
		this->dbConnection->CreateUser(username);
		this->dbConnection->GetUserByName(username, &userInfo);
	}

	*userId = userInfo.userId;
	return LoginResult::Success;
}

ChatCreateResult ServerSocketApp::CreateChat(uint64_t ownerUserId, std::vector<uint64_t> participants, bool isGroupChat)
{
	participants.push_back(ownerUserId);

	std::sort(participants.begin(), participants.end());
	participants.erase(std::unique(participants.begin(), participants.end()), participants.end());
	
	DatabaseUserInfo userInfo;
	for (uint64_t userId : participants)
	{
		if (!this->dbConnection->GetUserById(userId, &userInfo))
		{
			return ChatCreateResult::UserNotFound;
		}
	}

	uint64_t chatId = this->dbConnection->CreateChat(ownerUserId, participants, isGroupChat);

	DatabaseChatRoomInfo chatInfo;
	this->dbConnection->GetChatById(chatId, &chatInfo);

	PKT_S2C_NewChat newChatPkt;
	newChatPkt.chatID = chatId;
	newChatPkt.chatName = chatInfo.chatName;

	for (uint64_t userId : participants)
	{
		// flash the window for everybody except the one who created the chat room
		newChatPkt.flashWindow = userId != ownerUserId;

		RemoteClient* client = sApp->GetLoggedInClient(userId);
		if (client)
		{
			client->SendPacket(newChatPkt.Serialize());
		}
	}

	return ChatCreateResult::Success;
}

void ServerSocketApp::SetChatReadByUser(uint64_t chatId, uint64_t userId)
{
	DatabaseChatRoomInfo chatInfo;
	if (!this->dbConnection->GetChatById(chatId, &chatInfo))
	{
		return;
	}

	if (!this->dbConnection->SetChatReadByUser(chatId, userId))
	{
		return;
	}

	LogInfo("Read receipt updated in chat %u for user %u", chatId, userId);

	PKT_S2C_ChatReadReceipt pkt;
	pkt.chatId = chatId;
	pkt.userId = userId;

	for (uint64_t participantId : chatInfo.allParticipants)
	{
		RemoteClient* client = sApp->GetLoggedInClient(participantId);
		if (client)
		{
			client->SendPacket(pkt.Serialize());
		}
	}
}

void ServerSocketApp::AddFilePromise(uint64_t promiseId, uint64_t userId)
{
	this->promisesToUsersMapping[promiseId] = userId;
}

uint64_t ServerSocketApp::GetUserForFilePromise(uint64_t promiseId)
{
	if (this->promisesToUsersMapping.find(promiseId) == this->promisesToUsersMapping.end())
	{
		return INVALID_USER_ID;
	}

	return this->promisesToUsersMapping[promiseId];
}

void ServerSocketApp::Run()
{
	LogInfo("Server is accepting connections");

	for (;;)
	{
		this->AcceptIncomingConnections();
		this->UpdateConnections();

		if (GetTickCount64() - lastSeenUpdatedTick > 1000)
		{
			this->UpdateLastSeenTimes();
			this->UpdateReadReceipts();
			this->UpdateParticipantLists();

			lastSeenUpdatedTick = GetTickCount64();
		}

		Sleep(15);
	}
}

void ServerSocketApp::Shutdown()
{
	if (this->serverSocket != INVALID_SOCKET)
	{
		closesocket(this->serverSocket);
	}
}
