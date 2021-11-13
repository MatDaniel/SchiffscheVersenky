// Descibes the io interfaces used by the server client netcode for communication (this file has to be always up to date)
#pragma once

#include <stdint.h>

#ifdef _MSC_VER
#  define PACKED_STRUCT(name) \
    __pragma(pack(push, 1)) struct name __pragma(pack(pop))
#elif defined(__GNUC__)
#  define PACKED_STRUCT(name) struct __attribute__((packed)) name
#endif

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
