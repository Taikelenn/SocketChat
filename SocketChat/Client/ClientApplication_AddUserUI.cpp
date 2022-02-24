#include "ClientApplication.h"

#include "../Logger.h"

#include <string>
#include <stdexcept>
#include <cstdio>

#define ADDUSER_CLASS_NAME L"SocketChatClient_AddUser"

static std::string usernameMultibyte;

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
		if (lParam == (LPARAM)cApp->GetUI()->addUserDialog.g_AddUserButton)
		{
			// When we press the "add user" button, get the username and convert it to UTF-8.
			wchar_t username[128];
			if (GetWindowTextW(cApp->GetUI()->addUserDialog.g_UsernameEdit, username, _countof(username)) == 0 && GetLastError() != 0)
			{
				throw std::runtime_error("GetWindowTextW() failed");
			}

			if (wcslen(username) == 0 || wcslen(username) > 128)
			{
				MessageBoxW(cApp->GetUI()->addUserDialog.g_Window, L"The username is too short or too long.\r\nIt must be no longer than 128 characters.", L"Invalid username", MB_OK | MB_ICONERROR);
				return 0;
			}

			usernameMultibyte = cApp->UI_WideStringToUTF8(username);
			SendMessageW(cApp->GetUI()->addUserDialog.g_Window, WM_CLOSE, 0, 0);

			return 0;
		}
	}
	}

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

std::string ClientSocketApp::UI_ShowAddUserDialog()
{
	// Clear the global state (in case there was a previous call to this dialog)
	usernameMultibyte.clear();

	HINSTANCE hInstance = GetModuleHandleA(nullptr);
	HFONT hDefFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	WNDCLASSEXW wc = { 0 };

	// Register window metadata
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = ADDUSER_CLASS_NAME;
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	RegisterClassExW(&wc);

	// Create the main window and all controls.
	ui.addUserDialog.g_Window = CreateWindowExW(0, ADDUSER_CLASS_NAME, L"Add user", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, 411, 88, ui.g_Window, nullptr, hInstance, nullptr);
	ui.addUserDialog.g_PromptStatic = CreateWindowExW(0, L"STATIC", L"Enter username:", WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP,
		12, 17, 84, 13, ui.addUserDialog.g_Window, nullptr, hInstance, nullptr);
	ui.addUserDialog.g_UsernameEdit = CreateWindowExW(0, L"EDIT", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOVSCROLL,
		102, 14, 198, 20, ui.addUserDialog.g_Window, nullptr, hInstance, nullptr);
	ui.addUserDialog.g_AddUserButton = CreateWindowExW(0, L"BUTTON", L"Add user", WS_VISIBLE | WS_CHILD | BS_CENTER | BS_PUSHBUTTON,
		306, 12, 75, 23, ui.addUserDialog.g_Window, nullptr, hInstance, nullptr);

	// Set a nice "modern-looking" font
	SendMessageW(ui.addUserDialog.g_PromptStatic, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.addUserDialog.g_UsernameEdit, WM_SETFONT, (WPARAM)hDefFont, FALSE);
	SendMessageW(ui.addUserDialog.g_AddUserButton, WM_SETFONT, (WPARAM)hDefFont, FALSE);

	// Show the window
	ShowWindow(ui.addUserDialog.g_Window, SW_NORMAL);
	EnableWindow(ui.g_Window, FALSE);

	MSG msg = { 0 };
	while (GetMessageW(&msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	EnableWindow(ui.g_Window, TRUE);
	SetForegroundWindow(ui.g_Window);

	DestroyWindow(ui.addUserDialog.g_Window);

	memset(&ui.addUserDialog, 0, sizeof(ui.addUserDialog));

	return usernameMultibyte;
}
