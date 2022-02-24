#include "ClientApplication.h"

#include "../Logger.h"
#include "../Packets/Protocol.h"

#include <vector>
#include <cstdint>
#include <stdexcept>

#include <CommCtrl.h>

#define NEWCHAT_CLASS_NAME L"SocketChatClient_NewChatDialog"

std::unique_ptr<NewChatDialogResult> dialogResult;

static bool IsUsernameInList(const wchar_t* username)
{
	return SendMessageW(cApp->GetUI()->newChatDialog.g_UserListBox, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)username) != LB_ERR;
}

static int GetUserCountInList()
{
	return (int)SendMessageW(cApp->GetUI()->newChatDialog.g_UserListBox, LB_GETCOUNT, 0, 0);
}

static void UpdateButtons()
{
	if (GetUserCountInList() >= 2)
	{
		SendMessageW(cApp->GetUI()->newChatDialog.g_GroupChatCheckbox, BM_SETCHECK, BST_CHECKED, 0);
		EnableWindow(cApp->GetUI()->newChatDialog.g_GroupChatCheckbox, FALSE);
	}
	else
	{
		EnableWindow(cApp->GetUI()->newChatDialog.g_GroupChatCheckbox, TRUE);
	}

	if (SendMessageW(cApp->GetUI()->newChatDialog.g_GroupChatCheckbox, BM_GETCHECK, 0, 0) == BST_UNCHECKED)
	{
		EnableWindow(cApp->GetUI()->newChatDialog.g_AddUserButton, GetUserCountInList() >= 1 ? FALSE : TRUE);
	}
	else
	{
		EnableWindow(cApp->GetUI()->newChatDialog.g_AddUserButton, TRUE);
	}

	EnableWindow(cApp->GetUI()->newChatDialog.g_RemoveUserButton, SendMessageW(cApp->GetUI()->newChatDialog.g_UserListBox, LB_GETCURSEL, 0, 0) == LB_ERR ? FALSE : TRUE);
}

static LRESULT AddUserPressed()
{
	wchar_t username[128];
	if (GetWindowTextW(cApp->GetUI()->newChatDialog.g_UserEdit, username, _countof(username)) == 0 && GetLastError() != 0)
	{
		throw std::runtime_error("GetWindowTextW() failed");
	}

	if (wcslen(username) == 0 || wcslen(username) > 128)
	{
		MessageBoxW(cApp->GetUI()->newChatDialog.g_Window, L"The username is too short or too long.\r\nIt must be no longer than 128 characters.", L"Invalid username", MB_OK | MB_ICONERROR);
		return 0;
	}

	if (IsUsernameInList(username))
	{
		MessageBoxW(cApp->GetUI()->newChatDialog.g_Window, L"This user has already been added.", L"Invalid username", MB_OK | MB_ICONERROR);
		return 0;
	}

	std::string usernameMultibyte = cApp->UI_WideStringToUTF8(username);
	uint64_t userId = cApp->UsernameToID(usernameMultibyte);
	if (userId == INVALID_USER_ID)
	{
		MessageBoxW(cApp->GetUI()->newChatDialog.g_Window, L"This user does not exist.", L"Invalid username", MB_OK | MB_ICONERROR);
		return 0;
	}

	if (userId == cApp->GetCurrentUserId())
	{
		MessageBoxW(cApp->GetUI()->newChatDialog.g_Window, L"There is no need to add yourself to the chat room.", L"Invalid username", MB_OK | MB_ICONWARNING);
		return 0;
	}

	SendMessageW(cApp->GetUI()->newChatDialog.g_UserListBox, LB_ADDSTRING, 0, (LPARAM)username);
	SetWindowTextW(cApp->GetUI()->newChatDialog.g_UserEdit, nullptr);
	UpdateButtons();

	return 0;
}

static LRESULT RemoveUserPressed()
{
	int selIdx = SendMessageW(cApp->GetUI()->newChatDialog.g_UserListBox, LB_GETCURSEL, 0, 0);
	if (selIdx == LB_ERR)
	{
		return 0;
	}

	SendMessageW(cApp->GetUI()->newChatDialog.g_UserListBox, LB_DELETESTRING, selIdx, 0);
	UpdateButtons();

	return 0;
}

static LRESULT GroupChatChecked()
{
	UpdateButtons();
	return 0;
}

