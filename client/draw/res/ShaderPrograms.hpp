#ifndef SHIFVRSKY_SHADERPROGRAMS_HPP
#define SHIFVRSKY_SHADERPROGRAMS_HPP

#include <glad/glad.h>

#include <initializer_list>

#include "Materials.hpp"

/**
 * @brief This class represents a shader program.
 */
class ShaderProgram
{
public:

    // Helper Classes
    //----------------

    /**
     * @brief This represents a shader in a shader program.
     */
    class Part
    {
    public:

        // Constructors
        //--------------

        Part();
        Part(GLenum type, int rid);

        Part(Part&&);
        Part(const Part&) = delete;

        // Destructors
        //-------------

        ~Part();

        // Assignments
        //-------------

        Part &operator=(Part&&);
        Part &operator=(const Part&) = delete;

        // Getters
        //------------

        inline GLuint id() const noexcept
        {
            return m_id;
        }

    private:

        // Properties
        //------------

        GLuint m_id;

    };

    // Constructors
    //--------------

    ShaderProgram();
    ShaderProgram(std::initializer_list<Part> parts);

    // Destructors
    //-------------

    ~ShaderProgram();

    // Assignments
    //-------------

    ShaderProgram(ShaderProgram&&);
    ShaderProgram(const ShaderProgram&) = delete;

    ShaderProgram &operator=(ShaderProgram&&);
    ShaderProgram &operator=(const ShaderProgram&) = delete;

    // Getters
    //---------

    inline GLuint id() const noexcept
    {
        return m_id;
    }

    // Utility
    //---------

    inline void use() const noexcept
    {
        glUseProgram(m_id);
    }

private:

    // Properties
    //------------

    GLuint m_id;

};

/**
 * @brief Contains instances of shader programs.
 */
namespace ShaderPrograms
{

    extern ShaderProgram cel;

}

#endif // SHIFVRSKY_SHADERPROGRAMS_HPP