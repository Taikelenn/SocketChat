#include "ClientApplication.h"

#include "../Logger.h"
#include "../Packets/NetPacket.h"
#include "../Packets/Protocol.h"

#include <WS2tcpip.h>
#include <Windows.h>
#include <Richedit.h>
#include <CommCtrl.h>
#include <shellapi.h>

#include <stdexcept>
#include <cstdio>
#include <ctime>

#define ID_MENU_SHOW_TIMESTAMPS 101
#define ID_MENU_MEMBERS 102
#define ID_MENU_ADD_MEMBER 103
#define ID_MENU_RENAME_CHAT 104
#define ID_MENU_REMOVE_USER_BASE 110

using namespace std::string_literals;

// Makes the UI look prettier, and enables EM_SETCUEBANNER
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define CLASS_NAME L"SocketChatClient"

static std::unordered_map<LONG, uint64_t> linkFileIdMapping;

static uint64_t LinkToFilePromiseId(LONG linkBeginCharIdx)
{
	return linkFileIdMapping[linkBeginCharIdx];
}

static LRESULT SendMessageEvent()
{
	wchar_t message[1024];
	if (GetWindowTextW(cApp->GetUI()->g_CurrentMessageEdit, message, _countof(message)) == 0 && GetLastError() != 0)
	{
		throw std::runtime_error("GetWindowTextW() failed");
	}

	if (wcslen(message) == 0)
	{
		return 0;
	}

	std::string messageMultibyte = cApp->UI_WideStringToUTF8(message);

	PKT_C2S_SendMessage pkt;
	pkt.message = messageMultibyte;

	cApp->SendNetEvent(pkt.Serialize());

	SetWindowTextW(cApp->GetUI()->g_CurrentMessageEdit, nullptr);

	return 0;
}

static void AddMessageToChatbox(ChatMessage* msg)
{
	if (msg->author == INVALID_USER_ID) // system message
	{
		cApp->UI_AppendChatText((L"-- "s + cApp->UI_UTF8ToWideString(msg->message)).c_str(), UI_CHATCOLOR_INFOMSG);
	}
	else if (msg->filePromiseId != 0) // file promise
	{
		std::wstring usernameWide = cApp->UI_UTF8ToWideString(cApp->UserIDToName(msg->author));

		cApp->UI_AppendChatText((usernameWide + L" has sent "s).c_str(), UI_CHATCOLOR_INFOMSG);
		cApp->UI_AppendChatText(cApp->UI_UTF8ToWideString(msg->message).c_str(), UI_CHATCOLOR_FILENAME);
		cApp->UI_AppendChatText(L" - ", UI_CHATCOLOR_TEXT);
		cApp->UI_AppendChatTextLink(L"click here to receive", msg->filePromiseId);

		cApp->RegisterPromiseId(msg->filePromiseId, cApp->UI_UTF8ToWideString(msg->message));
	}
	else
	{
		std::wstring usernameWide = cApp->UI_UTF8ToWideString(cApp->UserIDToName(msg->author));

		cApp->UI_AppendChatText((L"["s + usernameWide + L"] "s).c_str(), msg->author == cApp->GetCurrentUserId() ? UI_CHATCOLOR_YOURNAME : UI_CHATCOLOR_OTHERS);
		cApp->UI_AppendChatText(cApp->UI_UTF8ToWideString(msg->message).c_str(), UI_CHATCOLOR_TEXT);
	}

	cApp->UI_AppendChatText(L"\n", UI_CHATCOLOR_TEXT);
}

