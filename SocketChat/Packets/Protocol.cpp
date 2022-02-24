#include "Protocol.h"
#include "NetPacket.h"

// ================================ PKT_C2S_Login ================================

std::unique_ptr<NetPacket> PKT_C2S_Login::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::C2S_Login);
	pkt->WriteString(this->username);

	return pkt;
}

std::unique_ptr<PKT_C2S_Login> PKT_C2S_Login::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_C2S_Login> pkt = std::make_unique<PKT_C2S_Login>();
	pkt->username = packetData->ReadString();

	return pkt;
}

// =============================== PKT_S2C_LoginAck ===============================

std::unique_ptr<NetPacket> PKT_S2C_LoginAck::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_LoginAck);
	pkt->WriteField<LoginResult>(this->result);
	if (this->result == LoginResult::Success)
	{
		pkt->WriteField<uint64_t>(this->userId);
	}

	return pkt;
}

std::unique_ptr<PKT_S2C_LoginAck> PKT_S2C_LoginAck::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_LoginAck> pkt = std::make_unique<PKT_S2C_LoginAck>();
	pkt->result = packetData->ReadField<LoginResult>();
	if (pkt->result == LoginResult::Success)
	{
		pkt->userId = packetData->ReadField<uint64_t>();
	}

	return pkt;
}

// ============================== PKT_C2S_CreateChat ==============================

std::unique_ptr<NetPacket> PKT_C2S_CreateChat::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::C2S_CreateChat);
	pkt->WriteField<bool>(this->isGroupChat);
	if (this->isGroupChat)
	{
		pkt->WriteField<size_t>(this->userIDs.size());
		for (uint64_t userID : this->userIDs)
		{
			pkt->WriteField<uint64_t>(userID);
		}
	}
	else
	{
		pkt->WriteField<uint64_t>(userIDs[0]); // only one user ID when the chat is not a group chat
	}

	return pkt;
}

std::unique_ptr<PKT_C2S_CreateChat> PKT_C2S_CreateChat::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_C2S_CreateChat> pkt = std::make_unique<PKT_C2S_CreateChat>();
	pkt->isGroupChat = packetData->ReadField<bool>();
	if (pkt->isGroupChat)
	{
		pkt->userIDs.resize(packetData->ReadField<size_t>());
		for (size_t i = 0; i < pkt->userIDs.size(); ++i)
		{
			pkt->userIDs[i] = packetData->ReadField<uint64_t>();
		}
	}
	else
	{
		pkt->userIDs.resize(1);
		pkt->userIDs[0] = packetData->ReadField<uint64_t>();
	}

	return pkt;
}

// =============================== PKT_S2C_NewChat ===============================

std::unique_ptr<NetPacket> PKT_S2C_NewChat::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_NewChat);
	pkt->WriteField<bool>(this->flashWindow);
	pkt->WriteField<uint64_t>(this->chatID);
	pkt->WriteString(this->chatName);

	return pkt;
}

std::unique_ptr<PKT_S2C_NewChat> PKT_S2C_NewChat::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_NewChat> pkt = std::make_unique<PKT_S2C_NewChat>();
	pkt->flashWindow = packetData->ReadField<bool>();
	pkt->chatID = packetData->ReadField<uint64_t>();
	pkt->chatName = packetData->ReadString();

	return pkt;
}

// =========================== PKT_C2S_ResolveUsername ===========================

std::unique_ptr<NetPacket> PKT_C2S_ResolveUsername::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::C2S_ResolveUsername);
	pkt->WriteField<bool>(this->resolveUsername);
	if (this->resolveUsername)
	{
		pkt->WriteString(this->username);
	}
	else
	{
		pkt->WriteField<uint64_t>(this->userId);
	}

	return pkt;
}

std::unique_ptr<PKT_C2S_ResolveUsername> PKT_C2S_ResolveUsername::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_C2S_ResolveUsername> pkt = std::make_unique<PKT_C2S_ResolveUsername>();
	pkt->resolveUsername = packetData->ReadField<bool>();
	if (pkt->resolveUsername)
	{
		pkt->username = packetData->ReadString();
	}
	else
	{
		pkt->userId = packetData->ReadField<uint64_t>();
	}

	return pkt;
}

