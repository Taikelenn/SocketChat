#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <string>
#include <vector>
#include <memory>

class NetPacket;
class PKT_C2S_Login;
class PKT_C2S_CreateChat;
class PKT_C2S_ResolveUsername;
class PKT_C2S_OpenChat;
class PKT_C2S_SendMessage;
class PKT_C2S_AddRemoveUser;
class PKT_C2S_RenameChat;
class PKT_C2S_FilePromise;
class PKT_C2S_RequestFile;
class PKT_C2S_SendFileChunk;

enum class ClientProcessingResult {
	Continue, // the connection will continue
	CloseConnection, // the client has somehow indicated that it wants to close the connection
	TerminateConnection // the client misbehaves, so we send a RST flag and drop the connection
};

class RemoteClient {
private:
	SOCKET s;
	sockaddr_in6 sockaddr_s;
	std::string ipAddress;

	std::string username;
	uint64_t userId;
	uint64_t openChatId;

	uint64_t fileNextRecipient;

	uint8_t currentPacketLengthBytes[4];
	int currentPacketLengthPosIndex;

	std::vector<uint8_t> currentPacketBytes;
	int currentPacketPosIndex;

	std::vector<uint8_t> outgoingDataBuffer;
	int outgoingDataPosIndex;

	bool disconnectionInProgress;

	ClientProcessingResult ReadData();
	ClientProcessingResult SendPendingData();
	ClientProcessingResult ProcessPacket(NetPacket* packet);

	ClientProcessingResult ProcessPacket_Login(PKT_C2S_Login* packet);
	ClientProcessingResult ProcessPacket_CreateChat(PKT_C2S_CreateChat* packet);
	ClientProcessingResult ProcessPacket_ResolveUsername(PKT_C2S_ResolveUsername* packet);
	ClientProcessingResult ProcessPacket_OpenChat(PKT_C2S_OpenChat* packet);
	ClientProcessingResult ProcessPacket_SendMessage(PKT_C2S_SendMessage* packet);
	ClientProcessingResult ProcessPacket_AddRemoveUser(PKT_C2S_AddRemoveUser* packet);
	ClientProcessingResult ProcessPacket_RenameChat(PKT_C2S_RenameChat* packet);
	ClientProcessingResult ProcessPacket_FilePromise(PKT_C2S_FilePromise* packet);
	ClientProcessingResult ProcessPacket_RequestFile(PKT_C2S_RequestFile* packet);
	ClientProcessingResult ProcessPacket_SendFileChunk(PKT_C2S_SendFileChunk* packet);

public:
	ClientProcessingResult Update();

	inline bool IsLoggedIn() { return !this->username.empty(); }
	inline std::string GetUsername() { return this->username; }
	inline uint64_t GetUserID() { return this->userId; }
	
	inline uint64_t GetActiveChatID() { return this->openChatId; }
	inline void SetActiveChatID(uint64_t chatId) { this->openChatId = chatId; }

	inline uint64_t GetNextFileReceipient() { return this->fileNextRecipient; }
	inline void SetNextFileReceipient(uint64_t recipientUserId) { this->fileNextRecipient = recipientUserId; }

	void SendChatList();
	void SendParticipantList();

	void SendPacket(std::unique_ptr<NetPacket> pkt);

	void ShowMessageBox(const std::string& message, bool shouldDisconnect);
	void ResetConnectionOnClose();

	RemoteClient(SOCKET s, sockaddr_in6* sockaddr_s);
	~RemoteClient();
};
