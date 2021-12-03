#include "Projection.hpp"

#include <glm/gtc/matrix_transform.hpp>

static constexpr float NEAR_PLANE = 0.1F;
static constexpr float FAR_PLANE = 1000.0F;

glm::mat4 PerspectiveProjection::calc(glm::vec2 windowSize) const
{
    return glm::perspective(fov, windowSize.x / windowSize.y, NEAR_PLANE, FAR_PLANE);
}

glm::mat4 OrthoProjection::calc(glm::vec2 windowSize) const
{
    float width = (windowSize.x / windowSize.y) * height;
    float heightHalf = height / 2.0F;
    float widthHalf = width / 2.0F;
    return glm::ortho(
        -widthHalf, widthHalf,
        -heightHalf, heightHalf,
        NEAR_PLANE, FAR_PLANE);
}