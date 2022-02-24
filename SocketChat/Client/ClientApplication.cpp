#include "ClientApplication.h"

#include "../Logger.h"
#include "../Packets/NetPacket.h"
#include "../Packets/Protocol.h"

#include <cstdio>
#include <WS2tcpip.h>
#include <stdexcept>
#include <random>

ClientSocketApp* cApp;

static volatile bool NetworkThreadExitFlag = false;

DWORD CALLBACK NetworkThread(LPVOID lpParameter)
{
	UNREFERENCED_PARAMETER(lpParameter);

	SOCKET s = (SOCKET)lpParameter;

	for (;;)
	{
		int recvResult = 0;

		uint32_t packetLength;
		if ((recvResult = recv(s, (char*)&packetLength, sizeof(packetLength), MSG_WAITALL)) != sizeof(packetLength))
		{
			if (NetworkThreadExitFlag)
			{
				return 0;
			}

			cApp->RaiseNetworkError(recvResult == SOCKET_ERROR ? WSAGetLastError() : 0);
			return 0;
		}

		std::vector<uint8_t> packetData;
		packetData.resize(packetLength);
		if ((recvResult = recv(s, (char*)packetData.data(), packetLength, MSG_WAITALL)) != (int)packetLength)
		{
			if (NetworkThreadExitFlag)
			{
				return 0;
			}

			cApp->RaiseNetworkError(recvResult == SOCKET_ERROR ? WSAGetLastError() : 0);
			return 0;
		}

		cApp->NotifyNetworkEvent(std::make_unique<NetPacket>(packetData.data(), packetLength));
	}
}

void ClientSocketApp::RaiseNetworkError(DWORD winsockErrorCode)
{
	shutdown(this->s, SD_BOTH);
	closesocket(this->s);

	LogWarning("Network error has been raised: error code %u", winsockErrorCode);

	SendMessageW(ui.g_Window, WM_NETERR_TERMINATE, winsockErrorCode, 0);
}

void ClientSocketApp::SendNetEvent(std::unique_ptr<NetPacket> packet)
{
	EnterCriticalSection(&sendEventCS);

	uint32_t packetSize = (uint32_t)packet->GetLength();

	// send() here is blocking, so it waits until everything is sent
	send(this->s, (const char*)&packetSize, sizeof(packetSize), 0);
	send(this->s, (const char*)packet->GetData(), packetSize, 0);

	LeaveCriticalSection(&sendEventCS);
}

uint64_t ClientSocketApp::UsernameToID(const std::string& username)
{
	auto mapResult = this->usernameToIdMapping.find(username);
	if (mapResult != this->usernameToIdMapping.end())
	{
		return mapResult->second;
	}

	PKT_C2S_ResolveUsername resolvePkt;
	resolvePkt.resolveUsername = true;
	resolvePkt.username = username;

	this->SendNetEvent(resolvePkt.Serialize());

	if (WaitForSingleObject(this->hResolveEvent, 10000) == WAIT_OBJECT_0)
	{
		if (this->usernameToIdMapping[username] == INVALID_USER_ID)
		{
			this->usernameToIdMapping.erase(username);
			return INVALID_USER_ID;
		}

		return this->usernameToIdMapping[username];
	}

	if (IsDebuggerPresent())
	{
		DebugBreak();
	}
	throw std::runtime_error("Server timed out while waiting for user ID");
}

std::string ClientSocketApp::UserIDToName(uint64_t userID)
{
	auto mapResult = this->userIdToNameMapping.find(userID);
	if (mapResult != this->userIdToNameMapping.end())
	{
		return mapResult->second;
	}

	PKT_C2S_ResolveUsername resolvePkt;
	resolvePkt.resolveUsername = false;
	resolvePkt.userId = userID;

	this->SendNetEvent(resolvePkt.Serialize());

	if (WaitForSingleObject(this->hResolveEvent, 10000) == WAIT_OBJECT_0)
	{
		if (this->userIdToNameMapping[userID].empty())
		{
			this->userIdToNameMapping.erase(userID);
			return "";
		}

		return this->userIdToNameMapping[userID];
	}

	if (IsDebuggerPresent())
	{
		DebugBreak();
	}
	throw std::runtime_error("Server timed out while waiting for user ID");
}

