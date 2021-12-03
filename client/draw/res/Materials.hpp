#ifndef SHIFVRSKY_MATERIALS_HPP
#define SHIFVRSKY_MATERIALS_HPP

#include <glm/vec4.hpp>
#include <glad/glad.h>

#include <unordered_map>

#include "Textures.hpp"
#include "ShaderPipelines.hpp"
#include "util/fnv_hash.hpp"

struct Material
{
    ShaderPipeline *pipeline { nullptr };
    Texture *texture { nullptr };
    glm::vec4 color_tilt { };
};

SHIV_FNV_HASH(Material)

namespace Materials
{

    typedef std::unordered_map<std::string, Material*> InstanceMap;
    extern const InstanceMap map;

    extern Material green;
    extern Material water;
    extern Material border;

}

#endif // SHIFVRSKY_MATERIALS_HPP