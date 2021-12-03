#ifndef SHIFVRSKY_VERTEX_HPP
#define SHIFVRSKY_VERTEX_HPP

#include <glm/glm.hpp>

#include "../../util/fnv_hash.hpp"

struct Vertex {

    // Position
    glm::vec3 position;

    // Normal
    glm::vec3 normal;

    // UV
    glm::vec2 texCoords;


};

inline bool operator==(const Vertex& lhs, const Vertex& rhs) {
    return lhs.position == rhs.position
        && lhs.normal == rhs.normal
        && lhs.texCoords == rhs.texCoords;
}

SHIV_FNV_HASH(Vertex)

#endif // SHIFVRSKY_VERTEX_HPP