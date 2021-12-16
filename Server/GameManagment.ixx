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
public:
	PlayerField(
		      uint8_t    XDimension,
		      uint8_t    YDimension,
		const ShipCount& NumberOfShips
	) 
		: NumberOfShipsPerType(NumberOfShips) {

		FieldDimensions = { XDimension, YDimension };
		FieldCellStates = new CellState[XDimension * YDimension];
		memset(FieldCellStates,
			CELL_IS_EMPTY,
			XDimension * YDimension);
		NumberOfShipsPerType = NumberOfShips;
		GameLog->info("Allocated and initialized internal gamefield lookup with parameters: {{{}:{}}}", 
			XDimension, YDimension);
	}
	~PlayerField() {
		delete FieldCellStates;
		GameLog->info("Playerfield [{}:{}] destroyed, cleaning up state",
			(void*)this, (void*)FieldCellStates);
	}

	long PlaceShip(
		const ShipState& RemoteShipState
	) {
		// Check if all Slots have been occupied
		if (FieldShipStates.size() >= NumberOfShipsPerType.GetTotalCount())
			return GameLog->error("No available slots to place ship, max: {} ships",
				NumberOfShipsPerType.GetTotalCount()), -1;

		// Walk OccupationTable and check if slot for specific ship type is available
		ShipCount CounterWeight;
		for (auto& ShipEntry : FieldShipStates)
			if (ShipEntry.ShipType == RemoteShipState.ShipType)
				if (++CounterWeight.ShipCounts[ShipEntry.ShipType] >= NumberOfShipsPerType.ShipCounts[RemoteShipState.ShipType])
					return GameLog->error("Conflicting ship category, all ships of type already in use"), -2;

		// check if the ship even fits within the field at its cords
		auto ShipEndPosition = CalculateCordinatesOfPartByDistanceWithShip(
			RemoteShipState,
			ShipLengthPerType[RemoteShipState.ShipType]);
		if (RemoteShipState.Cordinates.XCord > FieldDimensions.XDimension ||
			RemoteShipState.Cordinates.YCord > FieldDimensions.YDimension ||
			ShipEndPosition.XCord > FieldDimensions.XDimension ||
			ShipEndPosition.YCord > FieldDimensions.YDimension)
			return GameLog->error("{{{}:{}}} is no within the specified field size of {{{}:{}}}",
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
				return GameLog->error("Ship collides with another ship in {{{}:{}}}",
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
						return GameLog->error("Ship is in an illegal position conflicting with {{{}:{}}}",
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
		GameLog->info("Registered ship in list at location {{{}:{}}}",
			RemoteShipState.Cordinates.XCord, RemoteShipState.Cordinates.YCord);
	}

	ShipState* GetShipEntryForCordinate(
		FieldCoordinate Cordinates
	) {
		// Enumerate all current valid ships and test all calculated cords of their parts against requested cords
		for (auto& ShipEntry : FieldShipStates) {

			// Iterate over all parts of the the ShipEntry
			for (auto i = 0; i < ShipLengthPerType[ShipEntry.ShipType]; ++i) {

				// Calculate part location and check against given coordinates
				auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
					ShipEntry, i);
				if (IteratorPosition.XCord == Cordinates.XCord &&
					IteratorPosition.YCord == Cordinates.YCord)
					return GameLog->info("Found ship at {{{}:{}}}",
						IteratorPosition.XCord, IteratorPosition.YCord), &ShipEntry;
			}
		}

		GameLog->warn("Failed to find an associated ship for location {{{}:{}}}",
			Cordinates.XCord, Cordinates.YCord);
		return nullptr;
	}

	CellState StrikeCellAndUpdateShipList(
		FieldCoordinate Coordinates
	) {
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
				return GameLog->info("Ship at {{{}:{}}} was succesfully hit",
					ShipEntry.Cordinates.XCord, ShipEntry.Cordinates.YCord), LocalCell;
		}

		GameLog->info("Ship at {{{}:{}}} was sunken",
			ShipEntry.Cordinates.XCord, ShipEntry.Cordinates.YCord);
		return Cell_WAS_DESTRUCTOR;
	}

private:
	FieldCoordinate CalculateCordinatesOfPartByDistanceWithShip(
		const ShipState& BaseShipData,
		      uint8_t    DistanceToWalk
	) const {
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
		return FieldCellStates[Cordinates.YCord * FieldDimensions.XDimension + Cordinates.XCord];
	}

	FieldDimension FieldDimensions;
	CellState*                   FieldCellStates;

	ShipCount         NumberOfShipsPerType;
	vector<ShipState> FieldShipStates;
};


export class GameManagmentController {
public:
	static GameManagmentController* CreateSingletonOverride(
		uint8_t          XDimension,
		uint8_t          YDimension,
		const ShipCount& NumberOfShips
	) {		
		// Magic fuckery cause make_unique cannot normally access a private constructor
		struct EnableMakeUnique : public GameManagmentController {
			inline EnableMakeUnique(
				uint8_t          XDimension,
				uint8_t          YDimension,
				const ShipCount& NumberOfShips
			)
				: GameManagmentController(
					XDimension,
					YDimension,
					NumberOfShips) {}
		};

		InstanceObject = make_unique<EnableMakeUnique>(
			XDimension,
			YDimension,
			NumberOfShips);
		GameLog->info("Factory created game managment object at {}",
			(void*)InstanceObject.get());
		return InstanceObject.get();
	}
	static GameManagmentController* GetInstance() {
		return InstanceObject.get();
	}

	~GameManagmentController() {
		GameLog->info("game manager destroyed, cleaning up assocs");		
	}
	GameManagmentController(
		const GameManagmentController&) = delete;
	GameManagmentController& operator=(
		const GameManagmentController&) = delete;

private:
	GameManagmentController(
		uint8_t          XDimension,
		uint8_t          YDimension,
		const ShipCount& NumberOfShips
	) 
		: RemotePlayers{ { XDimension, YDimension, NumberOfShips },
	                     { XDimension, YDimension, NumberOfShips } } {}
	PlayerField RemotePlayers[2];

	static inline unique_ptr<GameManagmentController> InstanceObject;
};