void ClientSocketApp::SetChatReadState(uint64_t chatId, bool isRead)
{
	int selectedChat = SendMessageW(cApp->GetUI()->g_ChatListBox, LB_GETCURSEL, 0, 0);
	int chatCount = SendMessageW(cApp->GetUI()->g_ChatListBox, LB_GETCOUNT, 0, 0);

	for (int i = 0; i < chatCount; ++i)
	{
		uint64_t* currChatIdPtr = (uint64_t*)SendMessageW(cApp->GetUI()->g_ChatListBox, LB_GETITEMDATA, i, 0);
		uint64_t currChatId = *currChatIdPtr;
		if (chatId == currChatId)
		{
			wchar_t chatName[128] = { 0 };
			SendMessageW(cApp->GetUI()->g_ChatListBox, LB_GETTEXT, i, (LPARAM)chatName);

			if (!isRead)
			{
				if (wcsstr(chatName, L"*** ") == chatName)
				{
					// The chat is already marked as unread; ignore
					return;
				}

				wchar_t chatNameCopy[128];
				wcscpy(chatNameCopy, chatName);

				swprintf_s(chatName, L"*** %s ***", chatNameCopy);
			}
			else
			{
				if (wcsstr(chatName, L"*** ") != chatName)
				{
					// The chat is already marked as read; ignore
					return;
				}

				wchar_t chatNameCopy[128];
				wcscpy(chatNameCopy, chatName);

				wcscpy(chatName, chatNameCopy + 4); // skip the initial "*** "
				chatName[wcslen(chatName) - 4] = 0; // trim the trailing " ***"
			}

			SendMessageW(cApp->GetUI()->g_ChatListBox, LB_INSERTSTRING, i, (LPARAM)chatName);
			SendMessageW(cApp->GetUI()->g_ChatListBox, LB_SETITEMDATA, i, (LPARAM)new uint64_t(currChatId));
			SendMessageW(cApp->GetUI()->g_ChatListBox, LB_DELETESTRING, i + 1, 0);

			if (selectedChat == i)
			{
				SendMessageW(cApp->GetUI()->g_ChatListBox, LB_SETCURSEL, i, 0);
			}

			delete currChatIdPtr;
			return;
		}
	}
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND topMostWindow = cApp->UI_GetTopmostWindow();
	static bool isMenuOpen = false;

	switch (uMsg)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}
	case WM_KEYDOWN:
	{
		if (hwnd == cApp->GetUI()->g_CurrentMessageEdit && wParam == VK_ESCAPE)
		{
			SetWindowTextW(hwnd, nullptr);
			SetFocus(nullptr);

			return 0;
		}
		break;
	}
	case WM_COMMAND:
	{
		if (HIWORD(wParam) == 0 && LOWORD(wParam) >= ID_MENU_REMOVE_USER_BASE && LOWORD(wParam) <= (ID_MENU_REMOVE_USER_BASE + 128))
		{
			int userIdx = LOWORD(wParam) - ID_MENU_REMOVE_USER_BASE;
			uint64_t userId = cApp->GetUserIdInMenu(userIdx);

			cApp->RemoveUserFromChat(userId);
			return 0;
		}

		if (HIWORD(wParam) == 0 && LOWORD(wParam) == ID_MENU_ADD_MEMBER)
		{
			if (cApp->GetCurrentChatId() == INVALID_CHAT_ID)
			{
				MessageBoxW(cApp->GetUI()->g_Window, L"Please select a chat first.", L"Cannot add user", MB_ICONWARNING | MB_OK);
				return 0;
			}

			std::string dialogResult = cApp->UI_ShowAddUserDialog();
			if (dialogResult.empty())
			{
				return 0;
			}

			uint64_t userId = cApp->UsernameToID(dialogResult);
			if (userId == INVALID_USER_ID)
			{
				MessageBoxW(cApp->GetUI()->g_Window, L"User not found.", L"Cannot add user", MB_ICONWARNING | MB_OK);
				return 0;
			}

			PKT_C2S_AddRemoveUser pkt;
			pkt.userId = userId;
			pkt.isRemoveAction = false;

			cApp->SendNetEvent(pkt.Serialize());

			return 0;
		}

		if (HIWORD(wParam) == 0 && LOWORD(wParam) == ID_MENU_RENAME_CHAT)
		{
			if (cApp->GetCurrentChatId() == INVALID_CHAT_ID)
			{
				MessageBoxW(cApp->GetUI()->g_Window, L"Please select a chat first.", L"Cannot add user", MB_ICONWARNING | MB_OK);
				return 0;
			}

			std::string dialogResult = cApp->UI_ShowRenameChatDialog();
			if (dialogResult.empty())
			{
				return 0;
			}

			PKT_C2S_RenameChat pkt;
			pkt.newName = dialogResult;

			cApp->SendNetEvent(pkt.Serialize());
			return 0;
		}

		if (lParam == (LPARAM)cApp->GetUI()->g_NewChatButton)
		{
			std::unique_ptr<NewChatDialogResult> dialogResult = cApp->UI_ShowNewChatDialog();
			if (dialogResult)
			{
				cApp->CreateChatRoom(dialogResult->userIDs, dialogResult->isGroupChat);
			}
			return 0;
		}
		else if (lParam == (LPARAM)cApp->GetUI()->g_RefreshButton)
		{
			PKT_C2S_OpenChat pkt;
			pkt.chatId = INVALID_CHAT_ID;

			cApp->SendNetEvent(pkt.Serialize());
			return 0;
		}
		else if (lParam == (LPARAM)cApp->GetUI()->g_ChatListBox)
		{
			if (HIWORD(wParam) == LBN_SELCHANGE)
			{
				int chatIdx = SendMessageW(cApp->GetUI()->g_ChatListBox, LB_GETCURSEL, 0, 0);
				if (chatIdx != LB_ERR)
				{
					uint64_t chatID = *(uint64_t*)SendMessageW(cApp->GetUI()->g_ChatListBox, LB_GETITEMDATA, chatIdx, 0);
					if (chatID != (int)cApp->GetCurrentChatId())
					{
						cApp->OpenChatRoom(chatID);
					}
				}

				return 0;
			}
		}
		else if (lParam == (LPARAM)cApp->GetUI()->g_SendButton)
		{
			return SendMessageEvent();
		}
		break;
	}
	case WM_NOTIFY:
	{
		NMHDR* hdr = (NMHDR*)lParam;
		if (hdr->code == EN_LINK)
		{
			ENLINK* linkInfo = (ENLINK*)lParam;
			if (linkInfo->msg == WM_LBUTTONUP)
			{
				uint64_t filePromiseId = LinkToFilePromiseId(linkInfo->chrg.cpMin);
				if (filePromiseId == 0)
				{
					MessageBoxW(cApp->GetUI()->g_Window, L"The link does not seem to point to a valid file.", L"Promise ID not found", MB_OK | MB_ICONERROR);
					return 0;
				}

				cApp->DownloadFile(filePromiseId);
			}
		}
		break;
	}
	case WM_ENTERMENULOOP:
	{
		isMenuOpen = true;
		break;
	}
	case WM_EXITMENULOOP:
	{
		isMenuOpen = false;
		break;
	}
	case WM_NETERR_TERMINATE:
	{
		if (wParam == 0)
		{
			MessageBoxW(topMostWindow, L"The server has closed the connection.", L"Goodbye", MB_OK | MB_ICONINFORMATION);
		}
		else
		{
			wchar_t buf[1024];
			swprintf_s(buf, L"Connection to the server has been lost.\r\nError details: %s", cApp->GetWindowsErrorDescription(wParam));

			MessageBoxW(topMostWindow, buf, L"Connection error", MB_OK | MB_ICONERROR);
		}

		PostQuitMessage((int)wParam);
		return 0;
	}
	case WM_LOGINERR_TERMINATE:
	{
		switch ((LoginResult)wParam)
		{
		case LoginResult::UsernameWrongLength:
			MessageBoxW(topMostWindow, L"The server decided that your username's length is incorrect. Please, change your username.", L"Login error", MB_OK | MB_ICONERROR);
			break;

		case LoginResult::Failed:
			MessageBoxW(topMostWindow, L"Generic failure while logging in.", L"Login error", MB_OK | MB_ICONERROR);
			break;
		}

		PostQuitMessage(0);
		return 0;
	}
	case WM_CHATOPEN:
	{
		PKT_S2C_OpenChatAns* packet = (PKT_S2C_OpenChatAns*)lParam;

		SendMessageW(cApp->GetUI()->g_ChatContentsEdit, WM_SETREDRAW, FALSE, 0);
		cApp->UI_ClearChatText();

		for (size_t i = 0; i < packet->messages.size(); ++i)
		{
			ChatMessage* msg = &packet->messages[i];
			AddMessageToChatbox(msg);
		}

		SendMessageW(cApp->GetUI()->g_ChatContentsEdit, WM_SETREDRAW, TRUE, 0);
		RedrawWindow(cApp->GetUI()->g_ChatContentsEdit, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);

		delete packet;
		break;
	}
	case WM_CHATADDMSG:
	{
		PKT_S2C_NewMessage* packet = (PKT_S2C_NewMessage*)lParam;
		AddMessageToChatbox(&packet->message);

		delete packet;
		break;
	}
	case WM_REPLACEPARTICIPANTS:
	{
		PKT_S2C_ReplaceParticipantList* packet = (PKT_S2C_ReplaceParticipantList*)lParam;
		if (!isMenuOpen)
		{
			cApp->UI_UpdateMembersList(packet->users.data(), packet->users.size());
		}

		delete packet;
		break;
	}
	}

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

