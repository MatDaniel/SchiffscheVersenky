// This module defines the io interfaces shared between the server and client for communication,
// used by the io driver classes and game manager
module;

#include "SharedLegacy.h"

export module ShipSock;
using namespace std;

export enum ShipClass : uint8_t { // Specifies the type of ship
						   // is also used as an index into the count array of ShipCount
	DESTROYER_2x1 = 0,
	CRUISER_3x1,
	SUBMARINE_3x1,
	BATTLESHIP_4x1,
	CARRIER_5x1
};
export constexpr uint8_t ShipLengthPerType[5]{
	2, 3, 3, 4, 5
};
export enum ShipRotation {
	FACING_NORTH = 0,
	FACING_EAST = 1,
	FACING_SOUTH = 2,
	FACING_WEST = 3
};

export struct FieldDimension {
	uint8_t XDimension,
		YDimension;
};
export struct FieldCoordinate {
	uint8_t XCord,
		YCord;
};

// Sub packets
export struct ShipCount {
	uint8_t ShipCounts[5]{}; // Use enum ShipClass as an index to get the appropriate count associated to type

	uint8_t GetTotalCount() const {
		auto TotalCount = 0;
		for (auto i = 0; i < 5; ++i)
			TotalCount += ShipCounts[i];
		return TotalCount;
	}
};

export struct ShipState {
	ShipClass ShipType;

	// The cords of the ship always specify the location of the front
	FieldCoordinate Cordinates;

	ShipRotation Rotation;
	bool         Destroyed;

	ShipState& operator=(const ShipState& Rhs) {
		return memcpy(this,
			&Rhs,
			sizeof(ShipState)), * this;
	}
};

export enum CellState : uint8_t {
	CELL_IS_EMPTY = 0,
	CELL_IS_IN_USE = 1,
	CELL_WAS_SHOT_EMPTY = 2,
	CELL_WAS_SHOT_IN_USE = 3,

	// These values may only be used to probe and modify an existing state
	CELL_SHOOT_MERGE_VALUE = 2,
	CELL_PROBE_WAS_SHOT = 2,
	CELL_PROBE_USED = 1,
	CELL_WAS_ALREADY_SHOT = -1,
	Cell_WAS_DESTRUCTOR = -2
};
export CellState operator &=(CellState& Lhs,
	const CellState& Rhs) {

	using CellState_t = underlying_type<CellState>::type;
	return (CellState)((CellState_t&)Lhs &= (CellState_t)CELL_SHOOT_MERGE_VALUE);
}


// Server and Client may both receive as well as send this struct to remotes
export struct ShipSockControl {
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

export class SocketWrap {
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

	static inline atomic<uint32_t> RefrenceCounter = 0;
};
