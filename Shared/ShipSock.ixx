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
		TRACE_FUNTION_PROTO;

		auto TotalCount = 0;
		for (auto i = 0; i < 5; ++i)
			TotalCount += ShipCounts[i];
		return TotalCount;
	}
};

export struct ShipState {
	// Ship base information
	ShipClass       ShipType;
	FieldCoordinate Cordinates; // The cords of the ship always specify the location of the front
	ShipRotation    Rotation;
	
	// Additional meta data
	bool Destroyed;
};

export enum CellState : int8_t {
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
	TRACE_FUNTION_PROTO;

	using CellState_t = underlying_type<CellState>::type;
	return (CellState)((CellState_t&)Lhs &= (CellState_t)CELL_SHOOT_MERGE_VALUE);
}


export enum ShipControlStatus {
	STATUS_INVALID_PLACEMENT = -2000,
	STATUS_NOT_YOUR_TURN,

	STATUS_NEUTRAL = 0,

	STATUS_YOUR_TURN,
	STATUS_YOURE_LOBBY_HOST, // special, may never be implemented for MVP version

};

// Server and Client may both receive as well as send this struct to remotes
export struct ShipSockControl {
	uint8_t SizeOfThisStruct = sizeof(*this); // Has to specify the size of the this struct including the flexible array member content,
							                  // this is used for transmitting data, if this field is not set properly the send controls will probably fail

	enum ShipControlCommandName : int8_t {
		// The following is only send by the client to server
		NO_COMMAND_CLIENT = 0, // Should be unused (reserved for @Lima)
		SET_SHIP_LOC_CLIENT,   // Send only during setup, specifies where to place a ship
		SHOOT_CELL_CLIENT,     // Client requesting a cell to be shot
		READY_UP_CLIENT,       // Send by the client to state if they are ready or not
		
		// The following is only send by server to client
		NO_COMMAND_SERVER = 0, // Should be unused (reserved for @Lima)
		CELL_STATE_SERVER,     // Notifies clients of cell state at location
		DEAD_SHIP_SERVER,      // Notifies clients if ship is destroyed, posts its state information (overlays CELL_STATE_SERVER)
		PLAYER_TURN_SERVER,    // Notifies the player if its his turn or not
		GAME_READY_SERVER,     // Broadcasted to all players to let the game begin
		STARTUP_FIELDSIZE,     // Broadcasts the field dimensions of the current game to all connecting clients
		STARTUP_SHIPCOUNTS,    // Sends players specifically the number of ships available for the game
		
		// Command control codes shared by server and client
		RAISE_STATUS_MESSAGE = 2000, // Used to send status messages to clients

	} ControlCode;

	// The Following contains all data required to describe the players fields, not all fields have to be used by any command and better not be exposed 
	union {
		// SET_SHIP_LOC_CLIENT
		struct {
			ShipClass       ShipType;
			FieldCoordinate ShipsPosition;
			ShipRotation    Rotation;
		} SetShipLocation;

		// SHOOT_CELL_CLIENT
		FieldCoordinate ShootCellLocation;

		// STARTUP_FIELDSIZE
		FieldDimension GameFieldSizes;


		// CELL_STATE_SERVER
		struct {
			FieldCoordinate Coordinates;
			CellState       State;
		} CellStateUpdate;

		// RAISE_STATUS_MESSAGE
		ShipControlStatus ShipControlRaisedStatus;
	};
};

export spdlogger GameLog;

export class PlayerField {
	friend class GameManagmentController;
public:
	PlayerField(
		const FieldDimension FieldSizes,
		const ShipCount& NumberOfShips
	)
		: FieldDimensions(FieldSizes),
		NumberOfShipsPerType(NumberOfShips) {
		TRACE_FUNTION_PROTO;

		auto NumberOfCells = FieldDimensions.XDimension * FieldDimensions.YDimension;
		FieldCellStates = new CellState[NumberOfCells];
		memset(FieldCellStates,
			CELL_IS_EMPTY,
			NumberOfCells);
		NumberOfShipsPerType = NumberOfShips;
		SPDLOG_LOGGER_INFO(GameLog, "Allocated and initialized internal gamefield lookup with parameters: {{{}:{}}}",
			FieldDimensions.XDimension, FieldDimensions.YDimension);
	}
	~PlayerField() {
		TRACE_FUNTION_PROTO;

		delete FieldCellStates;
		SPDLOG_LOGGER_INFO(GameLog, "Playerfield [{}:{}] destroyed, cleaning up state",
			(void*)this, (void*)FieldCellStates);
	}

