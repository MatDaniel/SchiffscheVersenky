// Descibes the io interfaces used by the server client netcode for communication (this file has to be always up to date)
#pragma once

#pragma comment(lib, "ws2_32.lib")
#include <cstdint>
#include <WinSock2.h>
#include <WS2tcpip.h>

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <intrin.h>
#include <atomic>
#define PACKED_STRUCT(name) \
	__pragma(pack(push, 1)) struct name __pragma(pack(pop))

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
#elif
#define SsAssert(...)
#endif

inline void WaitForDebugger() {
	while (!IsDebuggerPresent())
		if (!SwitchToThread())
			SleepEx(100, TRUE);
}



// Server and Client may both recive as well as send this struct to remotes
struct ShipSockControl {
	uint8_t SizeOfThisStruct; // Has to Specifiy the size of the this struct including the flexible array member content,
	                          // this is used for transmiting data, if this field is not set properly the send controls will probably fail

	enum ShipControlCommandName {
		NO_COMMAND,        // Sould be unused (reserved for @Lima)
		SET_SHIP_LOC,      // Send by Client (only during setup)
		SHOOT_CELL,        // Send by Client 
		NOTIFY_CELL_STATE, // Send by Server 
		NOTIFY_DEAD_SHIP,  // Send by Server to client if ship is destroyed
		NOTIFY_SPECTATORS, // Send by Server (only send to spectators)
	} ControlCode;

	// The Following contains all data required to describe the players fields, not all fields have to be used by any command and better not be exposed 
	
	enum ShipTypes : uint8_t { // Specifies the type of ship
		DESTROYER_2x1,
		CRUISER_3x1,
		SUBMARINE_3x1,
		BATTLESHIP_4x1,
		CARRIER_5x1
	} ShipType;

	uint8_t XCord,    // Specifies the location of the ship in the system (if this is the front or not well i dont care @Sherlock make it work)
	        YCord;
	uint8_t Rotation; // Specifies the rotation of the ship (optimally north, east and so on with north starting at 0, maybe an enum idk)

	// Very experimental you guys modify this at any time if you please so
	union {
		struct {
			uint8_t IsHidden : 1;
			uint8_t IsDamaged : 1;
			uint8_t IsDestroyed : 1;
		} CurrentStateMask;

		enum : uint8_t {
			UNKNOWN,
			WATER,
			SHIP_HIT
		} CellState;
	};

	uint8_t FlexibleArrayMember[]; // this points to the end of the struct and can be additionally transmitted with the struct through the protocol,
	                               // should be useful for passing additional information that does not belong in the base class
};

class SocketWrap {
public:
	inline SocketWrap() {
		if (++RefrenceCounter == 1) {
			SsLog("Opening ShipSock\n");
			WSADATA WinSockData;
			auto Result = WSAStartup(MAKEWORD(2, 2),
				&WinSockData);
			SsAssert(Result,
				"failed to open Windows Socket API 2.2 [%d]\n",
				Result);
		}
	}
	inline ~SocketWrap() {
		if (--RefrenceCounter == 0)
			WSACleanup();
	}

	static inline std::atomic<uint32_t> RefrenceCounter = 0;
};

inline const char* DefaultPortNumber    = "50001";
inline const char* DefaultServerAddress = "127.0.0.1";

#define PACKET_BUFFER_SIZE 0x2000
#define SSOCKET_DISCONNECTED 0

#define FIELD_WIDTH  10
#define FIELD_HEIGHT 10
#define NUMBER_OF_SHIPS



enum ShipSocketStatus {
	STATUS_SOCKETERROR = SOCKET_ERROR,
	STATUS_SUCESSFUL = 0,
	STATUS_WORK_PENDING = 1,
	STATUS_FAILED_TO_CONNECT = -2,


};

