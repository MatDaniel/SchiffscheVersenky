#ifndef SHIFVRSKY_CAMERA_HPP
#define SHIFVRSKY_CAMERA_HPP

#include <glad/glad.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

class Camera
{
public:

    // Constructors
    //--------------

    Camera();

    // Destructors
    //-------------

    ~Camera();

    // Assignments
    //-------------

    Camera(Camera&&) noexcept;
    Camera(const Camera&) = delete;

    Camera &operator=(Camera&&) noexcept;
    Camera &operator=(const Camera&) = delete;

    // Utility
    //---------

    void use() const;

    // Getters & Setters
    //-------------------

    const glm::vec3 &position() const noexcept;
    void position(const glm::vec3 &pos) noexcept;

    const glm::vec3 &direction() const noexcept;
    void direction(const glm::vec3 &pos) noexcept;

    const glm::vec3 &up() const noexcept;
    void up(const glm::vec3 &pos) noexcept;

protected:

    virtual glm::mat4 projection(const glm::vec2 &windowSize) const = 0;

private:

    // ubo
    GLuint m_id;

    // transform
    glm::vec3 m_position;
    glm::vec3 m_direction;
    glm::vec3 m_up;

};

class PerspectiveCamera final : public Camera
{
public:

    PerspectiveCamera();

    float fov() const noexcept;
    void fov(float fov) noexcept;

protected:

    virtual glm::mat4 projection(const glm::vec2 &windowSize) const override;

private:

    float m_fov;

};

class OrthoCamera final : public Camera
{
protected:

    virtual glm::mat4 projection(const glm::vec2 &windowSize) const override;

private:

};

#endif // SHIFVRSKY_CAMERA_HPP