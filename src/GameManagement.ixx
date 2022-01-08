// This file implements the server sided game manager,
// it controls the entire game state and implements the
// game logic as well as game rules
module;

#include "BattleShip.h"
#include <memory>
#include <map>
#include <span>
#include <string>
#include <optional>

export module GameManagement;
export import GameManagementEx;
using namespace GameManagementEx;
import LayerBase;
using namespace std;
export SpdLogger GameLog;



// Shared game manager namespace
export namespace GameManagement {
	using namespace Network;

	// Primary game state referring to a unit field, each player is associated by handle (here by socket handle as id),
	// this offers the primary interface to interact with a players current game state
	class GmPlayerField {
		friend class GameManager2;
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

		GmPlayerField(                        // Todo move this to private, need to make make_unique enable
			const GameManager2* ManagerDevice
		);
		~GmPlayerField() {
			TRACE_FUNTION_PROTO; SPDLOG_LOGGER_INFO(GameLog,
				"Playerfield [{}:{}] destroyed, cleaning up state",
				(void*)this, (void*)FieldCellStates.get());
		}
		GmPlayerField(const GmPlayerField&) = delete;
		GmPlayerField& operator=(const GmPlayerField&) = delete;


		ProbeStatusList ProbeShipPlacement( // Probes the passed parameters of a possible requested ship placement
											// and validates them if they are possible or interfere with other
											// objects or the field area itself and returns a list of status location pairs based on that
			ShipClass      TypeOfShip,
			ShipRotation   ShipOrientation,
			PointComponent ShipCoordinates
		);

		bool PlaceShipBypassSecurityChecks( // This allocates a shipstate and enlists it in the fieldstate
											// and commits the changes to the cell grid array with no checks
											// always returns true if changes were applied
											// EX: may return false if
			ShipClass      TypeOfShip,
			ShipRotation   ShipOrientation,
			PointComponent ShipCoordinates
		);

		optional<PlayFieldStatus> 
		PlaceShipSecureCheckInterference(   // Tests and commits the changes to the game field,
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

			if (PlaceShipBypassSecurityChecks(ShipType,
				ShipOrientation,
				ShipCoordinates))
				return STATUS_SHIP_PLACED;
			return nullopt;
		}

		optional<ShipState>
		RemoveShipFromField(                // Tries to remove a ship that overlaps with the specified location,
			                                // returns the coords of the ship if removed or ZComponent is set to 1 on failure
			PointComponent ShipCoordinates  // A location to check overlapping with a ship placed on the field
		);

		optional<CellState> 
		StrikeCellAndUpdateShipList(         // Tries to "shoot" cell and applies handling,
		                                     // This will update the grid array and the shipstate list
			PointComponent TargetCoordinates // The target location to strike and update
		);


		// Helper functions for externals
		CellState& GetCellStateByCoordinates(
			PointComponent Cordinates
		);
		const ShipState* GetShipEntryForCordinate(
			PointComponent Cordinates
		) {
			TRACE_FUNTION_PROTO; return pGetShipEntryForCordinate(Cordinates);
		}
		span<const ShipState> GetShipStateList() const {
			TRACE_FUNTION_PROTO; return FieldShipStates;
		}
		ShipCount GetNumberOfShipsPlacedAndInverse( // Gets the ship count of the currently placed ships, or
			                                        // if the flag is set the number of ships left to be placed
			bool GetInverted						// 
		) const;
		uint8_t GetNumberOfShipsPlaced() const {
			TRACE_FUNTION_PROTO; return FieldShipStates.size();
		}
		bool operator==(const GmPlayerField& Other) const {
			TRACE_FUNTION_PROTO; return this == &Other;
		}

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
			void PushAndStoreProbeState(
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

			void InvalidateCache() {
				TRACE_FUNTION_PROTO;
				
				SizeOfListSinceLastPush = 0;
				clear();
				ValidationState = CACHE_INVALID;

				SPDLOG_LOGGER_DEBUG(GameLog, "Invalidated probe cache");
			}

			span<StatusResponseT> GetCurrentCommits() {
				TRACE_FUNTION_PROTO; return { begin() + SizeOfListSinceLastPush, end() };
			}
			size_t GetNumberOfCommits() const {
				TRACE_FUNTION_PROTO; return size() - SizeOfListSinceLastPush;
			}

		private:
			PointComponent LastTestedLocation{};
			ShipClass      LastShipType{};
			ShipRotation   LastShipOrientation{};
			size_t         SizeOfListSinceLastPush{};
		} 
		InternalProbeListCached;


