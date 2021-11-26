#ifndef SHIFVRSKY_PROJECTION_HPP
#define SHIFVRSKY_PROJECTION_HPP

#include <variant>

#include <glad/glad.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct AbstractProjection
{

    // Utility
    //---------

    virtual glm::mat4 calc(const glm::vec2 &windowSize) const = 0;

};

struct PerspectiveProjection final : public AbstractProjection
{

    // Utility
    //---------

    virtual glm::mat4 calc(const glm::vec2 &windowSize) const override;

    // Properties
    //------------

    float fov;

};

struct OrthoProjection final : public AbstractProjection
{

    // Utility
    //---------

    virtual glm::mat4 calc(const glm::vec2 &windowSize) const override;

};

typedef std::variant<PerspectiveProjection, OrthoProjection> Projection;

#endif // SHIFVRSKY_PROJECTION_HPP