static LRESULT CreateChatPressed()
{
	int userCount = GetUserCountInList();
	if (userCount == 0)
	{
		MessageBoxW(cApp->GetUI()->newChatDialog.g_Window, L"Cannot create an empty chat room.", L"Empty chat room", MB_OK | MB_ICONERROR);
		return 0;
	}

	std::vector<uint64_t> userIDs;
	for (int i = 0; i < userCount; ++i)
	{
		wchar_t username[128];
		if (SendMessageW(cApp->GetUI()->newChatDialog.g_UserListBox, LB_GETTEXT, i, (LPARAM)username) != LB_ERR)
		{
			std::string usernameMultibyte = cApp->UI_WideStringToUTF8(username);
			uint64_t userId = cApp->UsernameToID(usernameMultibyte);

			userIDs.push_back(userId);
		}
	}

	dialogResult = std::make_unique<NewChatDialogResult>();
	dialogResult->userIDs = userIDs;
	dialogResult->isGroupChat = SendMessageW(cApp->GetUI()->newChatDialog.g_GroupChatCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;

	SendMessageW(cApp->GetUI()->newChatDialog.g_Window, WM_CLOSE, 0, 0);
	return 0;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	}
	case WM_COMMAND:
	{
		if (lParam == (LPARAM)cApp->GetUI()->newChatDialog.g_AddUserButton)
		{
			return AddUserPressed();
		}
		else if (lParam == (LPARAM)cApp->GetUI()->newChatDialog.g_RemoveUserButton)
		{
			return RemoveUserPressed();
		}
		else if (lParam == (LPARAM)cApp->GetUI()->newChatDialog.g_GroupChatCheckbox)
		{
			return GroupChatChecked();
		}
		else if (lParam == (LPARAM)cApp->GetUI()->newChatDialog.g_CreateChatButton)
		{
			return CreateChatPressed();
		}
		else if (lParam == (LPARAM)cApp->GetUI()->newChatDialog.g_UserListBox)
		{
			if (HIWORD(wParam) == LBN_SELCHANGE)
			{
				UpdateButtons();
				return 0;
			}
		}
		break;
	}
	}

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

std::unique_ptr<NewChatDialogResult> ClientSocketApp::UI_ShowNewChatDialog()
{
	// Clear the global state (in case there was a previous call to this dialog)
	dialogResult.reset();

	HINSTANCE hInstance = GetModuleHandleA(nullptr);
	HFONT hDefFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	WNDCLASSEXW wc = { 0 };

	// Register window metadata
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = NEWCHAT_CLASS_NAME;
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	RegisterClassExW(&wc);

	// Create the main window and all controls.
	ui.newChatDialog.g_Window = CreateWindowExW(0, NEWCHAT_CLASS_NAME, L"Create a chat room", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 273, 195, ui.g_Window, nullptr, hInstance, nullptr);
	ui.newChatDialog.g_UserListBox = CreateWindowExW(0, L"LISTBOX", nullptr, WS_VISIBLE | WS_CHILD | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_BORDER,
		12, 12, 124, 101, ui.newChatDialog.g_Window, nullptr, hInstance, nullptr);
	ui.newChatDialog.g_AddUserButton = CreateWindowExW(0, L"BUTTON", L"Add user", WS_VISIBLE | WS_CHILD | BS_CENTER | BS_PUSHBUTTON,
		142, 38, 98, 23, ui.newChatDialog.g_Window, nullptr, hInstance, nullptr);
	ui.newChatDialog.g_RemoveUserButton = CreateWindowExW(0, L"BUTTON", L"Remove user", WS_VISIBLE | WS_CHILD | WS_DISABLED | BS_CENTER | BS_PUSHBUTTON,
		142, 67, 98, 23, ui.newChatDialog.g_Window, nullptr, hInstance, nullptr);
	ui.newChatDialog.g_CreateChatButton = CreateWindowExW(0, L"BUTTON", L"Create a new chat", WS_VISIBLE | WS_CHILD | BS_CENTER | BS_PUSHBUTTON,
		12, 119, 228, 25, ui.newChatDialog.g_Window, nullptr, hInstance, nullptr);
	ui.newChatDialog.g_UserEdit = CreateWindowExW(0, L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOVSCROLL,
		142, 12, 100, 20, ui.newChatDialog.g_Window, nullptr, hInstance, nullptr);
	ui.newChatDialog.g_GroupChatCheckbox = CreateWindowExW(0, L"BUTTON", L"Group chat", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
		142, 96, 79, 17, ui.newChatDialog.g_Window, nullptr, hInstance, nullptr);

	SendMessageW(ui.newChatDialog.g_UserEdit, EM_SETCUEBANNER, FALSE, (LPARAM)L"Enter username...");

	// Set a nice "modern-looking" font
	SendMessageW(ui.newChatDialog.g_AddUserButton, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.newChatDialog.g_RemoveUserButton, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.newChatDialog.g_CreateChatButton, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.newChatDialog.g_UserEdit, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.newChatDialog.g_GroupChatCheckbox, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.newChatDialog.g_UserListBox, WM_SETFONT, (WPARAM)hDefFont, FALSE);

	// Show the window
	ShowWindow(ui.newChatDialog.g_Window, SW_NORMAL);
	EnableWindow(ui.g_Window, FALSE);

	MSG msg = { 0 };
	while (GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	EnableWindow(ui.g_Window, TRUE);
	SetForegroundWindow(ui.g_Window);

	DestroyWindow(ui.newChatDialog.g_Window);

	memset(&ui.newChatDialog, 0, sizeof(ui.newChatDialog));

	return std::move(dialogResult);
}
