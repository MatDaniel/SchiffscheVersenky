// Legacy support header, this header declares and implements all legacy related interfaces
// Its meant to support special macros, configure global compiletime constants and
// provide legacy support for stuff such as headers pre c++20modules
#pragma once

// WinSock library imports
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <WS2tcpip.h>

// Spdlog config and imports
#include <spdlog/tweakme.h>
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#define NOMINMAX
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_color_sinks.h>



// Other globally used imports
#include <cstdint>



// MSVC specific imports and configurations
#ifdef _MSC_VER
#include <intrin.h>

#pragma warning(disable : 26812) // unscoped enum
#pragma warning(disable : 6336)  // hysteric, buffer overrun
#pragma warning(disable : 28183) // hysteric, null pointer realloc
#pragma warning(disable : 6201)  // hysteric, out of bounds index
// #pragma warning(disable : )

#define PACKED_STRUCT(name) \
	__pragma(pack(push, 1)) struct name __pragma(pack(pop))
#endif


	
// Legacy / debugging support helper, doesn't need a module can just be inlined
inline void WaitForDebugger() {
	while (!IsDebuggerPresent())
		if (!SwitchToThread())
			SleepEx(100, FALSE);
}



// Spdlog helpers and abstractions
using SpdLogger = std::shared_ptr<spdlog::logger>;
#define SPDLOG_FULL_PATTERN "[%7n:%^%-8!l%$ | %s:%# = %!] %v"
#define SPDLOG_FULL_PATTERN_NOALIGN "[%n:%^%l%$ | %s:%# = %!] %v"
#define SPDLOG_SMALL_PATTERN "[%7n:%^%-8!l%$] %v"
#define SPDLOG_SMALL_PATTERN_NOALIGN "[%n:%^%l%$] %v"
#define TRACE_FUNTION_PROTO SPDLOG_TRACE("")

// Default server/client connection configuration
inline const char* DefaultPortNumber    = "50001";
inline const char* DefaultServerAddress = "127.0.0.1";

// Network internal helpers
#define PACKET_BUFFER_SIZE 0x2000



// Other utility
#define DECLARE_PSFACTORIED_CLASS(ClassName)\
class ClassName : public ::MagicInstanceManagerBase<ClassName> {\
	friend class ::MagicInstanceManagerBase<ClassName>;
