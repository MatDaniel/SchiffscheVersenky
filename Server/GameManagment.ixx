// This file implements the server sided game manager,
// it controls the entire gamestate and implements the
// gamelogic as well as gamerules
module;

#include <SharedLegacy.h>
#include <memory>

export module GameManagment;
import ShipSock;
using namespace std;



export spdlogger GameLog;

export class PlayerField {
	friend class GameManagmentController;
public:
	PlayerField(
		const FieldDimension FieldSizes,
		const ShipCount&     NumberOfShips
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
						IteratorPosition.XCord, IteratorPosition.YCord), &ShipEntry;
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
	CellState*     FieldCellStates;
	SOCKET         PlayerAssociation = INVALID_SOCKET;

	ShipCount         NumberOfShipsPerType;
	vector<ShipState> FieldShipStates;
};

export struct PlayerState {
	bool ReadyUped;

};

export enum GamePhase {
	INVALID_PHASE = -1,
	SETUP_PHASE,
	GAME_PHASE,
	GAMEEND_PHASE
};



export class GameManagmentController {
public:
	static GameManagmentController* CreateSingletonOverride(
		const FieldDimension FieldSizes,
		const ShipCount&     NumberOfShips
	) {		
		TRACE_FUNTION_PROTO;

		// Magic fuckery cause make_unique cannot normally access a private constructor
		struct EnableMakeUnique : public GameManagmentController {
			inline EnableMakeUnique(
				const FieldDimension FieldSizes,
				const ShipCount&     NumberOfShips
			)
				: GameManagmentController(
					FieldSizes,
					NumberOfShips) {}
		};

		InstanceObject = make_unique<EnableMakeUnique>(
			FieldSizes,
			NumberOfShips);
		SPDLOG_LOGGER_INFO(GameLog, "Factory created game managment object at {}",
			(void*)InstanceObject.get());
		return InstanceObject.get();
	}
	static GameManagmentController* GetInstance() {
		TRACE_FUNTION_PROTO;

		return InstanceObject.get();
	}

	~GameManagmentController() {
		TRACE_FUNTION_PROTO;

		SPDLOG_LOGGER_INFO(GameLog, "game manager destroyed, cleaning up assocs");
	}
	GameManagmentController(
		const GameManagmentController&) = delete;
	GameManagmentController& operator=(
		const GameManagmentController&) = delete;



	uint8_t GetCurrentPlayerCount() {
		TRACE_FUNTION_PROTO;

		return CurrentPlayersRegistered;
	}
	PlayerField* AllocattePlayerWithId(
		SOCKET SocketAsId
	) {
		TRACE_FUNTION_PROTO;

		if (CurrentPlayersRegistered >= 2)
			return SPDLOG_LOGGER_WARN(GameLog, "Cannot allocate more Players, there are already 2 present"),
				nullptr;

		RemotePlayersField[CurrentPlayersRegistered].PlayerAssociation = SocketAsId;
		++CurrentPlayersRegistered;
		SPDLOG_LOGGER_INFO(GameLog, "Allocated Player to socket [{:04x}]",
			SocketAsId);
		return &RemotePlayersField[CurrentPlayersRegistered - 1];
	}

	PlayerField* GetPlayerFieldControllerById(
		SOCKET        SocketAsPlayerId,
		PlayerField** OtherPlayerField = nullptr
	) {
		TRACE_FUNTION_PROTO;

		auto PlayerIndex = GetPlayerIndexById(SocketAsPlayerId);
		if (PlayerIndex < 0)
			return nullptr;
		
		if (OtherPlayerField)
			*OtherPlayerField = &RemotePlayersField[PlayerIndex ^ 1];
		return &RemotePlayersField[PlayerIndex];
	}
	PlayerState* GetPlayerStateById(
		SOCKET        SocketAsPlayerId,
		PlayerState** OtherPlayerState = nullptr
	) {
		TRACE_FUNTION_PROTO;

		auto PlayerIndex = GetPlayerIndexById(SocketAsPlayerId);
		if (PlayerIndex < 0)
			return nullptr;

		if (OtherPlayerState)
			*OtherPlayerState = &RemotePlayerState[PlayerIndex ^ 1];
		return &RemotePlayerState[PlayerIndex];
	}

	PlayerField* GetPlayerByTurn() {
		TRACE_FUNTION_PROTO;

		return &RemotePlayersField[CurrentPlayersTurnIndex];
	}
	
	GamePhase GetCurrentGamePhase() {
		TRACE_FUNTION_PROTO;

		return CurrentGameState;
	}

	FieldDimension GetFieldDimensions() {
		TRACE_FUNTION_PROTO;

		return CurrentFieldDimensions;
	}

private:
	int32_t GetPlayerIndexById(
		SOCKET SocketAsPlayerId
	) {
		TRACE_FUNTION_PROTO;

		for (auto i = 0; i < _countof(RemotePlayersField); ++i)
			if (RemotePlayersField[i].PlayerAssociation == SocketAsPlayerId)
				return i;
		return -1;
	}

	GameManagmentController(
		const FieldDimension FieldSizes,
		const ShipCount&     NumberOfShips
	) 
		: RemotePlayersField{ { FieldSizes, NumberOfShips },
			{ FieldSizes, NumberOfShips } },
		CurrentFieldDimensions(FieldSizes) {}

	// Primary game state
	PlayerField RemotePlayersField[2];
	PlayerState RemotePlayerState[2]{};

	// Non primary game state
	uint8_t        CurrentPlayersRegistered = 0;
	uint8_t        CurrentPlayersTurnIndex = 0;
	GamePhase      CurrentGameState = SETUP_PHASE;
	FieldDimension CurrentFieldDimensions;



	static inline unique_ptr<GameManagmentController> InstanceObject;
};
