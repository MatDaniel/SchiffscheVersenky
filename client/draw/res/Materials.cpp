#include "Materials.hpp"

#include "ShaderPipelines.hpp"

const Materials::InstanceMap Materials::map {{
    { "green", &green },
    { "Water", &water },
    { "Border", &border }
}};

Material Materials::green
{
    &ShaderPipelines::cel,
    nullptr, // No texture
    glm::vec4(0.4F, 0.4F, 0.4F, 1.0F) // Dark green
};

Material Materials::water
{
    &ShaderPipelines::cel,
    nullptr, // No texture
    glm::vec4(0.18F, 0.33F, 1.0F, 1.0F) // Dark blue
};

Material Materials::border
{
    &ShaderPipelines::cel,
    nullptr, // No texture
    glm::vec4(1.0F, 1.0F, 1.0F, 1.0F) // White
};