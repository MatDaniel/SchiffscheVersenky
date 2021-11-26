#ifndef SHIFVRSKY_SHADERPIPELINES_HPP
#define SHIFVRSKY_SHADERPIPELINES_HPP

#include <glad/glad.h>

#include <initializer_list>

#include "ShaderStages.hpp"

/**
 * @brief This class represents a shader program.
 */
class ShaderPipeline
{
public:

    // Constants
    //-----------

    // Uniforms
    static constexpr GLuint TEXTURE_LOCATION = 0; 

    // Input buffers
    static constexpr GLuint POSITION_ATTRIBINDEX = 0;
    static constexpr GLuint NORMAL_ATTRIBINDEX = 1;
    static constexpr GLuint TEXCOORDS_ATTRIBINDEX = 2;
    static constexpr GLuint MODELMTX_ATTRIBINDEX = 3; // Takes up 3,4,5,6
    static constexpr GLuint COLTILT_ATTRIBINDEX = 7;

    // Lifecycle
    //-----------

    ShaderPipeline();
    ShaderPipeline(std::initializer_list<ShaderStage*> stages);
    ~ShaderPipeline();

    // Assignments
    //-------------

    ShaderPipeline(ShaderPipeline&&);
    ShaderPipeline &operator=(ShaderPipeline&&);

    ShaderPipeline(const ShaderPipeline&) = delete;
    ShaderPipeline &operator=(const ShaderPipeline&) = delete;

    // Getters
    //---------

    inline GLuint id() const noexcept
    {
        return m_id;
    }

private:

    // Properties
    //------------

    GLuint m_id;

};

/**
 * @brief Contains instances of shader programs.
 */
namespace ShaderPipelines
{

    extern ShaderPipeline cel;

}

#endif // SHIFVRSKY_SHADERPIPELINES_HPP