// ========================= PKT_S2C_ResolveUsernameAns ==========================

std::unique_ptr<NetPacket> PKT_S2C_ResolveUsernameAns::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_ResolveUsernameAns);
	pkt->WriteString(this->username);
	pkt->WriteField<uint64_t>(this->userId);

	return pkt;
}

std::unique_ptr<PKT_S2C_ResolveUsernameAns> PKT_S2C_ResolveUsernameAns::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_ResolveUsernameAns> pkt = std::make_unique<PKT_S2C_ResolveUsernameAns>();
	pkt->username = packetData->ReadString();
	pkt->userId = packetData->ReadField<uint64_t>();

	return pkt;
}

// ============================== PKT_C2S_OpenChat ===============================

std::unique_ptr<NetPacket> PKT_C2S_OpenChat::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::C2S_OpenChat);
	pkt->WriteField<uint64_t>(this->chatId);

	return pkt;
}

std::unique_ptr<PKT_C2S_OpenChat> PKT_C2S_OpenChat::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_C2S_OpenChat> pkt = std::make_unique<PKT_C2S_OpenChat>();
	pkt->chatId = packetData->ReadField<uint64_t>();

	return pkt;
}

// ============================= PKT_S2C_OpenChatAns ==============================

std::unique_ptr<NetPacket> PKT_S2C_OpenChatAns::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_OpenChatAns);
	pkt->WriteField<size_t>(this->messages.size());
	for (size_t i = 0; i < this->messages.size(); ++i)
	{
		pkt->WriteField<uint64_t>(this->messages[i].author);
		pkt->WriteField<uint64_t>(this->messages[i].sentTimestamp);
		pkt->WriteField<uint64_t>(this->messages[i].filePromiseId);
		pkt->WriteString(this->messages[i].message);
	}

	return pkt;
}

std::unique_ptr<PKT_S2C_OpenChatAns> PKT_S2C_OpenChatAns::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_OpenChatAns> pkt = std::make_unique<PKT_S2C_OpenChatAns>();
	pkt->messages.resize(packetData->ReadField<size_t>());
	for (size_t i = 0; i < pkt->messages.size(); ++i)
	{
		pkt->messages[i].author = packetData->ReadField<uint64_t>();
		pkt->messages[i].sentTimestamp = packetData->ReadField<uint64_t>();
		pkt->messages[i].filePromiseId = packetData->ReadField<uint64_t>();
		pkt->messages[i].message = packetData->ReadString();
	}

	return pkt;
}

// ============================= PKT_C2S_SendMessage ==============================

std::unique_ptr<NetPacket> PKT_C2S_SendMessage::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::C2S_SendMessage);
	pkt->WriteString(this->message);

	return pkt;
}

std::unique_ptr<PKT_C2S_SendMessage> PKT_C2S_SendMessage::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_C2S_SendMessage> pkt = std::make_unique<PKT_C2S_SendMessage>();
	pkt->message = packetData->ReadString();

	return pkt;
}

// ============================= PKT_S2C_NewMessage ===============================

std::unique_ptr<NetPacket> PKT_S2C_NewMessage::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_NewMessage);
	pkt->WriteField<uint64_t>(this->chatId);
	pkt->WriteField<uint64_t>(this->message.author);
	pkt->WriteField<uint64_t>(this->message.sentTimestamp);
	pkt->WriteField<uint64_t>(this->message.filePromiseId);
	pkt->WriteString(this->message.message);

	return pkt;
}

std::unique_ptr<PKT_S2C_NewMessage> PKT_S2C_NewMessage::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_NewMessage> pkt = std::make_unique<PKT_S2C_NewMessage>();
	pkt->chatId = packetData->ReadField<uint64_t>();
	pkt->message.author = packetData->ReadField<uint64_t>();
	pkt->message.sentTimestamp = packetData->ReadField<uint64_t>();
	pkt->message.filePromiseId = packetData->ReadField<uint64_t>();
	pkt->message.message = packetData->ReadString();

	return pkt;
}

// =========================== PKT_S2C_ChatReadReceipt ============================

