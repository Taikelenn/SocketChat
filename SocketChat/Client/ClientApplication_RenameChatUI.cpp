#include "ClientApplication.h"

#include "../Logger.h"

#include <string>
#include <stdexcept>
#include <cstdio>

#define RENAMECHAT_CLASS_NAME L"SocketChatClient_RenameChat"

static std::string chatNameMultibyte;

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
		if (lParam == (LPARAM)cApp->GetUI()->renameChatDialog.g_RenameButton)
		{
			// When we press the "add user" button, get the username and convert it to UTF-8.
			wchar_t chatName[256];
			if (GetWindowTextW(cApp->GetUI()->renameChatDialog.g_ChatNameEdit, chatName, _countof(chatName)) == 0 && GetLastError() != 0)
			{
				throw std::runtime_error("GetWindowTextW() failed");
			}

			if (wcslen(chatName) == 0)
			{
				MessageBoxW(cApp->GetUI()->renameChatDialog.g_Window, L"Please enter a chat name.", L"Invalid chat name", MB_OK | MB_ICONWARNING);
				return 0;
			}

			chatNameMultibyte = cApp->UI_WideStringToUTF8(chatName);
			SendMessageW(cApp->GetUI()->renameChatDialog.g_Window, WM_CLOSE, 0, 0);

			return 0;
		}
	}
	}

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

std::string ClientSocketApp::UI_ShowRenameChatDialog()
{
	// Clear the global state (in case there was a previous call to this dialog)
	chatNameMultibyte.clear();

	HINSTANCE hInstance = GetModuleHandleA(nullptr);
	HFONT hDefFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	WNDCLASSEXW wc = { 0 };

	// Register window metadata
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = RENAMECHAT_CLASS_NAME;
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	RegisterClassExW(&wc);

	// Create the main window and all controls.
	ui.renameChatDialog.g_Window = CreateWindowExW(0, RENAMECHAT_CLASS_NAME, L"Rename chat", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 411, 88, ui.g_Window, nullptr, hInstance, nullptr);
	ui.renameChatDialog.g_PromptStatic = CreateWindowExW(0, L"STATIC", L"New chat name:", WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP,
		12, 17, 84, 13, ui.renameChatDialog.g_Window, nullptr, hInstance, nullptr);
	ui.renameChatDialog.g_ChatNameEdit = CreateWindowExW(0, L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOVSCROLL,
		102, 14, 198, 20, ui.renameChatDialog.g_Window, nullptr, hInstance, nullptr);
	ui.renameChatDialog.g_RenameButton = CreateWindowExW(0, L"BUTTON", L"Rename", WS_VISIBLE | WS_CHILD | BS_CENTER | BS_PUSHBUTTON,
		306, 12, 75, 23, ui.renameChatDialog.g_Window, nullptr, hInstance, nullptr);

	// Set a nice "modern-looking" font
	SendMessageW(ui.renameChatDialog.g_PromptStatic, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.renameChatDialog.g_ChatNameEdit, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.renameChatDialog.g_RenameButton, WM_SETFONT, (WPARAM)hDefFont, FALSE);

	// Show the window
	ShowWindow(ui.renameChatDialog.g_Window, SW_NORMAL);
	EnableWindow(ui.g_Window, FALSE);

	MSG msg = { 0 };
	while (GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	EnableWindow(ui.g_Window, TRUE);
	SetForegroundWindow(ui.g_Window);

	DestroyWindow(ui.renameChatDialog.g_Window);

	memset(&ui.renameChatDialog, 0, sizeof(ui.renameChatDialog));

	return chatNameMultibyte;
}
