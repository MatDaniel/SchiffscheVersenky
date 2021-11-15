#include "SunLight.hpp"

SunLight::SunLight()
    : m_data({
        glm::vec3(-1.0F, -1.0F, 0.0F), // direction
        glm::vec3(400.0F, 400.0F, 400.0F)    // color
    })
{
    glGenBuffers(1, &m_id);
    glBindBuffer(GL_UNIFORM_BUFFER, m_id);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SunLightUBO), nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

SunLight::~SunLight()
{
    glDeleteBuffers(1, &m_id);
}

SunLight::SunLight(SunLight &&other) noexcept
    : m_id(other.m_id)
    , m_data(other.m_data)
{
    other.m_id = 0;
}

SunLight &SunLight::operator=(SunLight &&other) noexcept
{

    // Copy values
    m_id = other.m_id;
    m_data = other.m_data;

    // Invalidate state of the other object
    other.m_id = 0;

    // Return this instance
    return *this;

}

void SunLight::use() const
{
    
    // Bind the ubo to the binding `1`.
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_id);

    // Upload the view ubo data to the gpu.
    glBindBuffer(GL_UNIFORM_BUFFER, m_id);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(SunLightUBO), &m_data); 
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

}