ClientSocketApp::ClientSocketApp()
{
	this->myUserId = INVALID_USER_ID;
	this->currentChatId = INVALID_CHAT_ID;
	this->hResolveEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	InitializeCriticalSection(&this->sendEventCS);

	memset(&this->ui, 0, sizeof(this->ui));

	int proto = 0;

	for (;;)
	{
		char buf[256];

		Logger::PrintMessage("Connect using protocol (0 - IPv4, 1 - IPv6): ");
		if (fgets(buf, sizeof(buf), stdin) != nullptr && sscanf(buf, "%d", &proto) == 1 && (proto == 0 || proto == 1))
		{
			break;
		}
	}

	this->s = socket(proto == 0 ? AF_INET : AF_INET6, SOCK_STREAM, IPPROTO_TCP);

	if (this->s == INVALID_SOCKET)
	{
		LogWarning("Failed to create socket [error %u]", WSAGetLastError());
		throw std::runtime_error("socket() failed");
	}

	char serverIP[256], port[16];

	for (;;)
	{
		Logger::PrintMessage("Server's IP address (default %s): ", proto == 0 ? "127.0.0.1" : "::1");
		if (fgets(serverIP, sizeof(serverIP), stdin) != nullptr)
		{
			break;
		}
	}

	for (;;)
	{
		Logger::PrintMessage("Server's port (default 23456): ");
		if (fgets(port, sizeof(port), stdin) != nullptr)
		{
			break;
		}
	}

	serverIP[strcspn(serverIP, "\r\n")] = 0;
	port[strcspn(port, "\r\n")] = 0;

	if (serverIP[0] == 0)
	{
		strcpy_s(serverIP, proto == 0 ? "127.0.0.1" : "::1");
	}

	if (port[0] == 0)
	{
		strcpy_s(port, "23456");
	}

	addrinfo addr = { 0 };
	addr.ai_family = proto == 0 ? AF_INET : AF_INET6;
	addr.ai_socktype = SOCK_STREAM;
	addr.ai_protocol = IPPROTO_TCP;

	addrinfo* result;
	int resultCode;

	// get the addrinfo of the server address
	if ((resultCode = getaddrinfo(serverIP, port, &addr, &result)) != 0)
	{
		LogWarning("getaddrinfo() failed [error %d]", resultCode);
		throw std::runtime_error("getaddrinfo() failed");
	}

	if (connect(this->s, result->ai_addr, (int)result->ai_addrlen) != 0)
	{
		LogWarning("connect() failed [error %d]", WSAGetLastError());
		throw std::runtime_error("connect() failed");
	}

	LogInfo("Connected successfully to %s%s%s:%s", proto == 1 ? "[" : "", serverIP, proto == 1 ? "]" : "", port);

	this->hNetThread = CreateThread(nullptr, 0, NetworkThread, (LPVOID)this->s, 0, nullptr);

	// global instance of this application
	cApp = this;
}

ClientSocketApp::~ClientSocketApp()
{
	DeleteCriticalSection(&this->sendEventCS);
	CloseHandle(this->hResolveEvent);
}

void ClientSocketApp::CreateChatRoom(const std::vector<uint64_t>& userIDs, bool isGroupChat)
{
	PKT_C2S_CreateChat pkt;
	pkt.userIDs = userIDs;
	pkt.isGroupChat = isGroupChat;

	this->SendNetEvent(pkt.Serialize());
}

void ClientSocketApp::OpenChatRoom(uint64_t chatID)
{
	if (chatID == INVALID_CHAT_ID)
	{
		this->currentChatId = INVALID_CHAT_ID;
		return;
	}

	LogInfo("Opening chat room %I64u", chatID);
	this->currentChatId = chatID;
	this->SetChatRead(chatID);

	PKT_C2S_OpenChat pkt;
	pkt.chatId = chatID;

	this->SendNetEvent(pkt.Serialize());
}

void ClientSocketApp::AddUserToChat(uint64_t userID)
{
	LogInfo("Adding user %I64u", userID);

	PKT_C2S_AddRemoveUser pkt;
	pkt.userId = userID;
	pkt.isRemoveAction = false;

	this->SendNetEvent(pkt.Serialize());
}

