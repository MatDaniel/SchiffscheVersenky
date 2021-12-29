// This file implements the server sided game manager,
// it controls the entire game state and implements the
// game logic as well as game rules
module;

#include "BattleShip.h"
#include <memory>
#include <map>
#include <span>
#include <string>

export module GameManagment;
import LayerBase;
using namespace std;
export SpdLogger GameLog;



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
		using StatusResponseT = pair<PointComponent, PlayFieldStatus>;
		using ProbeStatusList = span<StatusResponseT>;

		GmPlayerField(
			      PointComponent FieldSizes,
			const ShipCount&     NumberOfShips
		)
			: FieldDimensions(FieldSizes),
			  NumberOfShipsPerType(NumberOfShips) {
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

			// Check if we can utilize the cached test and reuse it (just has to match the location)
			if (InternalProbeListCached.CompareCacheMetaData(ShipCoordinates,
				TypeOfShip,
				ShipOrientation))
				return InternalProbeListCached;
			SPDLOG_LOGGER_DEBUG(GameLog, "Lookup cache invalid, rebuilding...");

			// Walk OccupationTable and check if slot for specific ship type is available
			for (auto i = 0; auto& ShipEntry : FieldShipStates)
				if (ShipEntry.ShipType == TypeOfShip)
					if (++i >= NumberOfShipsPerType.ShipCounts[TypeOfShip]) {

						InternalProbeListCached.emplace_back(StatusResponseT{ {}, STATUS_NOT_ENOUGH_SLOTS });
						InternalProbeListCached.SaveProbeState(ShipCoordinates,
							TypeOfShip,
							ShipOrientation);
						SPDLOG_LOGGER_ERROR(GameLog, "Conflicting ship category, all ships of type already in use");
						return InternalProbeListCached;
					}


			// Check if the ship even fits within the field at its cords
			auto ShipEndPosition = CalculateCordinatesOfPartByDistanceWithShip(
				ShipCoordinates, ShipOrientation,
				ShipLengthPerType[TypeOfShip] - 1);
			if (ShipCoordinates.XComponent >= FieldDimensions.XComponent ||
				ShipCoordinates.YComponent >= FieldDimensions.YComponent)
				InternalProbeListCached.emplace_back(StatusResponseT{ ShipCoordinates, STATUS_OUT_OF_BOUNDS });
			if (ShipEndPosition.XComponent >= FieldDimensions.XComponent ||
				ShipEndPosition.YComponent >= FieldDimensions.YComponent)
				InternalProbeListCached.emplace_back(StatusResponseT{ ShipEndPosition, STATUS_OUT_OF_BOUNDS });
			if (InternalProbeListCached.GetNumberOfCommits()) {
				
				SPDLOG_LOGGER_WARN(GameLog, "{{{}:{}}} and {{{}:{}}} may not be within allocated field",
					ShipCoordinates.XComponent, ShipCoordinates.YComponent,
					ShipEndPosition.XComponent, ShipEndPosition.YComponent);
				InternalProbeListCached.SaveProbeState(ShipCoordinates,
					TypeOfShip,
					ShipOrientation);
				return InternalProbeListCached;
			}

			// Validate ship placement, check for collisions and illegal positioning
			// This loop iterates through all cell positions of ship parts,
			vector<StatusResponseT> PrivateCollisionCommitList;
			for (auto i = 0; i < ShipLengthPerType[TypeOfShip]; ++i) {

				// IteratorPosition is always valid as its within the field and has been validated previously
				auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
					ShipCoordinates, ShipOrientation, i);

				// This 2 dimensional loop evaluates all associated cells
				for (auto y = -1; y < 2; ++y)
					for (auto x = -1; x < 2; ++x) {

						// Check for illegal placement and skip invalid addresses outside of frame
						PointComponent LookupLocation{ IteratorPosition.XComponent + x,
							IteratorPosition.YComponent + y };

						// Filter out collision checks outside the field, assumes unsigned types
						if (LookupLocation.XComponent >= FieldDimensions.XComponent ||
							LookupLocation.YComponent >= FieldDimensions.YComponent)
							continue;

						// Lookup location and apply test, construct collision list from there
						auto CellstateOfLookup = GetCellReferenceByCoordinates(LookupLocation);
						if (CellstateOfLookup & PROBE_CELL_USED) {

							// Assume which type of collision we are dealing with, this may not be final
							auto TypeOfCollision = LookupLocation == IteratorPosition 
								? STATUS_DIRECT_COLLIDE : STATUS_INDIRECT_COLLIDE;

							// Check if location has already been reported or updated to direct if falsely reported as indirect
							bool IsAlreadyReported = false;
							for (auto& [ReportedLocation, StatusMessage] : PrivateCollisionCommitList)
								if (ReportedLocation == LookupLocation) {
									if (ReportedLocation == IteratorPosition &&
										StatusMessage == STATUS_INDIRECT_COLLIDE) {
										StatusMessage = STATUS_DIRECT_COLLIDE;

										SPDLOG_LOGGER_WARN(GameLog, "Updated collision state of {{{}:{}}} to direct",
											ReportedLocation.XComponent, ReportedLocation.YComponent);
									}
									
									IsAlreadyReported = true; break;
								}
							if (!IsAlreadyReported)
								PrivateCollisionCommitList.emplace_back(StatusResponseT{ LookupLocation, TypeOfCollision }),
								SPDLOG_LOGGER_WARN(GameLog, "Reported {} at {{{}:{}}}",
									LookupLocation == IteratorPosition ? "collision" : "touching",
									LookupLocation.XComponent, LookupLocation.YComponent);
						}
					}
			}

			// Merge Collision list into status list and push to cache
			InternalProbeListCached.insert(InternalProbeListCached.end(),
				PrivateCollisionCommitList.begin(), PrivateCollisionCommitList.end());
			InternalProbeListCached.SaveProbeState(ShipCoordinates,
				TypeOfShip,
				ShipOrientation);
			return InternalProbeListCached;
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

			// Finally add the ship to our list, and invalidate our lookup cache
			FieldShipStates.push_back(ShipState{
				.ShipType = TypeOfShip,
				.Cordinates = ShipCoordinates,
				.Rotation = ShipOrientation
				});
			
			// We must also now invalidate the the probe cache as the field state has changed
			// probing the same location as previously where now is a ship could elsewise 
			// lead to collisions being missed
			InternalProbeListCached.ProbeInvalidate();
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
						return SPDLOG_LOGGER_INFO(GameLog, "Found ship at {{{}:{}}}, for {{{}:{}}}",
							ShipEntry.Cordinates.XComponent, ShipEntry.Cordinates.YComponent,
							Cordinates.XComponent, Cordinates.YComponent),
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
			using Network::CellState;
			TRACE_FUNTION_PROTO;

			// Test and shoot cell in grid
			auto& LocalCell = GetCellReferenceByCoordinates(TargetCoordinates);
			if (LocalCell & PROBE_CELL_WAS_SHOT)
				return SPDLOG_LOGGER_ERROR(GameLog, "Location {{{}:{}}} was struck multiple times",
					TargetCoordinates.XComponent, TargetCoordinates.YComponent),
				LocalCell |= STATUS_WAS_ALREADY_SHOT;
			LocalCell |= MERGE_SHOOT_CELL;

			// Check if cell is within a ship, in which case we locate the ship and check for the destruction of the ship
			if (!(LocalCell & PROBE_CELL_USED))
				return LocalCell;

			auto ShipEntry = GetShipEntryForCordinate(TargetCoordinates);
			for (auto i = 0; i < ShipLengthPerType[ShipEntry->ShipType]; ++i) {

				// Enumerate all cells and test for non destroyed cell
				auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
					*ShipEntry, i);
				if ((GetCellReferenceByCoordinates(IteratorPosition) & MASK_FILTER_STATE_BITS) != CELL_SHIP_WAS_HIT)
					return SPDLOG_LOGGER_INFO(GameLog, "Ship at {{{}:{}}} was hit in {{{}:{}}}",
						ShipEntry->Cordinates.XComponent, ShipEntry->Cordinates.YComponent,
						TargetCoordinates.XComponent, TargetCoordinates.YComponent),
					LocalCell;
			}

			SPDLOG_LOGGER_INFO(GameLog, "Ship at {{{}:{}}} was sunken",
				ShipEntry->Cordinates.XComponent, ShipEntry->Cordinates.YComponent);
			return LocalCell |= STATUS_WAS_DESTRUCTOR;
		}



		// TODO: Deuglify this class this is a mess but ehh....
		CellState GetCellStateByCoordinates(
			const PointComponent Cordinates
		) const {
			TRACE_FUNTION_PROTO; return GetCellReferenceByCoordinates(Cordinates);
		}


		span<const ShipState> GetShipStateList() const {
			TRACE_FUNTION_PROTO; return FieldShipStates;
		}


		// Quick Utility Helpers
		uint8_t GetNumberOfShipsPlaced() const {
			TRACE_FUNTION_PROTO; return FieldShipStates.size();
		}
		bool operator==(const GmPlayerField& Other) const {
			TRACE_FUNTION_PROTO; return this == &Other;
		}



		// Initilization constants
		const PointComponent    FieldDimensions;
		const ShipCount         NumberOfShipsPerType;

	private:
		class ProbeListCache 
			: public vector<StatusResponseT> {
			enum {
				CACHE_INVALID = 0,
				CACHE_VALID,
			} ValidationState{};

		public:
			bool CompareCacheMetaData(
				PointComponent ShipLocation,
				ShipClass      ShipType,
				ShipRotation   ShipOrientation
			) const {
				TRACE_FUNTION_PROTO;

				if (ValidationState == CACHE_INVALID)
					return false;

				// Specialization for treating multiple calls with a ship type with no free slots:
				if (ShipType == LastShipType &&              // Must be the same ship type as previously recorded
					size() > 0 &&                            // The probe cache must contain at least 1 entry
					at(0).second == STATUS_NOT_ENOUGH_SLOTS) // First entry must signal not enough slots
					return true;                             // We can now assume and skip checking cords

				// Otherwise check if all meta data states match
				return ShipLocation == LastTestedLocation
					&& ShipType == LastShipType
					&& ShipOrientation == LastShipOrientation;
			}
			void ProbeInvalidate() {
				TRACE_FUNTION_PROTO;
				
				SizeOfListSinceLastPush = 0;
				clear();
				ValidationState = CACHE_INVALID;

				SPDLOG_LOGGER_DEBUG(GameLog, "Invalidated probe cache");
			}
			void SaveProbeState(
				PointComponent ShipLocation,
				ShipClass      ShipType,
				ShipRotation   ShipOrientation
			) {
				TRACE_FUNTION_PROTO;

				// Deletes all previous commits since the last commit tho the cache
				if (SizeOfListSinceLastPush)
					erase(begin(), begin() + SizeOfListSinceLastPush);
				SizeOfListSinceLastPush = size();

				// Set meta data of current probe parameters
				LastTestedLocation = ShipLocation;
				LastShipType = ShipType;
				LastShipOrientation = ShipOrientation;
				ValidationState = CACHE_VALID;

				SPDLOG_LOGGER_DEBUG(GameLog, "Commited {} entries to current probe cache",
					size());
			}
			size_t GetNumberOfCommits() const {
				TRACE_FUNTION_PROTO; return size() - SizeOfListSinceLastPush;
			}
			span<StatusResponseT> GetCurrentCommits() {
				TRACE_FUNTION_PROTO; return { begin() + GetNumberOfCommits(), end() };
			}

		private:
			PointComponent LastTestedLocation{};
			ShipClass      LastShipType{};
			ShipRotation   LastShipOrientation{};
			size_t         SizeOfListSinceLastPush{};
		} InternalProbeListCached;

		PointComponent CalculateCordinatesOfPartByDistanceWithShip(
			PointComponent ShipLocation,
			ShipRotation   ShipOrientation,
			uint8_t        DistanceToWalk
		) const {
			TRACE_FUNTION_PROTO;

			switch (ShipOrientation) {
			case FACING_NORTH:
				return PointComponent{ ShipLocation.XComponent, (uint8_t)(ShipLocation.YComponent + DistanceToWalk) };
			case FACING_EAST:
				return PointComponent{ (uint8_t)(ShipLocation.XComponent - DistanceToWalk), ShipLocation.YComponent };
			case FACING_SOUTH:
				return PointComponent{ ShipLocation.XComponent, (uint8_t)(ShipLocation.YComponent - DistanceToWalk) };
			case FACING_WEST:
				return PointComponent{ (uint8_t)(ShipLocation.XComponent + DistanceToWalk), ShipLocation.YComponent };
			}
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

		unique_ptr<CellState[]> FieldCellStates;
		vector<ShipState>       FieldShipStates;
	};

	// GmPlayerField debug printer v2 legend / symbolic explanation:
	// Prints the content of 1 or 2 fields reporting the state of the 2d matrix
	// Each cell is split into 2 chars like [[PrimaryState][Status]]
	// [PrimaryState] : 'EMPTY' -> Cell is untouched, aka water
	//                  '|'/'=' -> Cell is part of a ship, rotation aware
	//                  'O'/'X' -> Cell was a miss or a ship being hit
	//                  'E'     -> Short for error, the cell is invalid
	//       [Status] : 'EMPTY' -> No special state, generally the case
	//                  'd'     -> The cell was hit at least 2 times, invalid
	//                  'k'     -> This cell was resposible for sinking a ship
	//                  'D'     -> just like 'k' except it was hit again afterwards
	export void Debug_PrintGmPlayerField(
		GameManagment::GmPlayerField& MyPlayerField_Left,
		GameManagment::GmPlayerField* OpponentPlayer = nullptr
	) {
		TRACE_FUNTION_PROTO;

		constexpr char AlphapectiCordLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
		string FieldFormat{ "-> GmPlayerField debug Printer v2:\n   |" };

		auto TransformCellstateToText = [&FieldFormat](
			GmPlayerField& PlayerField,
			PointComponent Cordinates
			) -> void {
				TRACE_FUNTION_PROTO;

				auto State = PlayerField.GetCellStateByCoordinates(Cordinates);
				auto AssociatedShip = State & PROBE_CELL_USED ?
					PlayerField.GetShipEntryForCordinate(Cordinates) : nullptr;
				char Printable = AssociatedShip ? "  |=OOXX"[(State & MASK_FILTER_STATE_BITS) * 2
					+ (AssociatedShip->Rotation & 1)]
					: " EOX"[State & MASK_FILTER_STATE_BITS];
				char Property = " dkD"[(State >> REQUIRED_BITS_PRIMARY) & MASK_FILTER_STATE_BITS];
				FieldFormat.append({ Printable, Property });
		};

		try {
			for (auto i = 0; i < MyPlayerField_Left.FieldDimensions.XComponent; ++i)
				FieldFormat.append({ AlphapectiCordLookup[i], ' ' });
			if (OpponentPlayer) {
				FieldFormat.append("|    |");
				for (auto i = 0; i < MyPlayerField_Left.FieldDimensions.XComponent; ++i)
					FieldFormat.append({ AlphapectiCordLookup[i], ' ' });
			}

			FieldFormat.append(fmt::format(!OpponentPlayer ? "|\n---+{0:-^{1}}+\n"
				: "|\n---+{0:-^{1}}+ ---+{0:-^{1}}+\n",
				"", MyPlayerField_Left.FieldDimensions.XComponent * 2));

			for (auto i = 0; i < MyPlayerField_Left.FieldDimensions.YComponent; ++i) {

				FieldFormat.append(fmt::format("{:>2d} |", i));
				for (auto j = 0; j < MyPlayerField_Left.FieldDimensions.XComponent; ++j)
					TransformCellstateToText(MyPlayerField_Left,
						PointComponent{ (uint8_t)j, (uint8_t)i });

				FieldFormat.append(!OpponentPlayer ? "|"
					: fmt::format("| {:>2d} |", i));
				if (OpponentPlayer) {
					for (auto j = 0; j < MyPlayerField_Left.FieldDimensions.XComponent; ++j)
						TransformCellstateToText(*OpponentPlayer,
							PointComponent{ (uint8_t)j, (uint8_t)i });

					FieldFormat.push_back('|');
				}

				FieldFormat.push_back('\n');
			}

			FieldFormat.append(fmt::format(!OpponentPlayer ? "---+{0:-^{1}}+\n"
				: "---+{0:-^{1}}+ ---+{0:-^{1}}+\n",
				"", MyPlayerField_Left.FieldDimensions.XComponent * 2));

			SPDLOG_LOGGER_DEBUG(GameLog, FieldFormat);
		}
		catch (const fmt::format_error& ExceptionInformation) {

			SPDLOG_LOGGER_ERROR(GameLog, "fmt failed to format string: \"{}\"",
				ExceptionInformation.what());
		}
	}



	// Helper structures, TODO: move this into GameManger2
	enum GamePhase {
		INVALID_PHASE = -1,
		SETUP_PHASE,
		GAME_PHASE,
		GAMEEND_PHASE
	};



	// The primary interface, this gives full control of the game and forwards other functionality
	class GameManager2 final
		: public MagicInstanceManagerBase<GameManager2> {
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

			// Check if we have enough slots to allocate players in
			if (PlayerFieldData.size() >= 2)
				return SPDLOG_LOGGER_WARN(GameLog, "Cannot allocate more Players, there are already 2 present"), nullptr;

			// Allocate and insert player into player field database
			auto [FieldIterator, Inserted] = PlayerFieldData.try_emplace(
				SocketAsId,
				InternalFieldDimensions,
				InternalShipCount);
			if (!Inserted)
				throw (SPDLOG_LOGGER_ERROR(GameLog, "failed to insert player field into controller"),
					runtime_error("failed to insert player field into controller"));

			// Check if the allocated player is our own (this is for client side) and return 
			if (PlayerFieldData.size() == 1)
				MyPlayerId = SocketAsId;
			SPDLOG_LOGGER_INFO(GameLog, "Allocated player for socket [{:04x}]",
				SocketAsId);
			return &FieldIterator->second;
		}

		GameManagerStatus CheckReadyAndStartGame() { // Checks if everything is valid and the game has been properly setup
			TRACE_FUNTION_PROTO;

			// Check if all players are ready to play
			if (NumberOfReadyPlayers != 2)
				return SPDLOG_LOGGER_WARN(GameLog, "Not all players are ready'd up"),
					STATUS_PLAYERS_NOT_READY;

			// Check if players have all their ships placed
			for (const auto& [Key, PlayerFieldController] : PlayerFieldData)
				if (PlayerFieldController.NumberOfShipsPerType.GetTotalCount() <
					PlayerFieldController.GetNumberOfShipsPlaced())
					return SPDLOG_LOGGER_WARN(GameLog, "Not all ships of Player [{:04x}] have been placed",
						Key), STATUS_NOT_ALL_PLACED;

			// Switch game phase state and notify caller
			CurrentGameState = GAME_PHASE;
			SPDLOG_LOGGER_INFO(GameLog, "Game is setup, switched to game phase state");
			return STATUS_SWITCHED_GAME_PHASE;
		}


		enum GetPlayerFieldForOperationByType {
			PLAYER_INVALID = 0, // An invalid control type, may be adapted
			GET_PLAYER_BY_ID,   // Retrieves player field by specified Id
			DOES_ID_OWN_PLAYER, // Same as GET_PLAYER_BY_ID, just expressive for usecase
			PLAYER_BY_TURN,     // Gets the player's field who has his current turn
			GET_MY_PLAYER,      // Retrieves the clients own players field

			GET_OPPONENT_PLAYER // Searches for the opponent of the specified players id
			                    // If the specified id is INVALID_SOCKET gives the opponent
			                    // of the clients player field
		};
		GmPlayerField* GetPlayerFieldByOperation(
			GetPlayerFieldForOperationByType ControlType,
			SOCKET                           SocketAsId
		) {
			TRACE_FUNTION_PROTO;

			switch (ControlType) {
			case PLAYER_INVALID:
				SPDLOG_LOGGER_WARN(GameLog, "Check command PLAYER_INVALID");
				break;

			case DOES_ID_OWN_PLAYER:
			case GET_PLAYER_BY_ID:
				for (auto& [SocketHandle, PlayerField] : PlayerFieldData)
					if (SocketHandle == SocketAsId)
						return &PlayerField;
				return nullptr;

			case PLAYER_BY_TURN:
				return GetPlayerFieldByOperation(GET_PLAYER_BY_ID,
					CurrentSelectedPlayer);
								
			case GET_MY_PLAYER:
				return GetPlayerFieldByOperation(GET_PLAYER_BY_ID,
					MyPlayerId);
				
			case GET_OPPONENT_PLAYER:
				if (SocketAsId == INVALID_SOCKET)
					SocketAsId = MyPlayerId;
				if (!GetPlayerFieldByOperation(DOES_ID_OWN_PLAYER,
					SocketAsId))
					return nullptr;
				return PlayerFieldData.begin()->first == SocketAsId ?
					&PlayerFieldData.begin()->second : &PlayerFieldData.end()->second;

			default:
				SPDLOG_LOGGER_ERROR(GameLog, "unsupplied handling encountered");
			}

			return nullptr;
		}
		SOCKET GetSocketAsIdForPlayerField(
			const GmPlayerField& PlayerField
		) const {
			TRACE_FUNTION_PROTO;
			
			for (const auto& [SocketAsId, InternalField] : PlayerFieldData)
				if (InternalField == PlayerField)
					return SocketAsId;
			return INVALID_SOCKET;
		}



		uint8_t GetCurrentPlayerCount() {
			TRACE_FUNTION_PROTO;

			return PlayerFieldData.size();
		}
		GamePhase GetCurrentGamePhase() {
			TRACE_FUNTION_PROTO;

			return CurrentGameState;
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

		SOCKET    MyPlayerId = INVALID_SOCKET;
		SOCKET    CurrentSelectedPlayer = INVALID_SOCKET;

		uint8_t   NumberOfReadyPlayers = 0;

		GamePhase CurrentGameState = SETUP_PHASE;
	};
}
