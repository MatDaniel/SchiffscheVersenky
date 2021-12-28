module;

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>

#include "resource.h"

export module Game.ShipInfo;
import GameManagment;
import Network.Client;

namespace
{

	glm::mat4 RotateShip(float deg)
	{
		constexpr glm::vec3 up { 0.0F, 1.0F, 0.0F };
		return glm::rotate(glm::mat4(1.0F), glm::radians(deg), up);
	}

}

export namespace Draw
{

	export constexpr size_t ShipTypeCount = 5;

	export enum ShipType
	{
		ST_DESTROYER,
		ST_CRUISER,
		ST_SUBMARINE,
		ST_BATTLESHIP,
		ST_CARRIER
	};

	export struct ShipInfo
	{
		const char* const name;
		const uint32_t size;
		const int resId;
		const char* const resName;
	};

	export ShipInfo ShipInfos[ShipTypeCount]{
		{ "Destroyer",  2, IDR_MESH_SHIP_DSTR, "Ships/Destroyer"  },
		{ "Cruiser",    3, IDR_MESH_SHIP_CRUS, "Ships/Cruiser"    },
		{ "Submarine",  3, IDR_MESH_SHIP_SUBM, "Ships/Submarine"  },
		{ "Battleship", 4, IDR_MESH_SHIP_BTLE, "Ships/Battleship" },
		{ "Carrier",    5, IDR_MESH_SHIP_CARR, "Ships/Carrier"    }
	};

	// -- Direction --
	// First bit is axis (horizontal = 0; vertical = 1)
	// Second bit is direction (backwards = 0; forward = 1)
	export enum ShipDirection : uint8_t
	{

		// AXIS PART
		SD_AXISPART = 0x1, // first bit
		SD_HORIZONTAL = 0x1, // 10
		SD_VERTICAL = 0x0, // 00  

		// DIRECTION PART
		SD_DIRPART = 0x2, // second bit
		SD_BACKWARDS = 0x0, // 00
		SD_FORWARD = 0x2, // 01

		// CONCRETE DIRECTIONS
		SD_NORTH = 0x0, // 00
		SD_EAST = 0x3, // 11
		SD_SOUTH = 0x2, // 01
		SD_WEST = 0x1, // 10

	};

	glm::mat4 ShipRotations[4] {
		RotateShip(90.0F),
		RotateShip(180.0F),
		RotateShip(-90.0F),
		RotateShip(0.0F)
	};

	struct ShipPosition
	{
		glm::vec2 position;
		ShipDirection direction;
	};

	

}

// -- Casts between Network and Draw modules --
// Used to cast variables to network compatible and draw compatible types.

export Network::ShipRotation NetCastRot(Draw::ShipDirection dir)
{
	auto result = dir & Draw::SD_HORIZONTAL ? dir ^ Draw::SD_DIRPART : dir;
	return (Network::ShipRotation)result; // East & West are switched, North & South are the same
}

export Draw::ShipDirection DrawCastRot(Network::ShipRotation rot)
{
	auto result = (uint8_t)rot & Draw::SD_HORIZONTAL ? rot ^ Draw::SD_DIRPART : rot;
	return (Draw::ShipDirection)result;
}

export Network::ShipClass NetCastType(Draw::ShipType type)
{
	return (Network::ShipClass)type; // Should be the same
}

export Draw::ShipType DrawCastType(Network::ShipClass cls)
{
	return (Draw::ShipType)cls; // Should be the same
}

export Network::PointComponent NetCastPos(Draw::ShipType type, Draw::ShipDirection dir, const glm::vec2& pos)
{
	float hSize = Draw::ShipInfos[type].size / (dir & Draw::SD_FORWARD ? 2.0F : -2.0F);
	float mod = hSize > 0.0F ? -1.0F : 0.0F;
	if (dir & Draw::SD_HORIZONTAL)
		return Network::PointComponent{
			(uint8_t)(pos.x + hSize + mod),
			(uint8_t)(pos.y - 0.5F)
		};
	else
		return Network::PointComponent{
			(uint8_t)(pos.x - 0.5F),
			(uint8_t)(pos.y + hSize + mod)
		};
}

export Network::PointComponent NetCastPos(Draw::ShipType type, const Draw::ShipPosition& pos)
{
	return NetCastPos(type, pos.direction, pos.position);
}

export std::pair<Draw::ShipType, Draw::ShipPosition> DrawCastPos(Network::ShipState state)
{
	const auto type = DrawCastType(state.ShipType);
	const auto dir = DrawCastRot(state.Rotation);
	const auto& coords = state.Cordinates;
	float hSize = Draw::ShipInfos[type].size / (dir & Draw::SD_FORWARD ? 2.0F : -2.0F);
	float mod = hSize > 0.0F ? -1.0F : 0.0F;
	glm::vec2 pos { };
	if (dir & Draw::SD_HORIZONTAL)
		pos = glm::vec2(coords.XComponent - hSize - mod, coords.YComponent + 0.5F);
	else
		pos = glm::vec2(coords.XComponent + 0.5F, coords.YComponent - hSize - mod);
	return {
		type,
		Draw::ShipPosition {
			.position = pos,
			.direction = dir
		}
	};
}