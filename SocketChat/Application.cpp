#include "Application.h"
#include "Logger.h"

#include <Windows.h>

Application::~Application()
{
}

const wchar_t* Application::GetWindowsErrorDescription(unsigned long long errorCode)
{
	switch (errorCode)
	{
	case WSAECONNRESET:
		return L"Connection reset";
	case WSAETIMEDOUT:
		return L"Connection timed out";
	case WSAECONNREFUSED:
		return L"Connection refused";
	}

	return L"Unknown error";
}