WNDPROC originalMsgEditWndProc;

static LRESULT CALLBACK MessageEditWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CHAR)
	{
		if (wParam == VK_ESCAPE)
		{
			SetWindowTextW(hwnd, nullptr);
			SetFocus(nullptr);

			return 0;
		}
		else if (wParam == VK_RETURN)
		{
			return SendMessageEvent();
		}
	}

	return CallWindowProcW(originalMsgEditWndProc, hwnd, uMsg, wParam, lParam);
}

WNDPROC originalChatContentsWndProc;

static LRESULT CALLBACK ChatBoxEditWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_DROPFILES)
	{
		HDROP hDrop = (HDROP)wParam;
		if (cApp->GetCurrentChatId() == INVALID_CHAT_ID)
		{
			MessageBoxW(cApp->GetUI()->g_Window, L"Please select a chat first.", L"Cannot add user", MB_ICONWARNING | MB_OK);
			DragFinish(hDrop);
			return 0;
		}

		if (DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0) != 1)
		{
			MessageBoxW(cApp->GetUI()->g_Window, L"Please drop only one file.", L"Too many files", MB_OK | MB_ICONWARNING);
			DragFinish(hDrop);
			return 0;
		}

		wchar_t filePath[512];
		if (DragQueryFileW(hDrop, 0, filePath, _countof(filePath)) != 0)
		{
			DWORD fileAttribs = GetFileAttributesW(filePath);
			bool isFile = fileAttribs != INVALID_FILE_ATTRIBUTES && !(fileAttribs & FILE_ATTRIBUTE_DIRECTORY);

			if (!isFile)
			{
				MessageBoxW(cApp->GetUI()->g_Window, L"The specified object is not a file.", L"Not a file", MB_OK | MB_ICONWARNING);
				DragFinish(hDrop);
				return 0;
			}

			cApp->CreateFilePromise(filePath);
		}

		DragFinish(hDrop);
		return 0;
	}

	return CallWindowProcW(originalChatContentsWndProc, hwnd, uMsg, wParam, lParam);
}

