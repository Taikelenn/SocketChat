#include "Logger.h"

#include <Windows.h>
#include <cstdio>
#include <vector>

LARGE_INTEGER firstTick;
LARGE_INTEGER tickFreq;

float Logger::GetLogTime()
{
	LARGE_INTEGER currentTick;
	QueryPerformanceCounter(&currentTick);

	return (currentTick.QuadPart - firstTick.QuadPart) / static_cast<float>(tickFreq.QuadPart);
}

void Logger::Initialize()
{
	QueryPerformanceCounter(&firstTick);
	QueryPerformanceFrequency(&tickFreq);
}

inline void PrintSingleCharacter(char c)
{
	DWORD written;
	WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), &c, 1, &written, NULL);
}

void Logger::PrintMessage(const char* msg, ...)
{
	constexpr int BufferSize = 16384;

	std::vector<char> buf(BufferSize);
	va_list args;
	va_start(args, msg);

	vsnprintf(buf.data(), BufferSize, msg, args);
	char* bufptr = buf.data();

	HANDLE outHandle = GetStdHandle(STD_OUTPUT_HANDLE);

	while (*bufptr)
	{
		if (*bufptr == '\xb0')
		{
			++bufptr;
			SetConsoleTextAttribute(outHandle, *bufptr);
		}
		else
		{
			PrintSingleCharacter(*bufptr);
		}
		++bufptr;
	}

	va_end(args);
}