std::unique_ptr<NetPacket> PKT_S2C_ChatReadReceipt::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_ChatReadReceipt);
	pkt->WriteField<uint64_t>(this->chatId);
	pkt->WriteField<uint64_t>(this->userId);

	return pkt;
}

std::unique_ptr<PKT_S2C_ChatReadReceipt> PKT_S2C_ChatReadReceipt::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_ChatReadReceipt> pkt = std::make_unique<PKT_S2C_ChatReadReceipt>();
	pkt->chatId = packetData->ReadField<uint64_t>();
	pkt->userId = packetData->ReadField<uint64_t>();

	return pkt;
}

// =========================== PKT_S2C_ReplaceChatList ============================

std::unique_ptr<NetPacket> PKT_S2C_ReplaceChatList::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_ReplaceChatList);
	pkt->WriteField<size_t>(this->rooms.size());
	for (const auto& room : this->rooms)
	{
		pkt->WriteString(room.chatName);
		pkt->WriteField<uint64_t>(room.chatId);
		pkt->WriteField<bool>(room.isUnread);
	}

	return pkt;
}

std::unique_ptr<PKT_S2C_ReplaceChatList> PKT_S2C_ReplaceChatList::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_ReplaceChatList> pkt = std::make_unique<PKT_S2C_ReplaceChatList>();
	pkt->rooms.resize(packetData->ReadField<size_t>());
	for (size_t i = 0; i < pkt->rooms.size(); ++i)
	{
		pkt->rooms[i].chatName = packetData->ReadString();
		pkt->rooms[i].chatId = packetData->ReadField<uint64_t>();
		pkt->rooms[i].isUnread = packetData->ReadField<bool>();
	}

	return pkt;
}

// ======================== PKT_S2C_ReplaceParticipantList =========================

std::unique_ptr<NetPacket> PKT_S2C_ReplaceParticipantList::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_ReplaceParticipantList);
	pkt->WriteField<size_t>(this->users.size());
	for (const auto& user : this->users)
	{
		pkt->WriteField<uint64_t>(user.userId);
		pkt->WriteField<uint64_t>(user.lastSeen);
		pkt->WriteField<bool>(user.hasReadChat);
	}

	return pkt;
}

std::unique_ptr<PKT_S2C_ReplaceParticipantList> PKT_S2C_ReplaceParticipantList::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_ReplaceParticipantList> pkt = std::make_unique<PKT_S2C_ReplaceParticipantList>();
	pkt->users.resize(packetData->ReadField<size_t>());
	for (size_t i = 0; i < pkt->users.size(); ++i)
	{
		pkt->users[i].userId = packetData->ReadField<uint64_t>();
		pkt->users[i].lastSeen = packetData->ReadField<uint64_t>();
		pkt->users[i].hasReadChat = packetData->ReadField<bool>();
	}

	return pkt;
}

// ============================= PKT_C2S_AddRemoveUser ==============================

std::unique_ptr<NetPacket> PKT_C2S_AddRemoveUser::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::C2S_AddRemoveUser);
	pkt->WriteField<uint64_t>(this->userId);
	pkt->WriteField<bool>(this->isRemoveAction);

	return pkt;
}

std::unique_ptr<PKT_C2S_AddRemoveUser> PKT_C2S_AddRemoveUser::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_C2S_AddRemoveUser> pkt = std::make_unique<PKT_C2S_AddRemoveUser>();
	pkt->userId = packetData->ReadField<uint64_t>();
	pkt->isRemoveAction = packetData->ReadField<bool>();

	return pkt;
}

// ============================== PKT_S2C_MessageBox ================================

std::unique_ptr<NetPacket> PKT_S2C_MessageBox::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_MessageBox);
	pkt->WriteString(this->message);
	pkt->WriteField<bool>(this->isDisconnection);

	return pkt;
}

std::unique_ptr<PKT_S2C_MessageBox> PKT_S2C_MessageBox::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_MessageBox> pkt = std::make_unique<PKT_S2C_MessageBox>();
	pkt->message = packetData->ReadString();
	pkt->isDisconnection = packetData->ReadField<bool>();

	return pkt;
}

