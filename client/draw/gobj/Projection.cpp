#include "Projection.hpp"

#include <glm/gtc/matrix_transform.hpp>

glm::mat4 PerspectiveProjection::calc(const glm::vec2 &windowSize) const
{
    return glm::perspective(fov, windowSize.x / windowSize.y, 0.1F, 1000.0F);
}

glm::mat4 OrthoProjection::calc(const glm::vec2 &windowSize) const
{
    return glm::mat4(1.0F); // TODO: Calculate orthographic projection
}