#include "Camera.hpp"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "draw/Game.hpp"
#include "draw/util/UniformBufferObjects.hpp"

namespace
{
    struct CameraUBO
    {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec3 position;
    };
}

Camera::Camera()
    : m_position()
    , m_direction(0.0F, 0.0F, -1.0F)
    , m_up(0.0F, 1.0F, 0.0F)
{
    glGenBuffers(1, &m_id);
    glBindBuffer(GL_UNIFORM_BUFFER, m_id);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBO), nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

Camera::~Camera()
{
    glDeleteBuffers(1, &m_id);
}

Camera::Camera(Camera &&other) noexcept
    : m_id(other.m_id)
    , m_position(other.m_position)
    , m_direction(other.m_direction)
    , m_up(other.m_up)
{
    other.m_id = 0;
}

Camera &Camera::operator=(Camera &&other) noexcept
{

    // Copy values
    m_id = other.m_id;
    m_position = other.m_position;
    m_direction = other.m_direction;
    m_up = other.m_up;

    // Invalidate state of the other object
    other.m_id = 0;

    // Return this instance
    return *this;

}

void Camera::use() const
{

    glm::vec2 windowSize = Game::windowSize(); 

    // Prepare the view ubo for upload.
    CameraUBO ubo;
    ubo.projection = projection(windowSize);
    ubo.view = glm::lookAt(glm::vec3(), m_direction, m_up); // Apply rotation to view
    ubo.view = glm::translate(ubo.view, -m_position); // Apply position to view
    ubo.position = m_position;

    // Bind the ubo to the binding `0`.
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_id);

    // Upload the view ubo data to the gpu.
    glBindBuffer(GL_UNIFORM_BUFFER, m_id);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBO), &ubo); 
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

}

const glm::vec3 &Camera::position() const noexcept
{
    return m_position;
}

void Camera::position(const glm::vec3 &pos) noexcept
{
    m_position = pos;
}

const glm::vec3 &Camera::direction() const noexcept
{
    return m_direction;
}

void Camera::direction(const glm::vec3 &dir) noexcept
{
    m_direction = dir;
}

const glm::vec3 &Camera::up() const noexcept
{
    return m_up;
}

void Camera::up(const glm::vec3 &up) noexcept
{
    m_up = up;
}

PerspectiveCamera::PerspectiveCamera()
    : Camera()
    , m_fov(glm::radians(90.0F))
{
}

float PerspectiveCamera::fov() const noexcept
{
    return m_fov;
}

void PerspectiveCamera::fov(float fov) noexcept
{
    m_fov = fov;
}

glm::mat4 PerspectiveCamera::projection(const glm::vec2 &windowSize) const
{
    return glm::perspective(m_fov, windowSize.x / windowSize.y, 0.1F, 1000.0F);
}

glm::mat4 OrthoCamera::projection(const glm::vec2 &windowSize) const
{
    return glm::mat4(1.0F); // TODO: Calculate orthographic projection
}