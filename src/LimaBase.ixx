// This module defines the io interfaces shared between the server and client for communication,
// used by the io driver classes and game manager
module;

#include "BattleShip.h"
#include <atomic>
#include <utility>
#include <queue>
#include <variant>
#include <type_traits>
#include <mutex>

export module LayerBase;
using namespace std;
export SpdLogger NetworkLog;


// SOCKET formatter for spdlog/fmt
export ostream& operator<<(
	      ostream& Input,
	const SOCKET&  SocketAsId
) {
	TRACE_FUNTION_PROTO;
	return Input << fmt::format("[{:04x}]",
		(void*)SocketAsId);
}

// Magic helpers for weird ass c++ fuckery
export template<typename T> // Private tag, this allows to create a tag object used to prevent the creation of
                               // an object from outside of the specified function(s)/class(es)
class PrivateTag {
	friend T;
public:
	PrivateTag(const PrivateTag&) = default;
	PrivateTag& operator=(const PrivateTag&) = default;
private:
	PrivateTag() = default;
};
export template<class T>
struct EnableMakeUnique : public T {
	EnableMakeUnique(auto&&... ParametersForward)
		: T(forward<decltype(ParametersForward)>(ParametersForward)...) { TRACE_FUNTION_PROTO; }
};
export template<typename T>
class MagicInstanceManagerBase {
	friend T;
public:
	template<typename... Args>
	static T& CreateObject(Args&&... Parameters) {
		TRACE_FUNTION_PROTO;
		
		struct EnableMakeUnique : public T {
			EnableMakeUnique(Args&&... ParametersForward)
				: T(forward<Args>(ParametersForward)...) { TRACE_FUNTION_PROTO; };
		};
		InstanceObject = make_unique<EnableMakeUnique>(forward<Args>(Parameters)...);

		return *InstanceObject;
	}
	static T& GetInstance() {
		TRACE_FUNTION_PROTO;

		// Check if current instance is valid and return reference
		if (InstanceObject)
			return *InstanceObject;

		// In case the Instance is invalid this will always throw an error
		SPDLOG_CRITICAL("Magic instance was invalid, {} was not allowed to call this",
			_ReturnAddress());
		throw std::runtime_error("Magic instance was invalid, caller was not allowed to call this");
	}
	static T* GetInstancePointer() {
		TRACE_FUNTION_PROTO; return InstanceObject.get();
	}
	static void ManualReset() {
		TRACE_FUNTION_PROTO; InstanceObject.reset();
	}

	// Singleton(const Singleton&) = delete;
	// Singleton(const Singleton&&) = delete;
	MagicInstanceManagerBase& operator=(
		const MagicInstanceManagerBase&) = delete;
	MagicInstanceManagerBase& operator=(
		const MagicInstanceManagerBase&&) = delete;

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

		enum NwRequestStatus {            // Describes the current state of this network request packet,
								          // the callback has to set it correctly depending on what it did with the request
			INVALID_STATUS = -2000,       // This Status is invalid, gets counted as a failure
			STATUS_FAILED_TO_CONNECT,     // Network manager failed to connect to socket
			STATUS_REQUEST_ERROR,         // An undefined error occurred during handling of the NRP
			STATUS_SOCKET_ERROR,          // An socket error occurred during the NRP, (consider disconnecting)
			STATUS_REQUEST_NOT_HANDLED,   // Request was not completed and returned to the dispatcher
			STATUS_SOCKET_DISCONNECTED,   // The socket circuit was terminated, shutting down connection
			STATUS_SHUTDOWN,              // The entire manager has to be shutdown, object invalid

			STATUS_REQUEST_COMPLETED = 0, // Indicates success and that the request was correctly handled
			STATUS_LIST_MODIFIED,         // same as STATUS_REQUEST_COMPLETED, 
			                              // but indicates that the socket descriptor list was modified
			STATUS_REQUEST_IGNORED,       // Request was completed but ignored, also fine
			STATUS_WORK_PENDING,          // Request is currently completing asynchronously
			STATUS_NO_INPUT_OUTPUT,       // No processing applied and or applyable
			STATUS_CONNECTED,             // A connection was successfully established
		} IoRequestStatus;

		virtual NwRequestStatus CompleteIoRequest(
			NwRequestStatus RequestStatus
		) {
			TRACE_FUNTION_PROTO; return IoRequestStatus = RequestStatus;
		}

		friend ostream& operator<<(
			ostream& Input,
			const NwRequestBase::NwServiceCtl NwCtl
		) {
			TRACE_FUNTION_PROTO;
			return Input << fmt::format("[NWCTL: {}]",
				(underlying_type_t<decltype(NwCtl)>)NwCtl);
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
		static constexpr uint8_t INVALID_COORD = 0xff;
		bool operator==(const PointComponent&) const = default;
		friend ostream& operator<<(
			ostream& Input,
			const PointComponent& Location
			) {
			TRACE_FUNTION_PROTO;
			return Input << fmt::format("{{{}:{}}}",
				Location.XComponent, Location.YComponent);
		}

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
		CELL_IS_IN_USE,
		CELL_WAS_SHOT_EMPTY = 2,
		CELL_SHIP_WAS_HIT,
		STATUS_WAS_ALREADY_SHOT = 4,
		STATUS_WAS_DESTRUCTOR = 8,

		// These values may only be used to probe, modify and notify existing states
		PROBE_CELL_WAS_SHOT = 2,
		PROBE_CELL_USED = 1,
		MERGE_SHOOT_CELL = 2,
		MASK_FILTER_STATE_BITS = 3,
		REQUIRED_BITS_PRIMARY = 2,
	};
	CellState& operator |=(CellState& Lhs,
		CellState Rhs) {
		TRACE_FUNTION_PROTO;

		using CellState_t = underlying_type<CellState>::type;
		return (CellState&)((CellState_t&)Lhs |= (CellState_t)Rhs);
	}

