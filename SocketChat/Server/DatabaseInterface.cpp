#include "DatabaseInterface.h"
#include "../Logger.h"
#include "../Packets/Protocol.h"

#include <stdexcept>
#include <ctime>

// A helper function and a macro which asserts that an SQLite operation succeeds - if it fails, an exception is thrown.
static inline bool ThrowHelper(const std::string& msg)
{
	throw std::runtime_error(msg);
}
#define MUST_SUCCEED(x) (((x) == SQLITE_OK) || ThrowHelper("Statement [" _CRT_STRINGIZE(x) "] in " __FUNCTION__ " failed: " + std::string(sqlite3_errmsg(this->dbHandle))))

DatabaseInterface::DatabaseInterface(const char* databasePath)
{
	if (sqlite3_open(databasePath, &this->dbHandle) != SQLITE_OK)
	{
		throw std::runtime_error("Cannot open database");
	}

	MUST_SUCCEED(sqlite3_exec(this->dbHandle, "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, lastSeen TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)",
		nullptr, nullptr, nullptr));
	MUST_SUCCEED(sqlite3_exec(this->dbHandle, "CREATE TABLE IF NOT EXISTS chats(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL DEFAULT \"unnamed chat\", isGroupChat INTEGER NOT NULL,"
		"ownerUserId INTEGER NOT NULL)", nullptr, nullptr, nullptr));
	MUST_SUCCEED(sqlite3_exec(this->dbHandle, "CREATE TABLE IF NOT EXISTS users_in_chats(chatId INTEGER NOT NULL, userId INTEGER NOT NULL, hasRead INTEGER NOT NULL DEFAULT 0,"
		"PRIMARY KEY(chatId, userId))", nullptr, nullptr, nullptr));
	MUST_SUCCEED(sqlite3_exec(this->dbHandle, "CREATE TABLE IF NOT EXISTS messages(id INTEGER PRIMARY KEY AUTOINCREMENT, chatId INTEGER NOT NULL, senderId INTEGER NOT NULL, content TEXT NOT NULL,"
		"filePromiseId INTEGER DEFAULT NULL, sentTime TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP)", nullptr, nullptr, nullptr));
}

DatabaseInterface::~DatabaseInterface()
{
	sqlite3_close(this->dbHandle);
}

bool DatabaseInterface::GetUserById(uint64_t userId, DatabaseUserInfo* userInfo)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "SELECT name, lastSeen FROM users WHERE id = ?", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, userId));

	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		userInfo->userId = userId;
		userInfo->username = (char*)sqlite3_column_text(stmt, 0);
		userInfo->lastSeen = sqlite3_column_int64(stmt, 1);

		MUST_SUCCEED(sqlite3_finalize(stmt));
		return true;
	}

	MUST_SUCCEED(sqlite3_finalize(stmt));
	return false;
}

bool DatabaseInterface::GetUserByName(const std::string& username, DatabaseUserInfo* userInfo)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "SELECT id, lastSeen FROM users WHERE name = ?", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC));

	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		userInfo->userId = sqlite3_column_int64(stmt, 0);
		userInfo->lastSeen = sqlite3_column_int64(stmt, 1);
		userInfo->username = username;

		MUST_SUCCEED(sqlite3_finalize(stmt));
		return true;
	}

	MUST_SUCCEED(sqlite3_finalize(stmt));
	return false;
}

void DatabaseInterface::CreateUser(const std::string& username)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "INSERT INTO users (name) VALUES (?)", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC));
	sqlite3_step(stmt);
	MUST_SUCCEED(sqlite3_finalize(stmt));

	LogInfo("New user created: %s", username.c_str());
}

void DatabaseInterface::UpdateLastSeenTime(uint64_t userId)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "UPDATE users SET lastSeen = CURRENT_TIMESTAMP WHERE id = ?", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, userId));
	sqlite3_step(stmt);
	MUST_SUCCEED(sqlite3_finalize(stmt));
}

