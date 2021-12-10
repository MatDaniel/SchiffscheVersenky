#pragma once

#include "util/FNVHash.hpp"

// -- Vertex --
// Contains data of a point.

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
};

export inline bool operator==(const Vertex& lhs, const Vertex& rhs) {
	return lhs.position == rhs.position
		&& lhs.normal == rhs.normal
		&& lhs.texCoords == rhs.texCoords;
}

export SHIV_FNV_HASH(Vertex)