void ClientSocketApp::CreateGUI()
{
	LoadLibraryW(L"msftedit.dll"); // Load the required .dll for RichText controls

	ui.g_Menu = CreateMenu();
	HMENU hSubMenu = CreatePopupMenu();

	AppendMenuW(hSubMenu, MF_STRING, ID_MENU_ADD_MEMBER, L"Add chat member");
	AppendMenuW(hSubMenu, MF_STRING, ID_MENU_SHOW_TIMESTAMPS, L"Show timestamps");
	AppendMenuW(hSubMenu, MF_STRING, ID_MENU_RENAME_CHAT, L"Rename chat");
	AppendMenuW(ui.g_Menu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, L"Chat");

	HINSTANCE hInstance = GetModuleHandleA(nullptr);
	HFONT hDefFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	WNDCLASSEXW wc = { 0 };

	// Register window metadata
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	RegisterClassExW(&wc);

	// Create the main window and all controls.
	ui.g_Window = CreateWindowExW(0, CLASS_NAME, L"Generic messaging client", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 679, 395, nullptr, ui.g_Menu, hInstance, nullptr);
	ui.g_ChatListStatic = CreateWindowExW(0, L"STATIC", L"Select a chat room", WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 12, 9, 96, 13, ui.g_Window, nullptr, hInstance, nullptr);
	ui.g_LoggedInStatic = CreateWindowExW(0, L"STATIC", L"Logged in as: ???", WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP, 185, 9, 450, 13, ui.g_Window, nullptr, hInstance, nullptr);
	ui.g_ChatListBox = CreateWindowExW(0, L"LISTBOX", nullptr, WS_VISIBLE | WS_CHILD | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_BORDER,
		12, 30, 170, 264, ui.g_Window, nullptr, hInstance, nullptr);
	ui.g_RefreshButton = CreateWindowExW(0, L"BUTTON", L"Refresh", WS_VISIBLE | WS_CHILD | BS_CENTER | BS_PUSHBUTTON, 12, 300, 82, 23, ui.g_Window, nullptr, hInstance, nullptr);
	ui.g_NewChatButton = CreateWindowExW(0, L"BUTTON", L"New chat", WS_VISIBLE | WS_CHILD | BS_CENTER | BS_PUSHBUTTON, 100, 300, 82, 23, ui.g_Window, nullptr, hInstance, nullptr);
	ui.g_ChatContentsEdit = CreateWindowExW(WS_EX_ACCEPTFILES, MSFTEDIT_CLASS, nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_DISABLENOSCROLL | ES_MULTILINE | ES_READONLY | ES_NOHIDESEL,
		188, 30, 463, 264, ui.g_Window, nullptr, hInstance, nullptr);
	ui.g_CurrentMessageEdit = CreateWindowExW(0, L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 188, 302, 362, 20, ui.g_Window, nullptr, hInstance, nullptr);
	ui.g_SendButton = CreateWindowExW(0, L"BUTTON", L"Send message", WS_VISIBLE | WS_CHILD | BS_CENTER | BS_PUSHBUTTON, 556, 300, 95, 23, ui.g_Window, nullptr, hInstance, nullptr);

	// subclass the textbox for current message (we want to handle ESC and ENTER differently)
	originalMsgEditWndProc = (WNDPROC)SetWindowLongPtrW(ui.g_CurrentMessageEdit, GWLP_WNDPROC, (LONG_PTR)MessageEditWndProc);

	// subclass the textbox for current message (we want to handle ESC and ENTER differently)
	originalChatContentsWndProc = (WNDPROC)SetWindowLongPtrW(ui.g_ChatContentsEdit, GWLP_WNDPROC, (LONG_PTR)ChatBoxEditWndProc);

	// Set the hint text for current message textbox
	SendMessageW(ui.g_CurrentMessageEdit, EM_SETCUEBANNER, FALSE, (LPARAM)L"Enter your message here");

	// Set a nice "modern-looking" font
	SendMessageW(ui.g_ChatListStatic, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.g_LoggedInStatic, WM_SETFONT, (WPARAM)hDefFont, FALSE);

	SendMessageW(ui.g_RefreshButton, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.g_NewChatButton, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.g_SendButton, WM_SETFONT, (WPARAM)hDefFont, FALSE);

	SendMessageW(ui.g_ChatListBox, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.g_ChatContentsEdit, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.g_CurrentMessageEdit, WM_SETFONT, (WPARAM)hDefFont, FALSE);

	// Show the window
	ShowWindow(ui.g_Window, SW_NORMAL);
}

