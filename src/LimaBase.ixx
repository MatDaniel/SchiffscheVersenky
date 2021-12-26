// This module defines the io interfaces shared between the server and client for communication,
// used by the io driver classes and game manager
module;

#include "BattleShip.h"

export module LayerBase;
using namespace std;
export SpdLogger NetworkLog;


export template<typename T>
class PSingletonFactory {
public:
	template<typename... Args>
	static T& CreateObject(Args&&... Parameters) {
		struct EnableMakeUnique : public T {
			inline EnableMakeUnique(Args&&... ParametersForward)
				: T(forward<Args>(ParametersForward)...) {};
		};
		InstanceObject = make_unique<EnableMakeUnique>(forward<Args>(Parameters)...);

		return *InstanceObject;
	}
	static T& GetInstance() {
		return *InstanceObject;
	}
	static void ManualReset() {
		InstanceObject.reset();
	}

	// Singleton(const Singleton&) = delete;
	// Singleton(const Singleton&&) = delete;
	PSingletonFactory& operator=(const PSingletonFactory&) = delete;
	PSingletonFactory& operator=(const PSingletonFactory&&) = delete;

private:
	static inline unique_ptr<T> InstanceObject;
};



// Primary networking namespace
export namespace Network {
	
	struct NwRequestBase {
		enum NwServiceCtl {                  // a control code specifying the type of handling required
			// Shared commands
			NO_COMMAND,                      // reserved for @Lima, dont use
			INCOMING_PACKET,
			INCOMING_PACKET_PRIORITIZED,
			OUTGOING_PACKET_COMPLETE,
			SOCKET_DISCONNECTED,
			SERVICE_ERROR_DETECTED,
			
			// unique to server
			INCOMING_CONNECTION,
		} IoControlCode;
	

		enum NwRequestStatus {      // describes the current state of this network request packet,
								     // the callback has to set it correctly depending on what it did with the request
			INVALID_STATUS = -2000,
			STATUS_FAILED_TO_CONNECT, 
			STATUS_REQUEST_ERROR,
			STATUS_SOCKET_ERROR,
			STATUS_REQUEST_NOT_HANDLED,

			STATUS_REQUEST_COMPLETED = 0,
			STATUS_REQUEST_IGNORED,
			STATUS_WORK_PENDING,
			STATUS_LIST_MODIFIED,
		} IoRequestStatus;

		virtual NwRequestStatus CompleteIoRequest(
			NwRequestStatus RequestStatus
		) {
			TRACE_FUNTION_PROTO;

			return IoRequestStatus = RequestStatus;
		}

	};


	enum ShipClass : uint8_t { // Specifies the type of ship
							   // is also used as an index into the count array of ShipCount
		DESTROYER_2x1 = 0,
		CRUISER_3x1,
		SUBMARINE_3x1,
		BATTLESHIP_4x1,
		CARRIER_5x1
	};
	constexpr uint8_t ShipLengthPerType[5]{
		2, 3, 3, 4, 5
	};
	enum ShipRotation {
		FACING_NORTH = 0,
		FACING_EAST = 1,
		FACING_SOUTH = 2,
		FACING_WEST = 3
	};


	class PointComponent {
	public:
		bool operator==(const PointComponent&) const = default;
		
		uint8_t XComponent{},
			YComponent{},
			ZComponent{};
	};


	// Sub packets
	class ShipCount {
	public:
		uint8_t GetTotalCount() const {
			TRACE_FUNTION_PROTO;

			auto TotalCount = 0;
			for (auto i = 0; i < 5; ++i)
				TotalCount += ShipCounts[i];
			return TotalCount;
		}

		uint8_t ShipCounts[5]{}; // Use enum ShipClass as an index to get the appropriate count associated to type
	};

	struct ShipState {
		// Ship base information
		ShipClass       ShipType;
		PointComponent Cordinates; // The cords of the ship always specify the location of the front
		ShipRotation    Rotation;

		// Additional meta data
		bool Destroyed;
	};

	enum CellState : int8_t {
		CELL_IS_EMPTY = 0,
		CELL_IS_IN_USE = 1,
		CELL_WAS_SHOT_EMPTY = 2,
		CELL_WAS_SHOT_IN_USE = 3,

		// These values may only be used to probe, modify and notify existing states
		ASSING_SHOOT_MERGE_VALUE = 2,
		PROBE_CELL_WAS_SHOT = 2,
		PROBE_CELL_USED = 1,
		STATUS_WAS_ALREADY_SHOT = -1,
		STATUS_WAS_DESTRUCTOR = -2
	};
	CellState operator &=(CellState& Lhs,
		const CellState& Rhs) {
		TRACE_FUNTION_PROTO;

		using CellState_t = underlying_type<CellState>::type;
		return (CellState)((CellState_t&)Lhs &= (CellState_t)ASSING_SHOOT_MERGE_VALUE);
	}


	enum ShipControlStatus {
		STATUS_INVALID_PLACEMENT = -2000,
		STATUS_NOT_YOUR_TURN,

		STATUS_NEUTRAL = 0,

		STATUS_YOUR_TURN,
		STATUS_YOURE_LOBBY_HOST, // special, may never be implemented for MVP version

	};

	// Server and Client may both receive as well as send this struct to remotes
	struct ShipSockControl {
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
				PointComponent ShipsPosition;
				ShipRotation    Rotation;
			} SetShipLocation;

			// SHOOT_CELL_CLIENT
			PointComponent ShootCellLocation;

			// STARTUP_FIELDSIZE
			PointComponent GameFieldSizes;


			// CELL_STATE_SERVER
			struct {
				PointComponent Coordinates;
				CellState       State;
			} CellStateUpdate;

			// RAISE_STATUS_MESSAGE
			ShipControlStatus ShipControlRaisedStatus;
		};
	};



	class WsaNetworkBase {
	public:
		WsaNetworkBase() {
			TRACE_FUNTION_PROTO;

			if (++RefrenceCounter == 1) {
				WSADATA WinSockData;
				auto Result = WSAStartup(MAKEWORD(2, 2),
					&WinSockData);
				if (Result)
					throw (SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to initialize winsock layer"),
						Result);
				SPDLOG_LOGGER_INFO(NetworkLog, "Initilized winsock compatibility layer");
			}
		}
		~WsaNetworkBase() {
			TRACE_FUNTION_PROTO;

			if (--RefrenceCounter == 0)
				WSACleanup(),
					SPDLOG_LOGGER_INFO(NetworkLog, "Closed winsock compatibility base");
		}

		static inline atomic<uint32_t> RefrenceCounter = 0;
	};
}
