#pragma once

#include "../sqlite/sqlite3.h"

#include <cstdint>
#include <string>
#include <vector>

struct DatabaseUserInfo {
	uint64_t userId;
	uint64_t lastSeen;
	std::string username;
};

struct DatabaseUserInfoLite {
	uint64_t userId;
	uint64_t lastSeen;
	bool hasReadChat;
};

struct DatabaseUserInfoList {
	std::vector<DatabaseUserInfoLite> users;
};

struct DatabaseChatRoomInfo {
	uint64_t chatRoomId;
	uint64_t ownerUserId;
	std::vector<uint64_t> allParticipants;
	std::string chatName;
	bool isGroupChat;
};

struct DatabaseChatRoomInfoLite {
	uint64_t chatId;
	std::string chatName;
	bool isUnread;
};

struct DatabaseChatRoomList {
	std::vector<DatabaseChatRoomInfoLite> chats;
};

struct ChatMessage {
	uint64_t author;
	uint64_t sentTimestamp;
	uint64_t filePromiseId;
	std::string message;
};

struct DatabaseChatMessages {
	std::vector<ChatMessage> messages;
};

class DatabaseInterface {
private:
	sqlite3* dbHandle;

public:
	DatabaseInterface(const char* databasePath);
	~DatabaseInterface();

	bool GetUserById(uint64_t userId, DatabaseUserInfo* userInfo);
	bool GetUserByName(const std::string& username, DatabaseUserInfo* userInfo);
	void CreateUser(const std::string& username);
	void UpdateLastSeenTime(uint64_t userId);
	void GetChatsForUser(uint64_t userId, DatabaseChatRoomList* chatList);

	uint64_t CreateChat(uint64_t ownerUserId, const std::vector<uint64_t>& participants, bool isGroupChat);
	void AddChatParticipant(uint64_t chatId, uint64_t userId);
	void RemoveUserFromChat(uint64_t chatId, uint64_t userId);
	bool GetChatById(uint64_t chatId, DatabaseChatRoomInfo* chatInfo);
	void GetUsersInChat(uint64_t chatId, DatabaseUserInfoList* userList);
	void RenameChat(uint64_t chatId, const std::string& newName);

	bool GetChatMessages(uint64_t chatId, DatabaseChatMessages* chatMessages);
	void AddChatMessage(uint64_t chatId, uint64_t senderId, const std::string& message, uint64_t* msgTimestamp, uint64_t filePromiseId = 0);
	void AddFilePromiseMessage(uint64_t chatId, uint64_t senderId, uint64_t promiseId, const std::string& fileName, uint64_t* msgTimestamp);
	bool IsChatReadByUser(uint64_t chatId, uint64_t userId);
	bool SetChatReadByUser(uint64_t chatId, uint64_t userId);
};