bool ClientSocketApp::UpdateGUI()
{
	MSG msg = { 0 };

	if (GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
		return true;
	}

	return false;
}

void ClientSocketApp::UI_ClearChatText()
{
	SendMessageW(ui.g_ChatContentsEdit, EM_SETSEL, 0, -1);
	SendMessageW(ui.g_ChatContentsEdit, EM_REPLACESEL, FALSE, (LPARAM)L"");
	SendMessageW(ui.g_ChatContentsEdit, EM_EMPTYUNDOBUFFER, 0, 0);
	SendMessageW(ui.g_ChatContentsEdit, EM_SETEVENTMASK, 0, ENM_LINK);

	linkFileIdMapping.clear();
}

void ClientSocketApp::UI_AppendChatText(const wchar_t* text, COLORREF color)
{
	CHARFORMATW cfm = { 0 };
	cfm.cbSize = sizeof(cfm);
	cfm.crTextColor = color;
	cfm.dwMask = CFM_COLOR;

	SendMessageW(ui.g_ChatContentsEdit, EM_SETSEL, (WPARAM)-1, -2);
	SendMessageW(ui.g_ChatContentsEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfm);
	SendMessageW(ui.g_ChatContentsEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
}

void ClientSocketApp::UI_AppendChatTextLink(const wchar_t* text, uint64_t filePromiseId)
{
	CHARFORMAT2W cfm = { 0 };
	cfm.cbSize = sizeof(cfm);
	cfm.dwMask = CFM_LINK;
	cfm.dwEffects = CFE_LINK;

	int posBeforeInsert = 0;
	int lineCount = SendMessageW(ui.g_ChatContentsEdit, EM_GETLINECOUNT, 0, 0);
	for (int i = 0; i < lineCount; ++i)
	{
		int charIdx = SendMessageW(ui.g_ChatContentsEdit, EM_LINEINDEX, i, 0);
		posBeforeInsert += SendMessageW(ui.g_ChatContentsEdit, EM_LINELENGTH, charIdx, 0) + 1;
	}

	SendMessageW(ui.g_ChatContentsEdit, EM_SETSEL, (WPARAM)-1, -2);
	SendMessageW(ui.g_ChatContentsEdit, EM_REPLACESEL, FALSE, (LPARAM)text);

	SendMessageW(ui.g_ChatContentsEdit, EM_SETSEL, (WPARAM)posBeforeInsert - 1, (LPARAM)-1);
	SendMessageW(ui.g_ChatContentsEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfm);
	SendMessageW(ui.g_ChatContentsEdit, EM_SETSEL, (WPARAM)-1, -2);

	linkFileIdMapping[posBeforeInsert - 1] = filePromiseId;
}

void ClientSocketApp::UI_UpdateMembersList(DatabaseUserInfoLite* users, size_t userCount)
{
	menuUserIdMapping.clear();
	int nextMenuIndex = 0;

	if (cApp->GetUI()->g_MembersMenu)
	{
		DeleteMenu(cApp->GetUI()->g_Menu, ID_MENU_MEMBERS, MF_BYCOMMAND);
		DestroyMenu(cApp->GetUI()->g_MembersMenu);
		cApp->GetUI()->g_MembersMenu = nullptr;
	}

	cApp->GetUI()->g_MembersMenu = CreatePopupMenu();
	for (size_t i = 0; i < userCount; ++i)
	{
		if (users[i].lastSeen == CURRENTLY_ONLINE)
		{
			std::wstring usernameWide = cApp->UI_UTF8ToWideString(cApp->UserIDToName(users[i].userId)) + L"(" + std::to_wstring(users[i].userId) + L")";

			HMENU hUserMenu = CreatePopupMenu();
			AppendMenuW(hUserMenu, MF_STRING | MF_DISABLED, 0, L"Last seen: online");
			AppendMenuW(hUserMenu, MF_STRING | MF_DISABLED, 0, users[i].hasReadChat ? L"Has seen recent messages" : L"Has NOT seen recent messages");
			AppendMenuW(hUserMenu, MF_STRING, ID_MENU_REMOVE_USER_BASE + nextMenuIndex++, L"Remove user from group");
			AppendMenuW(cApp->GetUI()->g_MembersMenu, MF_STRING | MF_POPUP, (UINT_PTR)hUserMenu, usernameWide.c_str());

			menuUserIdMapping.push_back(users[i].userId);
		}
	}

	AppendMenuW(cApp->GetUI()->g_MembersMenu, MF_SEPARATOR, 0, nullptr);

	for (size_t i = 0; i < userCount; ++i)
	{
		if (users[i].lastSeen != CURRENTLY_ONLINE)
		{
			std::wstring usernameWide = cApp->UI_UTF8ToWideString(cApp->UserIDToName(users[i].userId)) + L"(" + std::to_wstring(users[i].userId) + L")";
			wchar_t buf[256] = { 0 };

			tm* timeinfo = localtime((const time_t*)&users[i].lastSeen);
			wcsftime(buf, _countof(buf), L"Last seen: %Y-%m-%d %H:%M:%S", timeinfo);

			HMENU hUserMenu = CreatePopupMenu();
			AppendMenuW(hUserMenu, MF_STRING | MF_DISABLED, 0, buf);
			AppendMenuW(hUserMenu, MF_STRING | MF_DISABLED, 0, users[i].hasReadChat ? L"Has seen recent messages" : L"Has NOT seen recent messages");
			AppendMenuW(hUserMenu, MF_STRING, ID_MENU_REMOVE_USER_BASE + nextMenuIndex++, L"Remove user from group");
			AppendMenuW(cApp->GetUI()->g_MembersMenu, MF_STRING | MF_POPUP, (UINT_PTR)hUserMenu, usernameWide.c_str());

			menuUserIdMapping.push_back(users[i].userId);
		}
	}

	MENUITEMINFOW itemInfo = { 0 };
	itemInfo.cbSize = sizeof(itemInfo);
	itemInfo.fMask = MIIM_STRING | MIIM_SUBMENU | MIIM_ID;
	itemInfo.dwTypeData = (LPWSTR)L"Members";
	itemInfo.hSubMenu = cApp->GetUI()->g_MembersMenu;
	itemInfo.wID = ID_MENU_MEMBERS;

	InsertMenuItemW(cApp->GetUI()->g_Menu, (UINT)-1, TRUE, &itemInfo);
	DrawMenuBar(ui.g_Window);
}
