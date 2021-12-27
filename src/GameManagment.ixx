// This file implements the server sided game manager,
// it controls the entire game state and implements the
// game logic as well as game rules
module;

#include "BattleShip.h"
#include <memory>
#include <map>

export module GameManagment;
import LayerBase;
using namespace std;
export SpdLogger GameLog;


//
// TODO: this has to be partially rewritten / ported for shared usage
// and Server::ManagmentDispatchRoutine has to be addapted to the new layout
//


// Shared game manager namespace
export namespace GameManagment {
	using namespace Network;

	// Primary game state referring to a unit field, each player is associated by handle (here by socket handle as id),
	// this offers the primary interface to interact with a players current game state
	class GmPlayerField {
		friend class GameManager;
	public:
		enum PlayFieldStatus {
			STATUS_INVALID_PLACEMENT = -2000,
			STATUS_OUT_OF_BOUNDS,
			STATUS_DIRECT_COLLIDE,
			STATUS_INDIRECT_COLLIDE,
			STATUS_NOT_ENOUGH_SLOTS,
			
			STATUS_OK = 0,
			STATUS_NO_COLLISION,
			STATUS_SHIP_PLACED,
		};
		using ProbeStatusList = vector<pair<PointComponent, PlayFieldStatus>>;


		GmPlayerField(
			      SOCKET         AssociatedSocket,
			      PointComponent FieldSizes,
			const ShipCount&     NumberOfShips
		)
			: FieldDimensions(FieldSizes),
			  NumberOfShipsPerType(NumberOfShips),
			  PlayerAssociation(AssociatedSocket) {
			TRACE_FUNTION_PROTO;

			auto NumberOfCells = FieldDimensions.XComponent * FieldDimensions.YComponent;
			FieldCellStates = make_unique<CellState[]>(NumberOfCells);
			memset(FieldCellStates.get(),
				CELL_IS_EMPTY,
				NumberOfCells);
			SPDLOG_LOGGER_INFO(GameLog, "Allocated and initialized internal gamefield lookup with parameters: {{{}:{}}}",
				FieldDimensions.XComponent, FieldDimensions.YComponent);
		}
		~GmPlayerField() {
			TRACE_FUNTION_PROTO;

			SPDLOG_LOGGER_INFO(GameLog, "Playerfield [{}:{}] destroyed, cleaning up state",
				(void*)this, (void*)FieldCellStates.get());
		}


		ProbeStatusList ProbeShipPlacement( // Probes the passed parameters of a possible requested ship placement
		                                    // and validates them if they are possible or interfere with other
		                                    // objects or the field area itself and returns a list of status location pairs based on that
			ShipClass      TypeOfShip,
			ShipRotation   ShipOrientation,
			PointComponent ShipCoordinates
		) {
			TRACE_FUNTION_PROTO;

			// Walk OccupationTable and check if slot for specific ship type is available
			ProbeStatusList ProbeList;
			for (auto i = 0; auto& ShipEntry : FieldShipStates)
				if (ShipEntry.ShipType == TypeOfShip)
					if (++i >= NumberOfShipsPerType.ShipCounts[TypeOfShip])
						return ProbeList.emplace_back(ProbeStatusList::value_type{ {0, 0}, STATUS_NOT_ENOUGH_SLOTS }),
							SPDLOG_LOGGER_ERROR(GameLog, "Conflicting ship category, all ships of type already in use"),
							ProbeList;

			// Check if the ship even fits within the field at its cords
			auto ShipEndPosition = CalculateCordinatesOfPartByDistanceWithShip(
				ShipCoordinates, ShipOrientation,
				ShipLengthPerType[TypeOfShip]);
			if (ShipCoordinates.XComponent > FieldDimensions.XComponent ||
				ShipCoordinates.YComponent > FieldDimensions.YComponent)
				ProbeList.emplace_back(ProbeStatusList::value_type{ ShipCoordinates, STATUS_OUT_OF_BOUNDS });
			if (ShipEndPosition.XComponent > FieldDimensions.XComponent ||
				ShipEndPosition.YComponent > FieldDimensions.YComponent)
				ProbeList.emplace_back(ProbeStatusList::value_type{ ShipEndPosition, STATUS_OUT_OF_BOUNDS });
			if (ProbeList.size())
				return SPDLOG_LOGGER_WARN(GameLog, "{{{}:{}}} and {{{}:{}}} may not be within allocated field",
					ShipCoordinates.XComponent, ShipCoordinates.YComponent,
					ShipEndPosition.XComponent, ShipEndPosition.YComponent),
					ProbeList;

			// Validate ship placement, check for collisions and illegal positioning
			// This loop iterates through all cell positions of ship parts,
			for (auto i = 0; i < ShipLengthPerType[TypeOfShip]; ++i) {

				// IteratorPosition is always valid as its within the field and has been validated previously
				auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
					ShipCoordinates, ShipOrientation, i);

				// This 2 dimensional loop evaluates all associated cells
				for (auto y = -1; y < 1; ++y)
					for (auto x = -1; x < 1; ++x) {

						// Check for illegal placement and skip invalid addresses outside of frame
						PointComponent LookupLocation{ IteratorPosition.XComponent + x,
							IteratorPosition.YComponent + y };

						// Filter out collision checks outside the field, assumes unsigned types
						if (LookupLocation.XComponent > FieldDimensions.XComponent ||
							LookupLocation.YComponent > FieldDimensions.YComponent)
							continue;

						// Lookup location and apply test, construct collision list from there
						if (GetCellReferenceByCoordinates(LookupLocation) != CELL_IS_EMPTY) {

							bool NotReported = true;
							for (auto& [ReportedLocation, StatusMessage] : ProbeList)
								if (LookupLocation == ReportedLocation) {
									NotReported = false; break;
								}

							if (NotReported)
								ProbeList.emplace_back(ProbeStatusList::value_type{ LookupLocation,
									LookupLocation == IteratorPosition ? STATUS_DIRECT_COLLIDE : STATUS_INDIRECT_COLLIDE }),
								SPDLOG_LOGGER_WARN(GameLog, "Reported {} at {{{}:{}}}",
									LookupLocation == IteratorPosition ? "collision" : "touching",
									LookupLocation.YComponent, FieldDimensions.YComponent);
						}
					}
			}

