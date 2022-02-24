#pragma once

#include <vector>
#include <memory>
#include <string>

#include "../Server/DatabaseInterface.h"

#define INVALID_USER_ID ((uint64_t)-1)
#define INVALID_CHAT_ID ((uint64_t)-1)
#define CURRENTLY_ONLINE ((uint64_t)-1)

class NetPacket;

enum class LoginResult : uint8_t {
	Success = 0,
	UsernameWrongLength = 1,
	Failed = 2
};

enum class ChatCreateResult : uint8_t {
	Success = 0,
	UserNotFound = 1,
	Failed = 2
};

// List of all packet headers (first byte of the packet is the header, identifying the purpose of the packet)
enum class PacketHeader : uint8_t {
	C2S_Login = 0,
	S2C_LoginAck = 1,

	C2S_CreateChat = 2,
	S2C_NewChat = 3,

	C2S_ResolveUsername = 4,
	S2C_ResolveUsernameAns = 5,

	C2S_OpenChat = 6,
	S2C_OpenChatAns = 7,

	C2S_SendMessage = 8,
	S2C_NewMessage = 9,

	S2C_ChatReadReceipt = 10,
	S2C_ReplaceChatList = 11,
	S2C_ReplaceParticipantList = 12,

	C2S_AddRemoveUser = 13,
	C2S_RenameChat = 14,

	S2C_MessageBox = 15,

	C2S_FilePromise = 16,
	C2S_RequestFile = 17,
	S2C_StartTransmission = 18,

	C2S_SendFileChunk = 19,
	S2C_ReceiveFileChunk = 20,
};

struct ChatRoomInfo {
	std::string name;
	uint64_t id;
};

class PKT_C2S_Login {
public:
	std::string username;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_C2S_Login> Deserialize(NetPacket* packetData);
};

class PKT_S2C_LoginAck {
public:
	LoginResult result;
	uint64_t userId;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_LoginAck> Deserialize(NetPacket* packetData);
};

class PKT_C2S_CreateChat {
public:
	bool isGroupChat;
	std::vector<uint64_t> userIDs;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_C2S_CreateChat> Deserialize(NetPacket* packetData);
};

class PKT_S2C_NewChat {
public:
	bool flashWindow;
	std::string chatName;
	uint64_t chatID;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_NewChat> Deserialize(NetPacket* packetData);
};

class PKT_C2S_ResolveUsername {
public:
	bool resolveUsername; // if true, this->username is used; otherwise, this->userId is used
	std::string username;
	uint64_t userId;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_C2S_ResolveUsername> Deserialize(NetPacket* packetData);
};

class PKT_S2C_ResolveUsernameAns {
public:
	std::string username;
	uint64_t userId; // INVALID_USER_ID if not found

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_ResolveUsernameAns> Deserialize(NetPacket* packetData);
};

class PKT_C2S_OpenChat {
public:
	uint64_t chatId;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_C2S_OpenChat> Deserialize(NetPacket* packetData);
};

class PKT_S2C_OpenChatAns {
public:
	std::vector<ChatMessage> messages;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_OpenChatAns> Deserialize(NetPacket* packetData);
};

class PKT_C2S_SendMessage {
public:
	std::string message;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_C2S_SendMessage> Deserialize(NetPacket* packetData);
};

class PKT_S2C_NewMessage {
public:
	uint64_t chatId;
	ChatMessage message;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_NewMessage> Deserialize(NetPacket* packetData);
};

class PKT_S2C_ChatReadReceipt {
public:
	uint64_t chatId;
	uint64_t userId;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_ChatReadReceipt> Deserialize(NetPacket* packetData);
};

class PKT_S2C_ReplaceChatList {
public:
	std::vector<DatabaseChatRoomInfoLite> rooms;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_ReplaceChatList> Deserialize(NetPacket* packetData);
};

class PKT_S2C_ReplaceParticipantList {
public:
	std::vector<DatabaseUserInfoLite> users;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_ReplaceParticipantList> Deserialize(NetPacket* packetData);
};

class PKT_C2S_AddRemoveUser {
public:
	uint64_t userId;
	bool isRemoveAction;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_C2S_AddRemoveUser> Deserialize(NetPacket* packetData);
};

class PKT_S2C_MessageBox {
public:
	std::string message;
	bool isDisconnection;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_MessageBox> Deserialize(NetPacket* packetData);
};

class PKT_C2S_RenameChat {
public:
	std::string newName;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_C2S_RenameChat> Deserialize(NetPacket* packetData);
};

class PKT_C2S_FilePromise {
public:
	uint64_t promiseId;
	std::string fileName;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_C2S_FilePromise> Deserialize(NetPacket* packetData);
};

class PKT_C2S_RequestFile {
public:
	uint64_t promiseId;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_C2S_RequestFile> Deserialize(NetPacket* packetData);
};

class PKT_S2C_StartTransmission {
public:
	uint64_t promiseId;
	uint64_t targetUserId;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_StartTransmission> Deserialize(NetPacket* packetData);
};

class PKT_C2S_SendFileChunk {
public:
	std::vector<uint8_t> fileData;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_C2S_SendFileChunk> Deserialize(NetPacket* packetData);
};

class PKT_S2C_ReceiveFileChunk {
public:
	std::vector<uint8_t> fileData;

	std::unique_ptr<NetPacket> Serialize();
	static std::unique_ptr<PKT_S2C_ReceiveFileChunk> Deserialize(NetPacket* packetData);
};
