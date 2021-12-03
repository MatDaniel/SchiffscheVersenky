#ifndef SHIFVRSKY_PROJECTION_HPP
#define SHIFVRSKY_PROJECTION_HPP

#include <variant>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct PerspectiveProjection final
{
    glm::mat4 calc(glm::vec2 windowSize) const;
    float fov = glm::radians(90.0F);
};

struct OrthoProjection final
{
    glm::mat4 calc(glm::vec2 windowSize) const;
    float height = 5.0F;
};

typedef std::variant<PerspectiveProjection, OrthoProjection> Projection;

#endif // SHIFVRSKY_PROJECTION_HPP