// ============================== PKT_C2S_RenameChat ================================

std::unique_ptr<NetPacket> PKT_C2S_RenameChat::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::C2S_RenameChat);
	pkt->WriteString(this->newName);

	return pkt;
}

std::unique_ptr<PKT_C2S_RenameChat> PKT_C2S_RenameChat::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_C2S_RenameChat> pkt = std::make_unique<PKT_C2S_RenameChat>();
	pkt->newName = packetData->ReadString();

	return pkt;
}

// ============================= PKT_C2S_FilePromise ================================

std::unique_ptr<NetPacket> PKT_C2S_FilePromise::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::C2S_FilePromise);
	pkt->WriteField<uint64_t>(this->promiseId);
	pkt->WriteString(this->fileName);

	return pkt;
}

std::unique_ptr<PKT_C2S_FilePromise> PKT_C2S_FilePromise::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_C2S_FilePromise> pkt = std::make_unique<PKT_C2S_FilePromise>();
	pkt->promiseId = packetData->ReadField<uint64_t>();
	pkt->fileName = packetData->ReadString();

	return pkt;
}

// ============================= PKT_C2S_RequestFile ================================

std::unique_ptr<NetPacket> PKT_C2S_RequestFile::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::C2S_RequestFile);
	pkt->WriteField<uint64_t>(this->promiseId);

	return pkt;
}

std::unique_ptr<PKT_C2S_RequestFile> PKT_C2S_RequestFile::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_C2S_RequestFile> pkt = std::make_unique<PKT_C2S_RequestFile>();
	pkt->promiseId = packetData->ReadField<uint64_t>();

	return pkt;
}

// ========================== PKT_S2C_StartTransmission =============================

std::unique_ptr<NetPacket> PKT_S2C_StartTransmission::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_StartTransmission);
	pkt->WriteField<uint64_t>(this->promiseId);
	pkt->WriteField<uint64_t>(this->targetUserId);

	return pkt;
}

std::unique_ptr<PKT_S2C_StartTransmission> PKT_S2C_StartTransmission::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_StartTransmission> pkt = std::make_unique<PKT_S2C_StartTransmission>();
	pkt->promiseId = packetData->ReadField<uint64_t>();
	pkt->targetUserId = packetData->ReadField<uint64_t>();

	return pkt;
}

// ============================ PKT_C2S_SendFileChunk ===============================

std::unique_ptr<NetPacket> PKT_C2S_SendFileChunk::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::C2S_SendFileChunk);
	pkt->WriteField<size_t>(this->fileData.size());
	pkt->WriteByteArray(this->fileData.data(), this->fileData.size());

	return pkt;
}

std::unique_ptr<PKT_C2S_SendFileChunk> PKT_C2S_SendFileChunk::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_C2S_SendFileChunk> pkt = std::make_unique<PKT_C2S_SendFileChunk>();
	pkt->fileData.resize(packetData->ReadField<size_t>());
	packetData->ReadByteArray(pkt->fileData.data(), pkt->fileData.size());

	return pkt;
}

// ========================== PKT_S2C_ReceiveFileChunk ==============================

std::unique_ptr<NetPacket> PKT_S2C_ReceiveFileChunk::Serialize()
{
	std::unique_ptr<NetPacket> pkt = std::make_unique<NetPacket>();

	pkt->WriteField<PacketHeader>(PacketHeader::S2C_ReceiveFileChunk);
	pkt->WriteField<size_t>(this->fileData.size());
	pkt->WriteByteArray(this->fileData.data(), this->fileData.size());

	return pkt;
}

std::unique_ptr<PKT_S2C_ReceiveFileChunk> PKT_S2C_ReceiveFileChunk::Deserialize(NetPacket* packetData)
{
	packetData->SetReadIterator(1); // skip the first byte (header)

	std::unique_ptr<PKT_S2C_ReceiveFileChunk> pkt = std::make_unique<PKT_S2C_ReceiveFileChunk>();
	pkt->fileData.resize(packetData->ReadField<size_t>());
	packetData->ReadByteArray(pkt->fileData.data(), pkt->fileData.size());

	return pkt;
}
