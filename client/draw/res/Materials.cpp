#include "Materials.hpp"

#include "ShaderPipelines.hpp"

const Materials::InstanceMap Materials::map {{
    "green", &green
}};

Material Materials::green
{
    &ShaderPipelines::cel,
    nullptr, // No texture
    glm::vec4(0.4F, 0.4F, 0.4F, 1.0F) // Dark green
};