	long PlaceShipSecureCheckInterference(
		ShipClass       ShipType,
		ShipRotation    ShipOrientation,
		FieldCoordinate ShipCoordinates
	) {
		TRACE_FUNTION_PROTO;

		// Check if all Slots have been occupied
		if (FieldShipStates.size() >= NumberOfShipsPerType.GetTotalCount())
			return SPDLOG_LOGGER_ERROR(GameLog, "No available slots to place ship, max: {} ships",
				NumberOfShipsPerType.GetTotalCount()), -1;

		// Walk OccupationTable and check if slot for specific ship type is available
		ShipCount CounterWeight;
		for (auto& ShipEntry : FieldShipStates)
			if (ShipEntry.ShipType == ShipType)
				if (++CounterWeight.ShipCounts[ShipEntry.ShipType] >= NumberOfShipsPerType.ShipCounts[ShipType])
					return SPDLOG_LOGGER_ERROR(GameLog, "Conflicting ship category, all ships of type already in use"), -2;

		// Check if the ship even fits within the field at its cords
		ShipState RemoteShipState{
			.ShipType = ShipType,
			.Cordinates = ShipCoordinates,
			.Rotation = ShipOrientation
		};
		auto ShipEndPosition = CalculateCordinatesOfPartByDistanceWithShip(
			RemoteShipState,
			ShipLengthPerType[RemoteShipState.ShipType]);
		if (RemoteShipState.Cordinates.XCord > FieldDimensions.XDimension ||
			RemoteShipState.Cordinates.YCord > FieldDimensions.YDimension ||
			ShipEndPosition.XCord > FieldDimensions.XDimension ||
			ShipEndPosition.YCord > FieldDimensions.YDimension)
			return SPDLOG_LOGGER_ERROR(GameLog, "{{{}:{}}} is no within the specified field size of {{{}:{}}}",
				RemoteShipState.Cordinates.XCord, RemoteShipState.Cordinates.YCord,
				FieldDimensions.XDimension, FieldDimensions.YDimension), -3;

		// Validate ship placement, check for collisions and illegal positioning
		for (auto i = 0; i < ShipLengthPerType[RemoteShipState.ShipType]; ++i) {

			// This loop iterates through all cell positions of ship parts,
			// IteratorPosition is always valid as its within the field and has been validated previously
			auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
				RemoteShipState, i);

			// Get cellstate at X:Y
			if (GetCellReferenceByCoordinates(IteratorPosition) != CELL_IS_EMPTY)
				return SPDLOG_LOGGER_ERROR(GameLog, "Ship collides with another ship in {{{}:{}}}",
					IteratorPosition.XCord, IteratorPosition.YCord), -4; // collision detected

			for (auto y = -1; y < 1; ++y)
				for (auto x = -1; x < 1; ++x) {

					// Check for illegal placement and skip invalid addresses outside of frame
					FieldCoordinate LookupLocation{
						.XCord = (uint8_t)(IteratorPosition.XCord + x),
						.YCord = (uint8_t)(IteratorPosition.YCord + y),
					};
					if (LookupLocation.XCord > FieldDimensions.XDimension ||
						LookupLocation.YCord > FieldDimensions.YDimension)
						continue;
					if (GetCellReferenceByCoordinates(LookupLocation) != CELL_IS_EMPTY)
						return SPDLOG_LOGGER_ERROR(GameLog, "Ship is in an illegal position conflicting with {{{}:{}}}",
							LookupLocation.XCord, LookupLocation.YCord), -5; // illegal placement detected
				}
		}

