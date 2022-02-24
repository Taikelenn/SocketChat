#include <Windows.h>
#include <WinSock2.h>
#include <memory>

#include "Logger.h"

#include "Client/ClientApplication.h"
#include "Server/ServerApplication.h"

int main(int argc, char* argv[])
{
	Logger::Initialize();

	// Initialize WinSock
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		LogError("Winsock initialization failed.");
		return 1;
	}

	bool isClient = true;

	try
	{
		std::unique_ptr<Application> app = nullptr;

		if (argc >= 2 && _stricmp(argv[1], "--server") == 0)
		{
			isClient = false;
			app = std::make_unique<ServerSocketApp>();
		}
		else if (argc >= 2 && _stricmp(argv[1], "--client") == 0)
		{
			app = std::make_unique<ClientSocketApp>();
		}
		else
		{
			LogError("Please specify either --server or --client as a command-line argument.");
			return 1;
		}

		app->Run();
		app->Shutdown();
	}
	catch (const std::exception& ex)
	{
		LogError("Runtime error: %s", ex.what());
		if (isClient)
		{
			MessageBoxW(nullptr, L"An unhandled exception has occurred, the application must be closed.", L"Fatal error", MB_OK | MB_ICONERROR);
		}
	}

	WSACleanup();

	return 0;
}
