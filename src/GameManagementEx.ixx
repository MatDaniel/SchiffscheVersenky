// This file defines and implements the GameManagement-Extensions (Ex)
// These allow to use the GameManager2 as the primary and prettymuch
// only interface present in the code base 
module;

#include "BattleShip.h"
#include <functional>
#include <map>
#include <algorithm>

export module GameManagementEx;
import Network.Client;
using namespace Network;
using namespace std;
export SpdLogger GameLogEx;

// Inject GameManagementEx requirements
export namespace Network::Client {
	class NetworkManager2;
}
export namespace Client {
	using namespace Network::Client;
	void InstallGameManagerInstrumentationCallbacks(
		NetworkManager2* NetworkDevice);
}
export namespace GameManagement {
	class GmPlayerField;
}

export namespace GameManagementEx {
	using namespace GameManagement;

	// Injectable callback interface used for instrumenting the game manager for inbuilt networking
	constexpr uint8_t PLACEMENT_ICALLBACK_INDEX = 0;
	constexpr uint8_t SRITECELL_ICALLBACK_INDEX = 1;
	constexpr uint8_t REMOVESHI_ICALLBACK_INDEX = 2;
	class CallbackInterface {
	public:
		uint8_t TypeTag_Index;
		
		bool StatusReceived = false;
		bool StatusAcceptable = true;

		CallbackInterface(uint8_t Tag)
			: TypeTag_Index(Tag) {}
		virtual ~CallbackInterface() = default;
	};

	class ShipPlacementEx
		: public CallbackInterface {
	public:
		ShipClass      TypeOfShip;
		ShipRotation   ShipOrientation;
		PointComponent ShipCoordinates;

		ShipPlacementEx(
			ShipClass      TypeOfShip,
			ShipRotation   ShipOrientation,
			PointComponent ShipCoordinates
		)
			: CallbackInterface(PLACEMENT_ICALLBACK_INDEX),
			  TypeOfShip(TypeOfShip),
			  ShipOrientation(ShipOrientation),
			  ShipCoordinates(ShipCoordinates) {}
	};
	class FieldStrikeLocationEx
		: public CallbackInterface {
	public:
		PointComponent StrikeLocation;

		FieldStrikeLocationEx(
			PointComponent StrikeLocation
		)
			: CallbackInterface(SRITECELL_ICALLBACK_INDEX),
		      StrikeLocation(StrikeLocation) {}
	};
	class RemoveShipLocationEx
		: public CallbackInterface {
	public:
		PointComponent OverlapRemoveLoc;

		RemoveShipLocationEx(
			PointComponent OverlapRemoveLoc
		)
			: CallbackInterface(REMOVESHI_ICALLBACK_INDEX),
			  OverlapRemoveLoc(OverlapRemoveLoc) {}
	};

	
	using InstrumentationCallback = bool(  // General Callback function signature,
										   // the function returns true if controlflow has been injected,
										   // otherwise it returns false and callers shoudl be have like normal
		GmPlayerField*     OptionalPlayer,
		CallbackInterface& RequestContext  // General purpose Callback context interface
		);
			
	// Injection entry list and indexes to those 
	using InjectedCallbackArray = function<InstrumentationCallback>[3];
}