	// Server and Client may both receive as well as send this struct to remotes
	struct ShipSockControl {
		size_t SizeOfThisStruct = sizeof(*this); // Has to specify the size of the this struct including the flexible array member content,
												 // this is used for transmitting data, if this field is not set properly the send controls will probably fail
		size_t SpecialRequestIdentifier;         // A special id given to asynchronous requests that may need server/client confirmation
		                                         // and response treatment

		enum ShipControlStatus {
			STATUS_INVALID_PLACEMENT = -2000,
			STATUS_NOT_YOUR_TURN,
			STATUS_NOT_A_PLAYER,
			STATUS_NOT_IN_PHASE,
			STATUS_ENTRY_NOT_FOUND,
			STATUS_NO_READY_STATE,

			STATUS_NEUTRAL = 0,

			STATUS_YOUR_TURN,
			STATUS_YOURE_READY_NOW,
			STATUS_YOURE_LOBBY_HOST, // special, may never be implemented for MVP version
			STATUS_GAME_OVER,        // ends the game itself the winner is contained in selected player field

		};
		enum ShipControlCommandName {
			// The following is only send by the client to server
			NO_COMMAND_CLIENT = 0, // Should be unused (reserved for @Lima)
			SET_SHIP_LOC_CLIENT,   // Send only during setup, specifies where to place a ship
			SHOOT_CELL_CLIENT,     // Client requesting a cell to be shot
			READY_UP_CLIENT,       // Send by the client to state if they are ready or not
			REMOVE_SHIP_CLIENT,    // Requests the server to remove a placed ship from the field

			// The following is only send by server to client
			NO_COMMAND_SERVER,     // Should be unused (reserved for @Lima)
			SHOOT_CELL_SERVER,     // Notifies the clients of a cell being shot at
			CELL_STATE_SERVER,     // Notifies clients of cell state at location
			DEAD_SHIP_SERVER,      // Notifies clients if ship is destroyed, posts its state information (overlays CELL_STATE_SERVER)
			PLAYER_TURN_SERVER,    // Notifies the player if its his turn or not
			GAME_READY_SERVER,     // Broadcasted to all players to let the game begin
			STARTUP_FIELDSIZE,     // Broadcasts the field dimensions of the current game to all connecting clients
			STARTUP_SHIPCOUNTS,    // Sends players specifically the number of ships available for the game

			STARTUP_YOUR_ID,       // Transmitted when connecting as player, used by the server to locally identify players.
			                       // Players have to store this and associate this id with them selfs and their own field,
								   // To determine if the server informed them about a change to their own or opponents state.
								   // Id is transfered in SocketAsSelectedPlayerId field
			STARTUP_PLAYER_JOINED, // Currently not implemented as not needed

			// Command control codes shared by server and client
			RAISE_STATUS_MESSAGE = 2000, // Used to send status messages to clients

		} ControlCode;

		SOCKET SocketAsSelectedPlayerId; // Contains the servers id assined to a player that it wants to update, etc.
		                                 // The client has to translate this id into its own id's
		                                 // This field is only set by the server and should only be used by the client

		// The Following contains all data required to describe the players fields, not all fields have to be used by any command and better not be exposed 
		union {
			
			// SET_SHIP_LOC_CLIENT / DEAD_SHIP_SERVER
			struct {
				PointComponent ShipsPosition;
				ShipRotation   Rotation;
				ShipClass      TypeOfShip;
			} ShipLocation;

			// SHOOT_CELL_CLIENT/SERVER
			PointComponent ShootCellLocation;

			// STARTUP_FIELDSIZE
			PointComponent GameFieldSizes;

			// STARTUP_SHIPCOUNTS
			ShipCount GameShipNumbers;

			// REMOVE_SHIP_CLIENT
			PointComponent RemoveShipLocation;

			// CELL_STATE_SERVER
			struct {
				PointComponent Coordinates;
				CellState      State;
			} CellStateUpdate;



			// RAISE_STATUS_MESSAGE
			ShipControlStatus ShipControlRaisedStatus;
		};

		friend ostream& operator<<(
			      ostream&                                Input, 
			const ShipSockControl::ShipControlCommandName SsCtl
		) {
			TRACE_FUNTION_PROTO;
			return Input << fmt::format("[SSCTL: {}]",
				(underlying_type_t<decltype(SsCtl)>)SsCtl);
		}
	};
	static_assert(is_trivially_copyable_v<ShipSockControl>,
		"ShipSockControl is a non trivially copyable type, "
		"cannot stream it as raw/pod data");

	class WsaNetworkBase {
	public:
		WsaNetworkBase() {
			TRACE_FUNTION_PROTO;

			if (++RefrenceCounter != 1)
				return;

			WSADATA WinSockData;
			auto Result = WSAStartup(MAKEWORD(2, 2),
				&WinSockData);
			if (Result)
				throw (SPDLOG_LOGGER_ERROR(NetworkLog, "Failed to initialize winsock layer with {}",
					WSAGetLastError()),
					runtime_error("Failed to initialize winsock layer"));
			SPDLOG_LOGGER_INFO(NetworkLog, "Initilized winsock compatibility layer");
		}
		~WsaNetworkBase() {
			TRACE_FUNTION_PROTO;

			if (--RefrenceCounter == 0)
				WSACleanup(),
					SPDLOG_LOGGER_INFO(NetworkLog, "Closed winsock compatibility base");
		}

		static inline atomic<uint32_t> RefrenceCounter = 0;
	};

	size_t GenerateUniqueIdentifier() {
		static size_t Identifier = 1;
		return Identifier++;
	}
}