		ShipState* pGetShipEntryForCordinate(
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
					if (IteratorPosition == Cordinates)
						return SPDLOG_LOGGER_INFO(GameLog, "Found ship at {}, for {}",
							ShipEntry.Cordinates,
							Cordinates),
							&ShipEntry;
				}
			}

			SPDLOG_LOGGER_WARN(GameLog, "Failed to find an associated ship for location {{{}:{}}}",
				Cordinates.XComponent, Cordinates.YComponent);
			return nullptr;
		}

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

		unique_ptr<CellState[]> FieldCellStates;
		vector<ShipState>       FieldShipStates;
	};

	// The primary interface, this gives full control of the game and forwards other functionality
	class GameManager2 final
		: public MagicInstanceManagerBase<GameManager2> {
		friend class MagicInstanceManagerBase<GameManager2>;
		friend GmPlayerField;
		friend void ::Client::InstallGameManagerInstrumentationCallbacks(Network::Client::NetworkManager2* NetworkDevice);
	public:
		enum GameManagerStatus {
			STATUS_INVALID = -2000,
			STATUS_PLAYERS_NOT_READY,
			STATUS_NOT_ALL_PLACED,
			STATUS_PLAYER_ALREADY_READY,

			STATUS_OK = 0,
			STATUS_SWITCHED_GAME_PHASE,
			STATUS_PLAYERS_READY,
		};
		enum GamePhase {
			INVALID_PHASE = -1,
			SETUP_PHASE,
			GAME_PHASE,
			GAMEEND_PHASE
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
				SocketAsId, this);
			if (!Inserted)
				throw (SPDLOG_LOGGER_ERROR(GameLog, "failed to insert player field into controller"),
					runtime_error("failed to insert player field into controller"));

			// Check if the allocated player is our own (this is for client side) and return 
			if (PlayerFieldData.size() == 1)
				MyPlayerId = SocketAsId;
			SPDLOG_LOGGER_INFO(GameLog, "Allocated player for socket {}",
				SocketAsId);
			return &FieldIterator->second;
		}

		// GameManagerStatus CheckReadyAndStartGame() { // Checks if everything is valid and the game has been properly setup
		// 	TRACE_FUNTION_PROTO;
		// 
		// 	// Check if all players are ready to play
		// 	if (ReadyPlayerMask != 2)
		// 		return SPDLOG_LOGGER_WARN(GameLog, "Not all players are ready'd up"),
		// 		STATUS_PLAYERS_NOT_READY;
		// 
		// 	// Check if players have all their ships placed
		// 	for (const auto& [Key, PlayerFieldController] : PlayerFieldData)
		// 		if (InternalShipCount.GetTotalCount() <
		// 			PlayerFieldController.GetNumberOfShipsPlaced())
		// 			return SPDLOG_LOGGER_WARN(GameLog, "Not all ships of Player {} have been placed",
		// 				Key), STATUS_NOT_ALL_PLACED;
		// 
		// 	// Switch game phase state and notify caller
		// 	CurrentGameState = GAME_PHASE;
		// 	SPDLOG_LOGGER_INFO(GameLog, "Game is setup, switched to game phase state");
		// 	return STATUS_SWITCHED_GAME_PHASE;
		// }
		GameManagerStatus CheckMyPlayerReadyBegin() { // Checks if we can ready local client game,
			                                          // cause no ships are fucking placed on their side for the opponent
			TRACE_FUNTION_PROTO;

			// Check if my player has all his ships placed
			auto PlayerField = GetPlayerFieldByOperation(GET_MY_PLAYER,
				INVALID_SOCKET);
			if (InternalShipCount.GetTotalCount() <
				PlayerField->GetNumberOfShipsPlaced())
				return SPDLOG_LOGGER_WARN(GameLog, "Not all ships of Player {} have been placed",
					GetSocketAsIdForPlayerField(*PlayerField)), STATUS_NOT_ALL_PLACED;

			// Switch game phase state and notify caller
			CurrentGameState = GAME_PHASE;
			SPDLOG_LOGGER_INFO(GameLog, "Game is setup, switched to game phase state");
			return STATUS_SWITCHED_GAME_PHASE;
		}
		GameManagerStatus ReadyUpPlayerByPlayer(
			const GmPlayerField& PlayerField
		) {
			TRACE_FUNTION_PROTO;

			if (InternalShipCount.GetTotalCount() <
				PlayerField.GetNumberOfShipsPlaced())
				return SPDLOG_LOGGER_WARN(GameLog, "Not all ships of Player {} have been placed",
					GetSocketAsIdForPlayerField(PlayerField)), STATUS_NOT_ALL_PLACED;
			
			auto FirstPlayerJoined = GetPlayerFieldByOperation(GET_MY_PLAYER, INVALID_SOCKET);
			auto PlayerMaskForThisPlayer = FirstPlayerJoined == &PlayerField ? 1 : 2;
			if (ReadyPlayerMask & PlayerMaskForThisPlayer)
				return STATUS_PLAYER_ALREADY_READY;
			ReadyPlayerMask |= PlayerMaskForThisPlayer;

			if (ReadyPlayerMask == 3)
				return STATUS_PLAYERS_READY;
			return STATUS_OK;
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

			auto PossibleLocation = find_if(PlayerFieldData.begin(),
				PlayerFieldData.end(),
				[&PlayerField](
					decltype(PlayerFieldData)::const_reference MapEntry
					) {
						return MapEntry.second == PlayerField;
				});
			
			return PossibleLocation == PlayerFieldData.end() ?
				INVALID_SOCKET
				: PossibleLocation->first;
		}

		uint8_t GetCurrentPlayerCount() const {
			TRACE_FUNTION_PROTO; return PlayerFieldData.size();
		}
		GamePhase GetCurrentGamePhase() const {
			TRACE_FUNTION_PROTO; return CurrentGameState;
		}
		SOCKET GetCurrentSelectedPlayer() const {
			TRACE_FUNTION_PROTO; return CurrentSelectedPlayer;
		}
		bool EndCurrentGame() {
			TRACE_FUNTION_PROTO;

			if (CurrentGameState != GAME_PHASE) {

				// We cannot end the game here
				SPDLOG_LOGGER_ERROR(GameLog, "Failed to end game, not even in the correct state");
				return false;
			}

			SPDLOG_LOGGER_INFO(GameLog, "Game over, gamestate changed and the game was terminated");
			return true;
		}

		SOCKET SwitchPlayersTurnState() {
			TRACE_FUNTION_PROTO;

			return CurrentSelectedPlayer = GetSocketAsIdForPlayerField(
				*GetPlayerFieldByOperation(GET_OPPONENT_PLAYER,
					CurrentSelectedPlayer));
		}
		SOCKET SelectRandomCurrentPlayer() {
			TRACE_FUNTION_PROTO;

			// Random bull shittery, not actually random tho, whatever fuck it
			return CurrentSelectedPlayer = MyPlayerId;
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
		uint8_t   ReadyPlayerMask = 0;
		GamePhase CurrentGameState = SETUP_PHASE;

		// GameManagementEx instrumentation callbacks injection
		InjectedCallbackArray ExCallbacks;
	};

