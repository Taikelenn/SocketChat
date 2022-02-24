#pragma once

namespace Logger {
	float GetLogTime();

	void Initialize();
	void PrintMessage(const char* msg, ...);
}

#define LogInfo(msg, ...) Logger::PrintMessage("\xb0\x0a[%09.3f] INFO\xb0\x0f: " msg "\n", Logger::GetLogTime(), __VA_ARGS__)
#define LogWarning(msg, ...) Logger::PrintMessage("\xb0\x0e[%09.3f] WARN\xb0\x0f: " msg "\n", Logger::GetLogTime(), __VA_ARGS__)
#define LogError(msg, ...) Logger::PrintMessage("\xb0\x0c[%09.3f] ERROR\xb0\x0f: " msg "\n", Logger::GetLogTime(), __VA_ARGS__)
