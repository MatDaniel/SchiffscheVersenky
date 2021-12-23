// This file implements the server sided game manager,
// it controls the entire gamestate and implements the
// gamelogic as well as gamerules
module;

#include <SharedLegacy.h>
#include <memory>

export module GameManagment;
import ShipSock;
using namespace std;



// export spdlogger GameLog;


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