#pragma region Game field controller fragment inplementation
	GmPlayerField::GmPlayerField(         // Todo move this to private, need to make make_unique enable
		const GameManager2* ManagerDevice
	) {
		TRACE_FUNTION_PROTO;

		auto NumberOfCells = ManagerDevice->InternalFieldDimensions.XComponent *
			ManagerDevice->InternalFieldDimensions.YComponent;
		FieldCellStates = make_unique<CellState[]>(NumberOfCells);
		memset(FieldCellStates.get(),
			CELL_IS_EMPTY,
			NumberOfCells);
		SPDLOG_LOGGER_INFO(GameLog, "Allocated and initialized internal gamefield lookup with parameters: {{{}:{}}}",
			ManagerDevice->InternalFieldDimensions.XComponent, ManagerDevice->InternalFieldDimensions.YComponent);
	}

	GmPlayerField::ProbeStatusList 
	GmPlayerField::ProbeShipPlacement( // Probes the passed parameters of a possible requested ship placement
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
		auto& ManagerDevice = GameManager2::GetInstance();
		for (auto i = 0; auto & ShipEntry : FieldShipStates)
			if (ShipEntry.ShipType == TypeOfShip)
				if (++i >= ManagerDevice.InternalShipCount.ShipCounts[TypeOfShip]) {

					InternalProbeListCached.emplace_back(StatusResponseT{ {}, STATUS_NOT_ENOUGH_SLOTS });
					InternalProbeListCached.PushAndStoreProbeState(ShipCoordinates,
						TypeOfShip,
						ShipOrientation);
					SPDLOG_LOGGER_ERROR(GameLog, "Conflicting ship category, all ships of type already in use");
					return InternalProbeListCached;
				}

		// Check if the ship even fits within the field at its cords
		auto& FieldDimensions = ManagerDevice.InternalFieldDimensions;
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

			SPDLOG_LOGGER_WARN(GameLog, "{} and {} may not be within allocated field",
				ShipCoordinates,
				ShipEndPosition);
			InternalProbeListCached.PushAndStoreProbeState(ShipCoordinates,
				TypeOfShip,
				ShipOrientation);
			return InternalProbeListCached;
		}

		// Validate ship placement, check for collisions and illegal positioning
		// This loop iterates through all cell positions of ship parts,
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
					auto CellstateOfLookup = GetCellStateByCoordinates(LookupLocation);
					if (CellstateOfLookup & PROBE_CELL_USED) {

						// Assume which type of collision we are dealing with, this may not be final
						auto TypeOfCollision = LookupLocation == IteratorPosition
							? STATUS_DIRECT_COLLIDE : STATUS_INDIRECT_COLLIDE;

						// Check if location has already been reported or updated to direct if falsely reported as indirect
						bool IsAlreadyReported = false;
						for (auto& [ReportedLocation, StatusMessage] : InternalProbeListCached.GetCurrentCommits())
							if (ReportedLocation == LookupLocation) {
								if (ReportedLocation == IteratorPosition &&
									StatusMessage == STATUS_INDIRECT_COLLIDE) {
									StatusMessage = STATUS_DIRECT_COLLIDE;

									SPDLOG_LOGGER_WARN(GameLog, "Updated collision state of {} to direct",
										ReportedLocation);
								}

								IsAlreadyReported = true; break;
							}
						if (!IsAlreadyReported)
							InternalProbeListCached.emplace_back(StatusResponseT{ LookupLocation, TypeOfCollision }),
							SPDLOG_LOGGER_WARN(GameLog, "Reported {} at {}",
								LookupLocation == IteratorPosition ? "collision" : "touching",
								LookupLocation);
					}
				}
		}

		InternalProbeListCached.PushAndStoreProbeState(ShipCoordinates,
			TypeOfShip,
			ShipOrientation);
		return InternalProbeListCached;
	}
	bool GmPlayerField::PlaceShipBypassSecurityChecks( // This allocates a shipstate and enlists it in the fieldstate
										               // and commits the changes to the cell grid array with no checks
		                                               // always returns true if changes were applied
		                                               // EX: may return false if
		ShipClass      TypeOfShip,
		ShipRotation   ShipOrientation,
		PointComponent ShipCoordinates
	) {
		TRACE_FUNTION_PROTO;

		// Get Instrumentation callback for control flow injection
		auto& GameManager = GameManager2::GetInstance();
		auto& PlacementCallback = GameManager.ExCallbacks[PLACEMENT_ICALLBACK_INDEX];
		if (PlacementCallback) {

			// Callback has been installed, build context and dispatch callable
			ShipPlacementEx PlacementInject(TypeOfShip,
				ShipOrientation,
				ShipCoordinates);
			if (PlacementCallback(this,
				PlacementInject))
				return false;
		}

		// Commit changes and place the ship on the field updating the cells
		for (auto i = 0; i < ShipLengthPerType[TypeOfShip]; ++i) {

			// This loop iterates through all cell positions of ship parts and writes the state to the grid
			auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
				ShipCoordinates, ShipOrientation, i);
			GetCellStateByCoordinates(IteratorPosition) = CELL_IS_IN_USE;
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
		InternalProbeListCached.InvalidateCache();
		SPDLOG_LOGGER_INFO(GameLog, "Registered ship in list at location {}",
			ShipCoordinates);
		return true;
	}

	CellState& GmPlayerField::GetCellStateByCoordinates(
		PointComponent Cordinates
	) {
		TRACE_FUNTION_PROTO; 
		return FieldCellStates[Cordinates.YComponent *
			GameManager2::GetInstance().InternalFieldDimensions.XComponent + Cordinates.XComponent];
	}

	ShipCount GmPlayerField::GetNumberOfShipsPlacedAndInverse(
		bool GetInverted
	) const {
		ShipCount ShipCountOrInverse;
		for (auto& ShipEntry : FieldShipStates)
			++ShipCountOrInverse.ShipCounts[ShipEntry.ShipType];

		if (GetInverted) {

			// Invert ship counts (the number of ships left on the field)
			auto& GameManager = GameManager2::GetInstance();
			for (auto i = 0; i < extent_v<decltype(ShipCount::ShipCounts)>; ++i)
				ShipCountOrInverse.ShipCounts[i] =
				GameManager.InternalShipCount.ShipCounts[i] - ShipCountOrInverse.ShipCounts[i];
		}

		return ShipCountOrInverse;
	}

	optional<CellState>
	GmPlayerField::StrikeCellAndUpdateShipList( // Tries to "shoot" cell and applies handling,
										        // This will update the grid array and the shipstate list
		PointComponent TargetCoordinates        // The target location to strike and update
	) {
		TRACE_FUNTION_PROTO;

		// Get Instrumentation callback for control flow injection
		auto& GameManager = GameManager2::GetInstance();
		auto& StrikeCallbackEx = GameManager.ExCallbacks[SRITECELL_ICALLBACK_INDEX];
		if (StrikeCallbackEx) {

			// Callback has been installed, build context and dispatch callable
			FieldStrikeLocationEx StrikeInjection(TargetCoordinates);
			if (StrikeCallbackEx(this,
				StrikeInjection))
				return nullopt;
		}

		// Test and shoot cell in grid
		auto& LocalCell = GetCellStateByCoordinates(TargetCoordinates);
		if (LocalCell & PROBE_CELL_WAS_SHOT)
			return SPDLOG_LOGGER_ERROR(GameLog, "Location {} was struck multiple times",
				TargetCoordinates),
			LocalCell |= STATUS_WAS_ALREADY_SHOT;
		LocalCell |= MERGE_SHOOT_CELL;
		SPDLOG_LOGGER_INFO(GameLog, "Cellstate in location {} was updated to {}",
			TargetCoordinates, LocalCell);

		// Check if cell is within a ship, in which case we locate the ship and check for the destruction of the ship
		if (!(LocalCell & PROBE_CELL_USED))
			return LocalCell;

		// Apply changes to each cell below the ship itself
		auto ShipEntry = pGetShipEntryForCordinate(TargetCoordinates);
		for (auto i = 0; i < ShipLengthPerType[ShipEntry->ShipType]; ++i) {

			// Enumerate all cells and test for non destroyed cell
			auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
				*ShipEntry, i);
			if ((GetCellStateByCoordinates(IteratorPosition) & MASK_FILTER_STATE_BITS) != CELL_SHIP_WAS_HIT)
				return SPDLOG_LOGGER_INFO(GameLog, "Ship at {} was hit in {}",
					ShipEntry->Cordinates,
					TargetCoordinates),
				LocalCell;
		}

		SPDLOG_LOGGER_INFO(GameLog, "Ship at {} was sunken",
			ShipEntry->Cordinates);
		return LocalCell |= STATUS_WAS_DESTRUCTOR;
	}

	optional<ShipState>
	GmPlayerField::RemoveShipFromField(     // Tries to remove a ship that overlaps with the specified location,
											// returns the coords of the ship if removed or ZComponent is set to 1 on failure
		PointComponent ShipCoordinates      // A location to check overlapping with a ship placed on the field
	) {
		TRACE_FUNTION_PROTO;

		// Find possible ship that overlays with our coordinates
		auto PossibleShip = pGetShipEntryForCordinate(ShipCoordinates);
		if (!PossibleShip)
			return ShipState{ .Cordinates = { 0, 0, 1} };

		// Get Instrumentation callback for control flow injection
		auto& GameManager = GameManager2::GetInstance();
		auto& RemoveShipCallbackEx = GameManager.ExCallbacks[REMOVESHI_ICALLBACK_INDEX];
		if (RemoveShipCallbackEx) {

			// Callback has been installed, build context and dispatch callable
			RemoveShipLocationEx RemoveInjection(ShipCoordinates);
			if (RemoveShipCallbackEx(this,
				RemoveInjection))
				return nullopt;
		}

		// We found a ship, now we just have to undo the placement
		for (auto i = 0; i < ShipLengthPerType[PossibleShip->ShipType]; ++i) {

			auto IteratorPosition = CalculateCordinatesOfPartByDistanceWithShip(
				*PossibleShip, i);
			(underlying_type_t<CellState>&)GetCellStateByCoordinates(
				IteratorPosition) &= ~PROBE_CELL_USED;
		}

		auto ShipLocationBackup = *PossibleShip;
		FieldShipStates.erase(PossibleShip - FieldShipStates.data()
			+ FieldShipStates.begin());
		InternalProbeListCached.InvalidateCache();
		SPDLOG_LOGGER_INFO(GameLog, "Ship at location {} was removed form the field",
			ShipLocationBackup.Cordinates);
		return ShipLocationBackup;
	}