			return ProbeList;
		}

		void PlaceShipBypassSecurityChecks( // This allocates a shipstate and enlists it in the fieldstate
		                                    // and commits the changes to the cell grid array with no checks
			ShipClass      TypeOfShip,
			ShipRotation   ShipOrientation,
			PointComponent ShipCoordinates
		) {
			TRACE_FUNTION_PROTO;

			// Commit changes and place the ship on the field updating the cells
			for (auto i = 0; i < ShipLengthPerType[TypeOfShip]; ++i) {

				// This loop iterates through all cell positions of ship parts and writes the state to the grid
				auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
					ShipCoordinates, ShipOrientation, i);
				GetCellReferenceByCoordinates(IteratorPosition) = CELL_IS_IN_USE;
			}

			// Finally add the ship to our list;
			FieldShipStates.push_back(ShipState{
				.ShipType = TypeOfShip,
				.Cordinates = ShipCoordinates,
				.Rotation = ShipOrientation
				});
			SPDLOG_LOGGER_INFO(GameLog, "Registered ship in list at location {{{}:{}}}",
				ShipCoordinates.XComponent, ShipCoordinates.YComponent);
		}

		PlayFieldStatus PlaceShipSecureCheckInterference( // Tests and commits the changes to the game field,
		                                                  // note for @DanielM:
		                                                  // you shouldnt use this function but rather do it like it does, this gives finer control
			ShipClass      ShipType,
			ShipRotation   ShipOrientation,
			PointComponent ShipCoordinates
		) {
			TRACE_FUNTION_PROTO;

			auto ProbeResultList = ProbeShipPlacement(ShipType,
				ShipOrientation,
				ShipCoordinates);
			if (ProbeResultList.size())
				return SPDLOG_LOGGER_ERROR(GameLog, "An attempt to place a ship at an invalid location was made"),
					STATUS_INVALID_PLACEMENT;

			PlaceShipBypassSecurityChecks(ShipType,
				ShipOrientation,
				ShipCoordinates);
			return STATUS_SHIP_PLACED;
		}


		ShipState* GetShipEntryForCordinate( // Searches the shipstate list for an entry that contains a ship colliding with said coords
			PointComponent Cordinates
		) {
			TRACE_FUNTION_PROTO;

			// Enumerate all current valid ships and test all calculated cords of their parts against requested cords
			for (auto& ShipEntry : FieldShipStates) {

				// Iterate over all parts of the the ShipEntry
				for (auto i = 0; i < ShipLengthPerType[ShipEntry.ShipType]; ++i) {

					// Calculate part location and check against given coordinates
					auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
						ShipEntry, i);
					if (IteratorPosition.XComponent == Cordinates.XComponent &&
						IteratorPosition.YComponent == Cordinates.YComponent)
						return SPDLOG_LOGGER_INFO(GameLog, "Found ship at {{{}:{}}}",
							IteratorPosition.XComponent, IteratorPosition.YComponent), 
							&ShipEntry;
				}
			}

			SPDLOG_LOGGER_WARN(GameLog, "Failed to find an associated ship for location {{{}:{}}}",
				Cordinates.XComponent, Cordinates.YComponent);
			return nullptr;
		}

		CellState StrikeCellAndUpdateShipList( // Tries to "shoot" cell and applies handling,
		                                       // This will update the grid array and the shipstate list
			PointComponent TargetCoordinates
		) {
			TRACE_FUNTION_PROTO;

			// Test and shoot cell in grid
			using Network::CellState;
			auto& LocalCell = GetCellReferenceByCoordinates(TargetCoordinates);
			if (LocalCell & PROBE_CELL_WAS_SHOT)
				return STATUS_WAS_ALREADY_SHOT;
			LocalCell &= ASSING_SHOOT_MERGE_VALUE;

			// Check if Cell is within a ship, in which case the we Locate the ship and Check for the destruction of the ship
			if (!LocalCell & PROBE_CELL_USED)
				return LocalCell;

			auto ShipEntry = GetShipEntryForCordinate(TargetCoordinates);
			for (auto i = 0; i < ShipLengthPerType[ShipEntry->ShipType]; ++i) {

				// Enumerate all cells and test for non destroyed cell
				auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
					*ShipEntry, i);
				if (GetCellReferenceByCoordinates(IteratorPosition) != CELL_WAS_SHOT_IN_USE)
					return SPDLOG_LOGGER_INFO(GameLog, "Ship at {{{}:{}}} was succesfully hit",
						ShipEntry->Cordinates.XComponent, ShipEntry->Cordinates.YComponent),
					LocalCell;
			}

			SPDLOG_LOGGER_INFO(GameLog, "Ship at {{{}:{}}} was sunken",
				ShipEntry->Cordinates.XComponent, ShipEntry->Cordinates.YComponent);
			return STATUS_WAS_DESTRUCTOR;
		}



		const ShipCount& GetInternalShipNumbersReference() const {
			return NumberOfShipsPerType;
		}
		uint8_t GetNumberOfShipsPlaced() const {
			return FieldShipStates.size();
		}



	private:
		PointComponent CalculateCordinatesOfPartByDistanceWithShip(
			PointComponent ShipLocation,
			ShipRotation   ShipOrientation,
			uint8_t        DistanceToWalk
		) const {
			TRACE_FUNTION_PROTO;

			return {
					.XComponent = (uint8_t)(ShipLocation.XComponent
						+ ((DistanceToWalk * (-1 * (ShipOrientation >> 1)))
							* (ShipOrientation % 2)) - 1),
					.YComponent = (uint8_t)(ShipLocation.YComponent
						+ ((DistanceToWalk * (-1 * (ShipOrientation >> 1)))
							* ((ShipOrientation + 1) % 2)) - 1)
			};
		}
		PointComponent CalculateCordinatesOfPartByDistanceWithShip(
			const ShipState& BaseShipData,
			      uint8_t    DistanceToWalk
		) const {
			TRACE_FUNTION_PROTO;

			return CalculateCordinatesOfPartByDistanceWithShip(BaseShipData.Cordinates,
				BaseShipData.Rotation,
				DistanceToWalk);
		}

		CellState& GetCellReferenceByCoordinates(
			const PointComponent Cordinates
		) const {
			TRACE_FUNTION_PROTO;

			return FieldCellStates[Cordinates.YComponent * FieldDimensions.XComponent + Cordinates.XComponent];
		}

		SOCKET                  PlayerAssociation;
		unique_ptr<CellState[]> FieldCellStates;
		PointComponent          FieldDimensions;

		vector<ShipState> FieldShipStates;
		ShipCount         NumberOfShipsPerType;
	};



	// Helper structures
	struct PlayerMetaData {
		
		bool ReadyUped;

	};
	enum GamePhase {
		INVALID_PHASE = -1,
		SETUP_PHASE,
		GAME_PHASE,
		GAMEEND_PHASE
	};



	// The primary interface, this gives full control of the game and forwards other functionality
	class GameManager2 
		: public MagicInstanceManagerBase<GameManager2>,
		  private WsaNetworkBase {
		friend class MagicInstanceManagerBase<GameManager2>;
	public:
		enum GameManagerStatus {
			STATUS_INVALID = -2000,
			STATUS_PLAYERS_NOT_READY,
			STATUS_NOT_ALL_PLACED,

			STATUS_OK = 0,
			STATUS_SWITCHED_GAME_PHASE,

		};


		~GameManager2() {
			TRACE_FUNTION_PROTO;

			SPDLOG_LOGGER_INFO(GameLog, "game manager destroyed, cleaning up assocs");
		}


		GmPlayerField* TryAllocatePlayerWithId( // Tries to allocate a player, and returns it,
		                                        // if there are too many players returns nullptr,
		                                        // throws runtime error if emplace fails (should be impossible)
			SOCKET SocketAsId
		) {
			TRACE_FUNTION_PROTO;

			if (CurrentPlayersRegistered >= 2)
				return SPDLOG_LOGGER_WARN(GameLog, "Cannot allocate more Players, there are already 2 present"), nullptr;

			auto [FieldIterator, Inserted] = PlayerFieldData.try_emplace(SocketAsId,
				SocketAsId,
				InternalFieldDimensions,
				InternalShipCount);
			if (!Inserted)
				throw (SPDLOG_LOGGER_ERROR(GameLog, "failed to insert player field into controller"),
					runtime_error("failed to insert player field into controller"));

			SPDLOG_LOGGER_INFO(GameLog, "Allocated player for socket [{:04x}]",
				SocketAsId);
			return &FieldIterator->second;
		}

		uint8_t GetCurrentPlayerCount() {
			TRACE_FUNTION_PROTO;

			return PlayerFieldData.size();
		}

		pair<GmPlayerField&, GmPlayerField&> // returns a pair of both players fields,
		GetFieldControllerPairById(          // return->first contains the player field of the searched id and
		                                     // return->second the oponents players	                                     
			SOCKET SocketAsPlayerId
		) {
			TRACE_FUNTION_PROTO;
		
			return { PlayerFieldData.begin()->second,
				PlayerFieldData.end()->second };
		}


		GamePhase GetCurrentGamePhase() {
			TRACE_FUNTION_PROTO;

			return CurrentGameState;
		}

		GameManagerStatus CheckReadyAndStartGame() { // Checks if everything is valid and the game has been properly setup
			TRACE_FUNTION_PROTO;

			// Check if all players are ready to play
			if (NumberOfReadyPlayers != 2)
				return SPDLOG_LOGGER_WARN(GameLog, "Not all players are ready'd up"),
					STATUS_PLAYERS_NOT_READY;

			// Check if players have all their ships placed
			for (const auto& [Key, PlayerFieldController] : PlayerFieldData)
				if (PlayerFieldController.GetInternalShipNumbersReference().GetTotalCount() <
					PlayerFieldController.GetNumberOfShipsPlaced())
					return SPDLOG_LOGGER_WARN(GameLog, "Not all ships of Player [{:04x}] have been placed",
						Key), STATUS_NOT_ALL_PLACED;

			// Switch game phase state and notify caller
			CurrentGameState = GAME_PHASE;
			SPDLOG_LOGGER_INFO(GameLog, "Game is setup, switched to game phase state");
			return STATUS_SWITCHED_GAME_PHASE;
		}
		
		// Game parameters, used for allocating players
		const PointComponent InternalFieldDimensions;
		const ShipCount      InternalShipCount;

	private:
		GameManager2(
			      PointComponent FieldSizes,
			const ShipCount&     NumberOfShips
		) 
			: InternalFieldDimensions(FieldSizes),
			  InternalShipCount(NumberOfShips) {
			TRACE_FUNTION_PROTO;

			SPDLOG_LOGGER_INFO(GameLog, "GameManager instance created {}",
				(void*)this);
		}


		map<SOCKET, GmPlayerField> PlayerFieldData;

		uint8_t   NumberOfReadyPlayers = 0;
		uint8_t   CurrentPlayersRegistered = 0;
		uint8_t   CurrentPlayersTurnIndex = 0;
		GamePhase CurrentGameState = SETUP_PHASE;
	};
}
