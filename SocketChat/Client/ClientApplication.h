#pragma once

#include "../Application.h"
#include "../Server/DatabaseInterface.h"

#include <WinSock2.h>
#include <Windows.h>

#include <vector>
#include <memory>
#include <string>
#include <queue>
#include <unordered_map>

#define UI_CHATCOLOR_YOURNAME RGB(0, 64, 255)
#define UI_CHATCOLOR_OTHERS RGB(127, 32, 32)
#define UI_CHATCOLOR_TEXT RGB(0, 0, 0)
#define UI_CHATCOLOR_INFOMSG RGB(0, 127, 64)
#define UI_CHATCOLOR_FILENAME RGB(0, 64, 0)

// Terminates the application due to a network error
#define WM_NETERR_TERMINATE (WM_APP + 1) // wParam: Winsock error code
// Terminates the application due to a login error
#define WM_LOGINERR_TERMINATE (WM_APP + 2) // wParam: error code from packet
// Chat window has been opened
#define WM_CHATOPEN (WM_APP + 3) // wParam: 0; lParam: pointer to PKT_S2C_OpenChatAns
// New message in the currently open chat arrived
#define WM_CHATADDMSG (WM_APP + 4) // wParam: 0; lParam: pointer to PKT_S2C_NewMessage
// Update the list of current chat's participants
#define WM_REPLACEPARTICIPANTS (WM_APP + 5) // wParam: 0; lParam: pointer to PKT_S2C_ReplaceParticipantList

class NetPacket;
class PKT_S2C_LoginAck;
class PKT_S2C_NewChat;
class PKT_S2C_ResolveUsernameAns;
class PKT_S2C_OpenChatAns;
class PKT_S2C_NewMessage;
class PKT_S2C_ChatReadReceipt;
class PKT_S2C_ReplaceChatList;
class PKT_S2C_ReplaceParticipantList;
class PKT_S2C_MessageBox;
class PKT_S2C_StartTransmission;
class PKT_S2C_ReceiveFileChunk;

struct UIVars {
	HWND g_Window;
	HMENU g_Menu;
	HMENU g_MembersMenu;

	HWND g_ChatListStatic;
	HWND g_LoggedInStatic;

	HWND g_RefreshButton;
	HWND g_NewChatButton;
	HWND g_SendButton;

	HWND g_ChatListBox;
	HWND g_ChatContentsEdit;
	HWND g_CurrentMessageEdit;

	struct {
		HWND g_Window;
		HWND g_PromptStatic;
		HWND g_UsernameEdit;
		HWND g_LoginButton;
	} loginDialog;

	struct {
		HWND g_Window;
		HWND g_PromptStatic;
		HWND g_UsernameEdit;
		HWND g_AddUserButton;
	} addUserDialog;

	struct {
		HWND g_Window;
		HWND g_PromptStatic;
		HWND g_ChatNameEdit;
		HWND g_RenameButton;
	} renameChatDialog;

	struct {
		HWND g_Window;
		HWND g_ProgressBar;
		HWND g_CancelButton;
		HWND g_ProgressStatic;
	} downloadDialog;

	struct {
		HWND g_Window;
		HWND g_UserListBox;
		HWND g_UserEdit;
		HWND g_AddUserButton;
		HWND g_RemoveUserButton;
		HWND g_GroupChatCheckbox;
		HWND g_CreateChatButton;
	} newChatDialog;
};

struct NewChatDialogResult {
	std::vector<uint64_t> userIDs;
	bool isGroupChat;
};

class ClientSocketApp : public Application {
private:
	SOCKET s;
	HANDLE hNetThread;
	CRITICAL_SECTION sendEventCS;

	UIVars ui;
	uint64_t myUserId;
	uint64_t currentChatId;
	uint64_t currentFilePromiseId;

	HANDLE hResolveEvent;
	std::unordered_map<std::string, uint64_t> usernameToIdMapping;
	std::unordered_map<uint64_t, std::string> userIdToNameMapping;

	std::vector<uint64_t> menuUserIdMapping;
	std::unordered_map<uint64_t, std::wstring> myFilePromises; // full paths
	std::unordered_map<uint64_t, std::wstring> allFilePromises; // only file names

	void SetChatReadState(uint64_t chatId, bool isRead);

	void HandlePacket_LoginAck(PKT_S2C_LoginAck* packet);
	void HandlePacket_NewChat(PKT_S2C_NewChat* packet);
	void HandlePacket_ResolveUsernameAns(PKT_S2C_ResolveUsernameAns* packet);
	void HandlePacket_OpenChatAns(PKT_S2C_OpenChatAns* packet);
	void HandlePacket_NewMessage(PKT_S2C_NewMessage* packet);
	void HandlePacket_ReplaceChatList(PKT_S2C_ReplaceChatList* packet);
	void HandlePacket_ReplaceParticipantList(PKT_S2C_ReplaceParticipantList* packet);
	void HandlePacket_MessageBox(PKT_S2C_MessageBox* packet);
	void HandlePacket_StartTransmission(PKT_S2C_StartTransmission* packet);
	void HandlePacket_ReceiveFileChunk(PKT_S2C_ReceiveFileChunk* packet);

public:
	ClientSocketApp();
	virtual ~ClientSocketApp();

	inline uint64_t GetCurrentUserId() { return myUserId; }
	inline uint64_t GetCurrentChatId() { return currentChatId; }
	inline uint64_t GetUserIdInMenu(int idx) { return menuUserIdMapping[idx]; }
	void CreateChatRoom(const std::vector<uint64_t>& userIDs, bool isGroupChat);
	void OpenChatRoom(uint64_t chatID);
	void AddUserToChat(uint64_t userID);
	void RemoveUserFromChat(uint64_t userID);
	void CreateFilePromise(const wchar_t* fullPath);
	void DownloadFile(uint64_t promiseID);

	inline void SetChatRead(uint64_t chatId) { this->SetChatReadState(chatId, true); }
	inline void SetChatUnread(uint64_t chatId) { this->SetChatReadState(chatId, false); }

	// Network methods

	void RaiseNetworkError(DWORD winsockErrorCode);
	void SendNetEvent(std::unique_ptr<NetPacket> packet);
	void NotifyNetworkEvent(std::unique_ptr<NetPacket> packet);

	// If the username is unknown to the client, it will send a packet and block until a response is received
	uint64_t UsernameToID(const std::string& username);
	std::string UserIDToName(uint64_t userID);

	inline void RegisterPromiseId(uint64_t filePromiseId, const std::wstring& fileName) { this->allFilePromises[filePromiseId] = fileName; }

	// UI methods

	inline UIVars* GetUI() { return &this->ui; }

	void CreateGUI();
	bool UpdateGUI();

	HWND UI_GetTopmostWindow();
	std::string UI_WideStringToUTF8(const wchar_t* text);
	std::wstring UI_UTF8ToWideString(const std::string& text);

	std::string UI_ShowLoginDialog();
	std::string UI_ShowAddUserDialog();
	std::string UI_ShowRenameChatDialog();
	std::unique_ptr<NewChatDialogResult> UI_ShowNewChatDialog();

	void UI_ClearChatText();
	void UI_AppendChatText(const wchar_t* text, COLORREF color);
	void UI_AppendChatTextLink(const wchar_t* text, uint64_t filePromiseId);

	void UI_UpdateMembersList(DatabaseUserInfoLite* users, size_t userCount);

	// Main application methods

	virtual void Run();
	virtual void Shutdown();
};

extern ClientSocketApp* cApp;