void ClientSocketApp::RemoveUserFromChat(uint64_t userID)
{
	LogInfo("Removing user %I64u", userID);

	PKT_C2S_AddRemoveUser pkt;
	pkt.userId = userID;
	pkt.isRemoveAction = true;

	this->SendNetEvent(pkt.Serialize());
}

void ClientSocketApp::CreateFilePromise(const wchar_t* fullPath)
{
	const wchar_t* fileName = wcsrchr(fullPath, L'\\');
	if (fileName == nullptr)
	{
		return;
	}

	fileName++; // skip the \ character

	static std::random_device rd;
	static std::mt19937_64 gen(rd());
	static std::uniform_int_distribution<uint64_t> randDist;

	uint64_t promiseId = randDist(gen);

	LogInfo("Creating file promise: %I64u <--> \xb0\x0d%S\xb0\x0f", promiseId, fileName);

	this->myFilePromises[promiseId] = fullPath;

	PKT_C2S_FilePromise pkt;
	pkt.promiseId = promiseId;
	pkt.fileName = this->UI_WideStringToUTF8(fileName);

	this->SendNetEvent(pkt.Serialize());
}

void ClientSocketApp::DownloadFile(uint64_t promiseID)
{
	LogInfo("Downloading file %I64u", promiseID);

	PKT_C2S_RequestFile pkt;
	pkt.promiseId = promiseID;

	this->currentFilePromiseId = promiseID;
	this->SendNetEvent(pkt.Serialize());
}

HWND ClientSocketApp::UI_GetTopmostWindow()
{
	if (ui.loginDialog.g_Window)
		return ui.loginDialog.g_Window;

	if (ui.addUserDialog.g_Window)
		return ui.addUserDialog.g_Window;

	if (ui.renameChatDialog.g_Window)
		return ui.renameChatDialog.g_Window;

	if (ui.newChatDialog.g_Window)
		return ui.newChatDialog.g_Window;

	return ui.g_Window;
}

std::string ClientSocketApp::UI_WideStringToUTF8(const wchar_t* text)
{
	std::string utfText;
	int textLen = (int)wcslen(text);
	if (textLen == 0)
	{
		return "";
	}

	int reqChars = WideCharToMultiByte(CP_UTF8, 0, text, textLen, nullptr, 0, nullptr, nullptr);
	if (reqChars == 0)
	{
		throw std::runtime_error("WideCharToMultiByte() failed");
	}

	utfText.resize(reqChars);
	WideCharToMultiByte(CP_UTF8, 0, text, textLen, utfText.data(), reqChars, nullptr, nullptr);

	return utfText;
}

std::wstring ClientSocketApp::UI_UTF8ToWideString(const std::string& text)
{
	std::wstring wideText;
	int textLen = text.size();
	if (textLen == 0)
	{
		return L"";
	}

	int reqChars = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), textLen, nullptr, 0);
	if (reqChars == 0)
	{
		throw std::runtime_error("WideCharToMultiByte() failed");
	}

	wideText.resize(reqChars);
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), textLen, wideText.data(), reqChars);

	return wideText;
}

void ClientSocketApp::Run()
{
	CreateGUI();
	std::string username = this->UI_ShowLoginDialog();

	if (username.empty())
	{
		DestroyWindow(ui.g_Window);

		LogError("Username not provided.");
		return;
	}
	else
	{
		PKT_C2S_Login loginPacket;
		loginPacket.username = username;

		this->SendNetEvent(loginPacket.Serialize());
	}

	for (;;)
	{
		if (!UpdateGUI())
		{
			return;
		}
	}
}

void ClientSocketApp::Shutdown()
{
	NetworkThreadExitFlag = true;

	if (this->s != INVALID_SOCKET)
	{
		shutdown(this->s, SD_BOTH);
		closesocket(this->s);
	}

	if (this->hNetThread)
	{
		// Wait for the network thread to exit
		WaitForSingleObject(this->hNetThread, INFINITE);
	}

	this->s = INVALID_SOCKET;
}