#pragma endregion Game field controller fragment inplementation


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
		GameManagement::GmPlayerField& MyPlayerField_Left,
		GameManagement::GmPlayerField* OpponentPlayer = nullptr
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

		// Get field dimensions, the field itself inherits the values of the manager
		auto& ManagerDevice = GameManager2::GetInstance();
		auto& FieldDimensions = ManagerDevice.InternalFieldDimensions;
		try {
			for (auto i = 0; i < FieldDimensions.XComponent; ++i)
				FieldFormat.append({ AlphapectiCordLookup[i], ' ' });
			if (OpponentPlayer) {
				FieldFormat.append("|    |");
				for (auto i = 0; i < FieldDimensions.XComponent; ++i)
					FieldFormat.append({ AlphapectiCordLookup[i], ' ' });
			}

			FieldFormat.append(fmt::format(!OpponentPlayer ? "|\n---+{0:-^{1}}+\n"
				: "|\n---+{0:-^{1}}+ ---+{0:-^{1}}+\n",
				"", FieldDimensions.XComponent * 2));

			for (auto i = 0; i < FieldDimensions.YComponent; ++i) {

				FieldFormat.append(fmt::format("{:>2d} |", i));
				for (auto j = 0; j < FieldDimensions.XComponent; ++j)
					TransformCellstateToText(MyPlayerField_Left,
						PointComponent{ (uint8_t)j, (uint8_t)i });

				FieldFormat.append(!OpponentPlayer ? "|"
					: fmt::format("| {:>2d} |", i));
				if (OpponentPlayer) {
					for (auto j = 0; j < FieldDimensions.XComponent; ++j)
						TransformCellstateToText(*OpponentPlayer,
							PointComponent{ (uint8_t)j, (uint8_t)i });

					FieldFormat.push_back('|');
				}

				FieldFormat.push_back('\n');
			}

			FieldFormat.append(fmt::format(!OpponentPlayer ? "---+{0:-^{1}}+\n"
				: "---+{0:-^{1}}+ ---+{0:-^{1}}+\n",
				"", FieldDimensions.XComponent * 2));

			SPDLOG_LOGGER_DEBUG(GameLog, FieldFormat);
		}
		catch (const fmt::format_error& ExceptionInformation) {

			SPDLOG_LOGGER_ERROR(GameLog, "fmt failed to format string: \"{}\"",
				ExceptionInformation.what());
		}
	}
}
