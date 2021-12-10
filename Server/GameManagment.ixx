// This file implements the server sided game manager,
// it controls the entire gamestate and implements the
// gamelogic as well as gamerules
module;

#include <ShipSock.h>

export module GameManagment;



export class GameManagmentController {
public:


	struct PlayerFiled {
		enum CellState : uint8_t {
			CELL_IS_EMPTY,
			CELL_IS_IN_USE,
			CELL_WAS_SHOT_EMPTY,
			CELL_WAS_SHOT_IN_USE
		} **CellField;
		struct {
			uint8_t Xd,
				Yd;
		} FieldDimensions;

		struct ShipState {
			ShipSockControl::ShipTypes ShipType;
			uint8_t XCord,
				YCord;
			uint8_t Rotation;
			bool    Destroyed;
		} *ShipStates;
		struct ShipData {
			uint8_t DestroyerCount;
			uint8_t CruiserCount;
			uint8_t SubmarineCount;
			uint8_t BattleshipCount;
			uint8_t CarrierCount;
		};
	};


private:



};