void DatabaseInterface::GetChatsForUser(uint64_t userId, DatabaseChatRoomList* chatList)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "SELECT chatId FROM users_in_chats WHERE userId = ? ORDER BY chatId DESC", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, userId));

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		DatabaseChatRoomInfoLite roomInfo;
		roomInfo.chatId = sqlite3_column_int64(stmt, 0);

		DatabaseChatRoomInfo fullRoomInfo;
		this->GetChatById(roomInfo.chatId, &fullRoomInfo);

		roomInfo.chatName = fullRoomInfo.chatName;
		roomInfo.isUnread = !IsChatReadByUser(roomInfo.chatId, userId);

		chatList->chats.push_back(roomInfo);
	}

	MUST_SUCCEED(sqlite3_finalize(stmt));
}

uint64_t DatabaseInterface::CreateChat(uint64_t ownerUserId, const std::vector<uint64_t>& participants, bool isGroupChat)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_exec(this->dbHandle, "BEGIN TRANSACTION", nullptr, nullptr, nullptr));

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "INSERT INTO chats (isGroupChat, ownerUserId) VALUES (?, ?)", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int(stmt, 1, isGroupChat ? 1 : 0));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 2, ownerUserId));
	sqlite3_step(stmt);
	MUST_SUCCEED(sqlite3_finalize(stmt));

	uint64_t chatId = sqlite3_last_insert_rowid(this->dbHandle);

	for (uint64_t userId : participants)
	{
		this->AddChatParticipant(chatId, userId);
	}

	this->AddChatMessage(chatId, INVALID_USER_ID, "Chatroom created", nullptr);

	MUST_SUCCEED(sqlite3_exec(this->dbHandle, "COMMIT", nullptr, nullptr, nullptr));

	return chatId;
}

void DatabaseInterface::AddChatParticipant(uint64_t chatId, uint64_t userId)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "INSERT INTO users_in_chats (chatId, userId) VALUES (?, ?)", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, chatId));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 2, userId));
	sqlite3_step(stmt);
	MUST_SUCCEED(sqlite3_finalize(stmt));
}

bool DatabaseInterface::GetChatById(uint64_t chatId, DatabaseChatRoomInfo* chatInfo)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "SELECT name, isGroupChat, ownerUserId FROM chats WHERE id = ?", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, chatId));

	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		chatInfo->allParticipants.clear();
		chatInfo->chatRoomId = chatId;
		chatInfo->chatName = (char*)sqlite3_column_text(stmt, 0);
		chatInfo->isGroupChat = sqlite3_column_int(stmt, 1) != 0;
		chatInfo->ownerUserId = sqlite3_column_int64(stmt, 2);

		MUST_SUCCEED(sqlite3_finalize(stmt));

		MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "SELECT userId FROM users_in_chats WHERE chatId = ?", -1, &stmt, nullptr));
		MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, chatId));

		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			chatInfo->allParticipants.push_back(sqlite3_column_int64(stmt, 0));
		}

		MUST_SUCCEED(sqlite3_finalize(stmt));
		return true;
	}

	MUST_SUCCEED(sqlite3_finalize(stmt));
	return false;
}

void DatabaseInterface::GetUsersInChat(uint64_t chatId, DatabaseUserInfoList* userList)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "SELECT userId, STRFTIME('%s', lastSeen) FROM users_in_chats INNER JOIN users ON userId = users.id "
		"WHERE chatId = ? ORDER BY name ASC", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, chatId));

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		DatabaseUserInfoLite userInfo;
		userInfo.userId = sqlite3_column_int64(stmt, 0);
		userInfo.lastSeen = sqlite3_column_int64(stmt, 1);
		userInfo.hasReadChat = IsChatReadByUser(chatId, userInfo.userId);

		userList->users.push_back(userInfo);
	}

	MUST_SUCCEED(sqlite3_finalize(stmt));
}

void DatabaseInterface::RenameChat(uint64_t chatId, const std::string& newName)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "UPDATE chats SET name = ? WHERE id = ?", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_text(stmt, 1, newName.c_str(), -1, SQLITE_STATIC));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 2, chatId));
	sqlite3_step(stmt);
	MUST_SUCCEED(sqlite3_finalize(stmt));
}

