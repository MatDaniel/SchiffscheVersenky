#ifndef SHIFVRSKY_SUNLIGHT_HPP
#define SHIFVRSKY_SUNLIGHT_HPP

#include <glad/glad.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

struct SunLightUBO
{

    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 color;

};

class SunLight
{
public:

    // Constrcutors
    //--------------

    SunLight();

    // Destructors
    //-------------

    ~SunLight();

    // Assignments
    //-------------

    SunLight(SunLight&&) noexcept;
    SunLight(const SunLight&) = delete;

    SunLight &operator=(SunLight&&) noexcept;
    SunLight &operator=(const SunLight&) = delete;

    // Utility
    //---------

    void use() const;

    // Getters & Setters
    //-------------------

    inline const glm::vec3 &direction() const noexcept
    {
        return m_data.direction;
    }
    
    inline void direction(const glm::vec3 &val) noexcept
    {
        m_data.direction = val;
    }

    inline const glm::vec3 &color() const noexcept
    {
        return m_data.color;
    }

    inline void color(const glm::vec3 &val) noexcept
    {
        m_data.color = val;
    }

private:

    GLuint m_id;
    SunLightUBO m_data;

};

#endif // SHIFVRSKY_SUNLIGHT_HPP