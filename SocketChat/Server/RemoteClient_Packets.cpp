#include "RemoteClient.h"
#include "ServerApplication.h"
#include "DatabaseInterface.h"

#include "../Logger.h"
#include "../Packets/NetPacket.h"
#include "../Packets/Protocol.h"

#include <memory>

ClientProcessingResult RemoteClient::ProcessPacket_Login(PKT_C2S_Login* packet)
{
	// cannot log in twice
	if (this->IsLoggedIn())
	{
		return ClientProcessingResult::Continue;
	}

	LoginResult result = sApp->CreateLoginSession(packet->username, &this->userId);
	if (result == LoginResult::Success)
	{
		this->username = packet->username;
		LogInfo("%s logs in as \xb0\x0b%s\xb0\x0f (%I64u)", this->ipAddress.c_str(), this->username.c_str(), this->userId);
	}

	PKT_S2C_LoginAck loginAckPacket;
	loginAckPacket.result = result;
	loginAckPacket.userId = this->userId;

	this->SendPacket(loginAckPacket.Serialize());
	if (result == LoginResult::Success)
	{
		this->SendChatList();
		return ClientProcessingResult::Continue;
	}

	return ClientProcessingResult::CloseConnection;
}

ClientProcessingResult RemoteClient::ProcessPacket_CreateChat(PKT_C2S_CreateChat* packet)
{
	if (!this->IsLoggedIn())
	{
		return ClientProcessingResult::TerminateConnection;
	}

	ChatCreateResult result = sApp->CreateChat(this->userId, packet->userIDs, packet->isGroupChat);
	if (result == ChatCreateResult::Success)
	{
		LogInfo("New%schat created by \xb0\x0b%s\xb0\x0f", packet->isGroupChat ? " group " : " ", this->username.c_str());
	}

	return ClientProcessingResult::Continue;
}

ClientProcessingResult RemoteClient::ProcessPacket_ResolveUsername(PKT_C2S_ResolveUsername* packet)
{
	if (!this->IsLoggedIn())
	{
		return ClientProcessingResult::TerminateConnection;
	}

	PKT_S2C_ResolveUsernameAns response;
	DatabaseUserInfo userInfo;

	if (packet->resolveUsername)
	{
		response.username = packet->username;
		if (!sApp->GetDB()->GetUserByName(packet->username, &userInfo))
		{
			response.userId = INVALID_USER_ID;
		}
		else
		{
			response.userId = userInfo.userId;
		}
	}
	else
	{
		response.userId = packet->userId;
		if (!sApp->GetDB()->GetUserById(packet->userId, &userInfo))
		{
			response.username = "";
		}
		else
		{
			response.username = userInfo.username;
		}
	}

	LogInfo("Resolved %s <--> %I64u", response.username.c_str(), response.userId);

	this->SendPacket(response.Serialize());
	return ClientProcessingResult::Continue;
}

ClientProcessingResult RemoteClient::ProcessPacket_OpenChat(PKT_C2S_OpenChat* packet)
{
	if (!this->IsLoggedIn())
	{
		return ClientProcessingResult::TerminateConnection;
	}

	if (packet->chatId == INVALID_CHAT_ID)
	{
		this->SendChatList();
		return ClientProcessingResult::Continue;
	}

	LogInfo("\xb0\x0b%s\xb0\x0f is opening chat %u", this->username.c_str(), packet->chatId);

	DatabaseChatMessages chatMessages;
	sApp->GetDB()->GetChatMessages(packet->chatId, &chatMessages);

	this->SetActiveChatID(packet->chatId);

	PKT_S2C_OpenChatAns response;
	response.messages = chatMessages.messages;

	this->SendPacket(response.Serialize());

	sApp->SetChatReadByUser(packet->chatId, this->GetUserID());
	this->SendParticipantList();

	return ClientProcessingResult::Continue;
}

ClientProcessingResult RemoteClient::ProcessPacket_SendMessage(PKT_C2S_SendMessage* packet)
{
	if (!this->IsLoggedIn())
	{
		return ClientProcessingResult::TerminateConnection;
	}

	if (this->openChatId == INVALID_CHAT_ID)
	{
		return ClientProcessingResult::Continue;
	}

	LogInfo("\xb0\x0b%s\xb0\x0f sent a message in chat %u", this->username.c_str(), this->openChatId);

	uint64_t msgTimestamp;
	sApp->GetDB()->AddChatMessage(this->openChatId, this->userId, packet->message, &msgTimestamp);

	PKT_S2C_NewMessage response;
	response.chatId = this->openChatId;
	response.message.author = this->userId;
	response.message.message = packet->message;
	response.message.filePromiseId = 0;
	response.message.sentTimestamp = msgTimestamp;

	DatabaseChatRoomInfo chatInfo;
	sApp->GetDB()->GetChatById(this->openChatId, &chatInfo);

	for (uint64_t participantId : chatInfo.allParticipants)
	{
		RemoteClient* client = sApp->GetLoggedInClient(participantId);
		if (client)
		{
			client->SendPacket(response.Serialize());
		}
	}

	return ClientProcessingResult::Continue;
}

