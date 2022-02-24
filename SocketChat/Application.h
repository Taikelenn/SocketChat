#pragma once

constexpr unsigned short ServerPort = 23456;

class Application {
public:
	virtual ~Application();
	virtual void Run() = 0;
	virtual void Shutdown() = 0;

	static const wchar_t* GetWindowsErrorDescription(unsigned long long errorCode);
};