bool DatabaseInterface::GetChatMessages(uint64_t chatId, DatabaseChatMessages* chatMessages)
{
	sqlite3_stmt* stmt;

	chatMessages->messages.clear();

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "SELECT senderId, content, STRFTIME('%s', sentTime), filePromiseId FROM messages WHERE chatId = ? ORDER BY sentTime ASC", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, chatId));

	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		ChatMessage msg;
		msg.author = sqlite3_column_int64(stmt, 0);
		msg.message = (char*)sqlite3_column_text(stmt, 1);
		msg.sentTimestamp = sqlite3_column_int64(stmt, 2);
		msg.filePromiseId = sqlite3_column_int64(stmt, 3);

		chatMessages->messages.push_back(msg);
	}

	MUST_SUCCEED(sqlite3_finalize(stmt));
	return true;
}

void DatabaseInterface::AddChatMessage(uint64_t chatId, uint64_t senderId, const std::string& message, uint64_t* msgTimestamp, uint64_t filePromiseId)
{
	sqlite3_stmt* stmt;

	// Add the message.
	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "INSERT INTO messages (chatId, senderId, content, filePromiseId) VALUES (?, ?, ?, ?)", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, chatId));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 2, senderId));
	MUST_SUCCEED(sqlite3_bind_text(stmt, 3, message.c_str(), -1, SQLITE_STATIC));

	if (filePromiseId)
	{
		MUST_SUCCEED(sqlite3_bind_int64(stmt, 4, filePromiseId));
	}
	else
	{
		MUST_SUCCEED(sqlite3_bind_null(stmt, 4));
	}

	sqlite3_step(stmt);
	MUST_SUCCEED(sqlite3_finalize(stmt));

	// Update read receipts (if the message is not a system message, that is) for everybody other than the message reader
	if (senderId != INVALID_USER_ID)
	{
		MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "UPDATE users_in_chats SET hasRead = 0 WHERE chatId = ? AND userId != ?", -1, &stmt, nullptr));
		MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, chatId));
		MUST_SUCCEED(sqlite3_bind_int64(stmt, 2, senderId));
		sqlite3_step(stmt);
		MUST_SUCCEED(sqlite3_finalize(stmt));
	}

	// Fetch the timestamp of the newly added message (if applicable)
	if (msgTimestamp)
	{
		MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "SELECT STRFTIME('%s', sentTime) FROM messages WHERE id = ?", -1, &stmt, nullptr));
		MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, sqlite3_last_insert_rowid(this->dbHandle)));
		if (sqlite3_step(stmt) == SQLITE_ROW)
		{
			*msgTimestamp = sqlite3_column_int64(stmt, 0);
		}
		MUST_SUCCEED(sqlite3_finalize(stmt));
	}
}

void DatabaseInterface::AddFilePromiseMessage(uint64_t chatId, uint64_t senderId, uint64_t promiseId, const std::string& fileName, uint64_t* msgTimestamp)
{
	this->AddChatMessage(chatId, senderId, fileName, msgTimestamp, promiseId);
}

bool DatabaseInterface::IsChatReadByUser(uint64_t chatId, uint64_t userId)
{
	sqlite3_stmt* stmt;
	bool retval = false;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "SELECT hasRead FROM users_in_chats WHERE chatId = ? AND userId = ?", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, chatId));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 2, userId));
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		if (sqlite3_column_int64(stmt, 0) != 0)
		{
			retval = true;
		}
	}
	MUST_SUCCEED(sqlite3_finalize(stmt));

	return retval;
}

bool DatabaseInterface::SetChatReadByUser(uint64_t chatId, uint64_t userId)
{
	sqlite3_stmt* stmt;

	if (this->IsChatReadByUser(chatId, userId))
	{
		return false;
	}

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "UPDATE users_in_chats SET hasRead = 1 WHERE chatId = ? AND userId = ?", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, chatId));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 2, userId));
	sqlite3_step(stmt);
	MUST_SUCCEED(sqlite3_finalize(stmt));

	return true;
}

void DatabaseInterface::RemoveUserFromChat(uint64_t chatId, uint64_t userId)
{
	sqlite3_stmt* stmt;

	MUST_SUCCEED(sqlite3_prepare_v2(this->dbHandle, "DELETE FROM users_in_chats WHERE chatId = ? AND userId = ?", -1, &stmt, nullptr));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 1, chatId));
	MUST_SUCCEED(sqlite3_bind_int64(stmt, 2, userId));
	sqlite3_step(stmt);
	MUST_SUCCEED(sqlite3_finalize(stmt));
}