ClientProcessingResult RemoteClient::ProcessPacket_AddRemoveUser(PKT_C2S_AddRemoveUser* packet)
{
	if (!this->IsLoggedIn())
	{
		return ClientProcessingResult::TerminateConnection;
	}

	if (this->openChatId == INVALID_CHAT_ID)
	{
		return ClientProcessingResult::Continue;
	}

	LogInfo("\xb0\x0b%s\xb0\x0f %s user %u", this->username.c_str(), packet->isRemoveAction ? "removes" : "adds", packet->userId);

	DatabaseChatRoomInfo chatInfo;
	sApp->GetDB()->GetChatById(this->openChatId, &chatInfo);

	if (packet->isRemoveAction)
	{
		sApp->GetDB()->RemoveUserFromChat(this->openChatId, packet->userId);
		
		RemoteClient* removedClient = sApp->GetLoggedInClient(packet->userId);
		if (removedClient)
		{
			removedClient->SendChatList();
		}
	}
	else
	{
		if (std::find(chatInfo.allParticipants.begin(), chatInfo.allParticipants.end(), packet->userId) != chatInfo.allParticipants.end())
		{
			this->ShowMessageBox("The user you are trying to add is already in this group.", false);
			return ClientProcessingResult::Continue;
		}

		sApp->GetDB()->AddChatParticipant(this->openChatId, packet->userId);

		RemoteClient* addedClient = sApp->GetLoggedInClient(packet->userId);
		if (addedClient)
		{
			PKT_S2C_NewChat newChatPkt;
			newChatPkt.chatID = chatInfo.chatRoomId;
			newChatPkt.chatName = chatInfo.chatName;
			newChatPkt.flashWindow = true;

			addedClient->SendPacket(newChatPkt.Serialize());
		}
	}

	for (uint64_t participantId : chatInfo.allParticipants)
	{
		RemoteClient* client = sApp->GetLoggedInClient(participantId);
		if (client && client->GetActiveChatID() == chatInfo.chatRoomId)
		{
			client->SendParticipantList();
		}
	}

	return ClientProcessingResult::Continue;
}

ClientProcessingResult RemoteClient::ProcessPacket_RenameChat(PKT_C2S_RenameChat* packet)
{
	if (!this->IsLoggedIn())
	{
		return ClientProcessingResult::TerminateConnection;
	}

	if (this->openChatId == INVALID_CHAT_ID)
	{
		return ClientProcessingResult::Continue;
	}

	sApp->GetDB()->RenameChat(this->openChatId, packet->newName);

	DatabaseChatRoomInfo chatInfo;
	sApp->GetDB()->GetChatById(this->openChatId, &chatInfo);

	for (uint64_t participantId : chatInfo.allParticipants)
	{
		RemoteClient* client = sApp->GetLoggedInClient(participantId);
		if (client)
		{
			client->SendChatList();
		}
	}

	return ClientProcessingResult::Continue;
}

ClientProcessingResult RemoteClient::ProcessPacket_FilePromise(PKT_C2S_FilePromise* packet)
{
	if (!this->IsLoggedIn())
	{
		return ClientProcessingResult::TerminateConnection;
	}

	if (this->openChatId == INVALID_CHAT_ID)
	{
		return ClientProcessingResult::Continue;
	}

	LogInfo("\xb0\x0b%s\xb0\x0f sent a file promise in chat %u", this->username.c_str(), this->openChatId);

	sApp->AddFilePromise(packet->promiseId, this->userId);

	uint64_t msgTimestamp;
	sApp->GetDB()->AddFilePromiseMessage(this->openChatId, this->userId, packet->promiseId, packet->fileName, &msgTimestamp);

	PKT_S2C_NewMessage response;
	response.chatId = this->openChatId;
	response.message.author = this->userId;
	response.message.message = packet->fileName;
	response.message.filePromiseId = packet->promiseId;
	response.message.sentTimestamp = msgTimestamp;

	DatabaseChatRoomInfo chatInfo;
	sApp->GetDB()->GetChatById(this->openChatId, &chatInfo);

	for (uint64_t participantId : chatInfo.allParticipants)
	{
		RemoteClient* client = sApp->GetLoggedInClient(participantId);
		if (client)
		{
			client->SendPacket(response.Serialize());
		}
	}

	return ClientProcessingResult::Continue;
}

