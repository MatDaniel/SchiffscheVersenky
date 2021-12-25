// Descibes the io interfaces used by the server client netcode for communication (this file has to be always up to date)
// Edit, this file is a huge mess and has to be completely rewritten 

#pragma once

#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>

#include <cstdint>

#include <spdlog/tweakme.h>

#define NOMINMAX
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>


#ifdef _MSC_VER
#include <cstdio>
#include <intrin.h>
#include <atomic>

#pragma warning(disable : 26812) // unscoped enum
#pragma warning(disable : 6336)  // hysteric, buffer overrun
#pragma warning(disable : 28183) // hysteric, null pointer realloc
#pragma warning(disable : 6201)  // hysteric, outofbounds index
// #pragma warning(disable : )


#define PACKED_STRUCT(name) \
	__pragma(pack(push, 1)) struct name __pragma(pack(pop))

#if 0
__declspec(noinline)
inline long SsAssertExecute(
	const int   Expression,
	const char* FailString,
	...
) {
	if (Expression) {

		char StringBuffer[2000];
		printf("[ShipSock] ");
		va_list va;
		va_start(va, FailString);
		vsprintf(
			StringBuffer, 
			FailString,
			va);
		va_end(va);

		auto StringInterator = StringBuffer;
		auto LastSTringAddress = StringInterator;
		while (*StringInterator) {
			if (*StringInterator == '\n') {
				*StringInterator = '\0';
				printf(LastSTringAddress);
				printf((*(StringInterator + 1) != '\0') ? "\n           " : "\n");
				LastSTringAddress = StringInterator + 1;
			}
			++StringInterator;
		}
		if (StringInterator != LastSTringAddress) {
			*(uint16_t*)StringInterator = '\n\0';
			printf(LastSTringAddress);
		}

		if (Expression > 0)
			return IsDebuggerPresent();
	}

	return 0;
}

// Make this mess multithreading safe for the client due to the uhm, bad structure...
inline bool SsLastExpressionEvaluated;
#define SsAssert(Expression, FailString, ...)\
	((SsAssertExecute(SsLastExpressionEvaluated = (Expression), (FailString), __VA_ARGS__) &&\
	(__debugbreak(), 1)), SsLastExpressionEvaluated)
#define SsLog(...) SsAssertExecute(-1, __VA_ARGS__)
#endif
#elif
#if 0
#define SsAssert(...)
#endif
#endif

inline void WaitForDebugger() {
	while (!IsDebuggerPresent())
		if (!SwitchToThread())
			SleepEx(100, TRUE);
}


inline const char* DefaultPortNumber    = "50001";
inline const char* DefaultServerAddress = "127.0.0.1";

#define PACKET_BUFFER_SIZE 0x2000
#define SSOCKET_DISCONNECTED 0

#define FIELD_WIDTH  10
#define FIELD_HEIGHT 10
#define NUMBER_OF_SHIPS

using SpdLogger = std::shared_ptr<spdlog::logger>;

#define SPDLOG_FULL_PATTERN "[%7n:%^%-8!l%$ | %s:%# = %!] %v"
#define SPDLOG_FULL_PATTERN_NOALIGN "[%n:%^%l%$ | %s:%# = %!] %v"
#define SPDLOG_SMALL_PATTERN "[%7n:%^%-8!l%$] %v"
#define SPDLOG_SMALL_PATTERN_NOALIGN "[%n:%^%l%$] %v"

#define TRACE_FUNTION_PROTO SPDLOG_TRACE("")