		// All validations have been performed, we can now commit changes and add place the ship on the field
		for (auto i = 0; i < ShipLengthPerType[RemoteShipState.ShipType]; ++i) {

			// This loop iterates through all cell positions of ship parts and writes the state to the grid
			auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
				RemoteShipState, i);
			GetCellReferenceByCoordinates(IteratorPosition) = CELL_IS_IN_USE;
		}

		// Finally add the Ship to our list;
		FieldShipStates.push_back(RemoteShipState);
		SPDLOG_LOGGER_INFO(GameLog, "Registered ship in list at location {{{}:{}}}",
			RemoteShipState.Cordinates.XCord, RemoteShipState.Cordinates.YCord);
	}

	ShipState* GetShipEntryForCordinate(
		FieldCoordinate Cordinates
	) {
		TRACE_FUNTION_PROTO;

		// Enumerate all current valid ships and test all calculated cords of their parts against requested cords
		for (auto& ShipEntry : FieldShipStates) {

			// Iterate over all parts of the the ShipEntry
			for (auto i = 0; i < ShipLengthPerType[ShipEntry.ShipType]; ++i) {

				// Calculate part location and check against given coordinates
				auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
					ShipEntry, i);
				if (IteratorPosition.XCord == Cordinates.XCord &&
					IteratorPosition.YCord == Cordinates.YCord)
					return SPDLOG_LOGGER_INFO(GameLog, "Found ship at {{{}:{}}}",
						IteratorPosition.XCord, IteratorPosition.YCord), & ShipEntry;
			}
		}

		SPDLOG_LOGGER_WARN(GameLog, "Failed to find an associated ship for location {{{}:{}}}",
			Cordinates.XCord, Cordinates.YCord);
		return nullptr;
	}

	CellState StrikeCellAndUpdateShipList(
		FieldCoordinate Coordinates
	) {
		TRACE_FUNTION_PROTO;

		// Test and shoot cell in grid
		auto& LocalCell = GetCellReferenceByCoordinates(Coordinates);
		if (LocalCell & CELL_PROBE_WAS_SHOT)
			return CELL_WAS_ALREADY_SHOT;
		LocalCell &= CELL_SHOOT_MERGE_VALUE;

		// Check if Cell is within a ship, in which case the we Locate the ship and Check for the destruction of the ship
		if (!LocalCell & CELL_PROBE_USED)
			return LocalCell;
		auto& ShipEntry = *GetShipEntryForCordinate(Coordinates); // the function is able to return nullptr,
																  // if it does we know that the game state is corrupt, 
																  // in which case we can just crash, it doesnt matter anymore
		for (auto i = 0; i < ShipLengthPerType[ShipEntry.ShipType]; ++i) {

			// Enumerate all cells and test for non destroyed cell
			auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
				ShipEntry, i);
			if (GetCellReferenceByCoordinates(IteratorPosition) != CELL_WAS_SHOT_IN_USE)
				return SPDLOG_LOGGER_INFO(GameLog, "Ship at {{{}:{}}} was succesfully hit",
					ShipEntry.Cordinates.XCord, ShipEntry.Cordinates.YCord), LocalCell;
		}

		SPDLOG_LOGGER_INFO(GameLog, "Ship at {{{}:{}}} was sunken",
			ShipEntry.Cordinates.XCord, ShipEntry.Cordinates.YCord);
		return Cell_WAS_DESTRUCTOR;
	}

private:
	FieldCoordinate CalculateCordinatesOfPartByDistanceWithShip(
		const ShipState& BaseShipData,
		uint8_t    DistanceToWalk
	) const {
		TRACE_FUNTION_PROTO;

		return {
				.XCord = (uint8_t)(BaseShipData.Cordinates.XCord
					+ ((DistanceToWalk * (-1 * (BaseShipData.Rotation >> 1)))
						* (BaseShipData.Rotation % 2)) - 1),
				.YCord = (uint8_t)(BaseShipData.Cordinates.YCord
					+ ((DistanceToWalk * (-1 * (BaseShipData.Rotation >> 1)))
						* ((BaseShipData.Rotation + 1) % 2)) - 1)
		};
	}

	CellState& GetCellReferenceByCoordinates(
		const FieldCoordinate Cordinates
	) const {
		TRACE_FUNTION_PROTO;

		return FieldCellStates[Cordinates.YCord * FieldDimensions.XDimension + Cordinates.XCord];
	}

	FieldDimension FieldDimensions;
	CellState* FieldCellStates;
	SOCKET         PlayerAssociation = INVALID_SOCKET;

	ShipCount         NumberOfShipsPerType;
	vector<ShipState> FieldShipStates;
};



export class SocketWrap {
public:
	SocketWrap() {
		TRACE_FUNTION_PROTO;

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
	~SocketWrap() {
		TRACE_FUNTION_PROTO;

		if (--RefrenceCounter == 0)
			WSACleanup();
	}

	static inline atomic<uint32_t> RefrenceCounter = 0;
};