ClientProcessingResult RemoteClient::ProcessPacket_RequestFile(PKT_C2S_RequestFile* packet)
{
	if (!this->IsLoggedIn())
	{
		return ClientProcessingResult::TerminateConnection;
	}

	LogInfo("\xb0\x0b%s\xb0\x0f requested file promise %I64u", this->username.c_str(), packet->promiseId);

	uint64_t fileOwnerUserId = sApp->GetUserForFilePromise(packet->promiseId);
	if (fileOwnerUserId == INVALID_USER_ID)
	{
		this->ShowMessageBox("This file is no longer available.", false);
		return ClientProcessingResult::Continue;
	}

	RemoteClient* fileOwner = sApp->GetLoggedInClient(fileOwnerUserId);
	if (!fileOwner)
	{
		this->ShowMessageBox("The sender of this file is not online\nThe file cannot be sent.", false);
		return ClientProcessingResult::Continue;
	}

	PKT_S2C_StartTransmission pkt;
	pkt.promiseId = packet->promiseId;
	pkt.targetUserId = this->GetUserID();

	fileOwner->SetNextFileReceipient(this->GetUserID());
	fileOwner->SendPacket(pkt.Serialize());

	LogInfo("User %I64u will send the file to %I64u", fileOwner->GetUserID(), this->GetUserID());

	return ClientProcessingResult::Continue;
}

ClientProcessingResult RemoteClient::ProcessPacket_SendFileChunk(PKT_C2S_SendFileChunk* packet)
{
	if (!this->IsLoggedIn())
	{
		return ClientProcessingResult::TerminateConnection;
	}

	RemoteClient* fileRecipient = sApp->GetLoggedInClient(this->fileNextRecipient);
	if (fileRecipient)
	{
		PKT_S2C_ReceiveFileChunk pkt;
		pkt.fileData = packet->fileData;

		fileRecipient->SendPacket(pkt.Serialize());
	}

	this->SetNextFileReceipient(INVALID_USER_ID);
	return ClientProcessingResult::Continue;
}

ClientProcessingResult RemoteClient::ProcessPacket(NetPacket* packet)
{
	PacketHeader header = packet->ReadField<PacketHeader>();

	switch (header)
	{
	case PacketHeader::C2S_Login:
	{
		std::unique_ptr<PKT_C2S_Login> pkt = PKT_C2S_Login::Deserialize(packet);
		return this->ProcessPacket_Login(pkt.get());
	}
	case PacketHeader::C2S_ResolveUsername:
	{
		std::unique_ptr<PKT_C2S_ResolveUsername> pkt = PKT_C2S_ResolveUsername::Deserialize(packet);
		return this->ProcessPacket_ResolveUsername(pkt.get());
	}
	case PacketHeader::C2S_CreateChat:
	{
		std::unique_ptr<PKT_C2S_CreateChat> pkt = PKT_C2S_CreateChat::Deserialize(packet);
		return this->ProcessPacket_CreateChat(pkt.get());
	}
	case PacketHeader::C2S_OpenChat:
	{
		std::unique_ptr<PKT_C2S_OpenChat> pkt = PKT_C2S_OpenChat::Deserialize(packet);
		return this->ProcessPacket_OpenChat(pkt.get());
	}
	case PacketHeader::C2S_SendMessage:
	{
		std::unique_ptr<PKT_C2S_SendMessage> pkt = PKT_C2S_SendMessage::Deserialize(packet);
		return this->ProcessPacket_SendMessage(pkt.get());
	}
	case PacketHeader::C2S_AddRemoveUser:
	{
		std::unique_ptr<PKT_C2S_AddRemoveUser> pkt = PKT_C2S_AddRemoveUser::Deserialize(packet);
		return this->ProcessPacket_AddRemoveUser(pkt.get());
	}
	case PacketHeader::C2S_RenameChat:
	{
		std::unique_ptr<PKT_C2S_RenameChat> pkt = PKT_C2S_RenameChat::Deserialize(packet);
		return this->ProcessPacket_RenameChat(pkt.get());
	}
	case PacketHeader::C2S_FilePromise:
	{
		std::unique_ptr<PKT_C2S_FilePromise> pkt = PKT_C2S_FilePromise::Deserialize(packet);
		return this->ProcessPacket_FilePromise(pkt.get());
	}
	case PacketHeader::C2S_RequestFile:
	{
		std::unique_ptr<PKT_C2S_RequestFile> pkt = PKT_C2S_RequestFile::Deserialize(packet);
		return this->ProcessPacket_RequestFile(pkt.get());
	}
	case PacketHeader::C2S_SendFileChunk:
	{
		std::unique_ptr<PKT_C2S_SendFileChunk> pkt = PKT_C2S_SendFileChunk::Deserialize(packet);
		return this->ProcessPacket_SendFileChunk(pkt.get());
	}
	default:
	{
		LogWarning("Received a packet with unknown header %u", header);
		return ClientProcessingResult::TerminateConnection;
	}
	}
}
