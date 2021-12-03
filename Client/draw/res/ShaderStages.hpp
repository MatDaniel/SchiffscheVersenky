#ifndef SHIFVRSKY_SHADERSTAGES_HPP
#define SHIFVRSKY_SHADERSTAGES_HPP

#include <glad/glad.h>

/**
 * @brief This class represents a shader program.
 */
class ShaderStage
{
public:

    // Lifecycle
    //-----------

    ShaderStage();
    ShaderStage(GLenum type, int rid);
    ~ShaderStage();

    // Assignments
    //-------------

    ShaderStage(ShaderStage&&);
    ShaderStage &operator=(ShaderStage&&);

    ShaderStage(const ShaderStage&) = delete;
    ShaderStage &operator=(const ShaderStage&) = delete;

    // Getters
    //---------

    inline GLbitfield bit() const noexcept
    {
        return m_bit;
    }

    inline GLuint id() const noexcept
    {
        return m_id;
    }

private:

    // Properties
    //------------

    GLbitfield m_bit;
    GLuint m_id;

};

/**
 * @brief Contains instances of shader programs.
 */
namespace ShaderStages
{

    namespace Vertex
    {

        extern ShaderStage cel;

    }

    namespace Fragment
    {

        extern ShaderStage cel;

    }

}

#endif // SHIFVRSKY_SHADERSTAGES_HPP