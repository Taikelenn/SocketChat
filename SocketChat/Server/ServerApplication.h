#pragma once

#include <WinSock2.h>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include "../Application.h"

class RemoteClient;
class DatabaseInterface;

enum class LoginResult : uint8_t;
enum class ChatCreateResult : uint8_t;

class ServerSocketApp : public Application {
private:
	SOCKET serverSocket;
	std::unique_ptr<DatabaseInterface> dbConnection;
	std::vector<std::unique_ptr<RemoteClient>> connectedClients;

	std::unordered_map<uint64_t, uint64_t> promisesToUsersMapping;

	uint64_t lastSeenUpdatedTick;

	// When the server is in listening state, this function will accept all incoming connections and add them to a list.
	void AcceptIncomingConnections();
	
	// Updates existing connections (calls recv(), processes incoming packets etc.)
	void UpdateConnections();

	// Updates the "last seen" times of logged in users
	void UpdateLastSeenTimes();

	// Updates read receipts of logged in users
	void UpdateReadReceipts();

	// Sends current chat participant lists to logged in users
	void UpdateParticipantLists();

	// Sets a socket as non-blocking and disables Nagle's algorithm for reduced latency.
	void SetSocketNonBlocking(SOCKET s);

public:
	ServerSocketApp();
	virtual ~ServerSocketApp();

	RemoteClient* GetLoggedInClient(uint64_t userId);
	inline DatabaseInterface* GetDB() { return dbConnection.get(); }

	LoginResult CreateLoginSession(std::string username, uint64_t* userId);
	ChatCreateResult CreateChat(uint64_t ownerUserId, std::vector<uint64_t> participants, bool isGroupChat);
	void SetChatReadByUser(uint64_t chatId, uint64_t userId);

	void AddFilePromise(uint64_t promiseId, uint64_t userId);
	uint64_t GetUserForFilePromise(uint64_t promiseId);

	virtual void Run();
	virtual void Shutdown();
};

extern ServerSocketApp* sApp;
