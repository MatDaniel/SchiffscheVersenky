module;

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include "resource.h"

export module Game.ShipInfo;

export constexpr size_t ShipCount = 5;

export enum ShipType
{
	ST_DESTROYER,
	ST_SUBMARINE,
	ST_CRUISER,
	ST_BATTLESHIP,
	ST_CARRIER
};

export struct ShipInfo
{
	const char *const name;
	const uint32_t size;
	const int resId;
	const char *const resName;
};

export ShipInfo ShipInfos[ShipCount] {
	{ "Destroyer",  2, IDR_MESH_SHIP_DSTR, "Ships/Destroyer"  },
	{ "Submarine",  3, IDR_MESH_SHIP_SUBM, "Ships/Submarine"  },
	{ "Cruiser",    3, IDR_MESH_SHIP_CRUS, "Ships/Cruiser"    },
	{ "Battleship", 4, IDR_MESH_SHIP_BTLE, "Ships/Battleship" },
	{ "Carrier",    5, IDR_MESH_SHIP_CARR, "Ships/Carrier"    }
};

// -- Direction --
// First bit is axis (horizontal = 0; vertical = 1)
// Second bit is direction (backwards = 0; forward = 1)
export enum ShipDirection : uint8_t
{

	// AXIS PART
	SD_AXISPART   = 0x1, // first bit
	SD_HORIZONTAL = 0x1, // 10
	SD_VERTICAL   = 0x0, // 00  

	// DIRECTION PART
	SD_DIRPART   = 0x2, // second bit
	SD_BACKWARDS = 0x0, // 00
	SD_FORWARD   = 0x2, // 01

	// CONCRETE DIRECTIONS
	SD_NORTH = 0x0, // 00
	SD_EAST = 0x3, // 11
	SD_SOUTH = 0x2, // 01
	SD_WEST = 0x1, // 10

};

static glm::mat4 generateShipRotation(float rotDeg)
{
	constexpr glm::vec3 forward { 0.0F, 0.0F, 1.0F };
	constexpr glm::vec3 up { 0.0F, 1.0F, 0.0F };
	return glm::rotate(glm::rotate(glm::mat4(1.0F), glm::radians(rotDeg), up), glm::radians(180.0F), forward);
}

export glm::mat4 ShipRotations[4]{
	generateShipRotation(90.0F),
	generateShipRotation(180.0F),
	generateShipRotation(-90.0F),
	generateShipRotation(0.0F)
};

export struct ShipPosition
{
	glm::vec2 position;
	ShipDirection direction;
};