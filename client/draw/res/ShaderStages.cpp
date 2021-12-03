#include "ShaderStages.hpp"

#include <glad/glad.h>

#include <iostream>

#include "Resource.hpp"
#include "ShaderPipelines.hpp"

namespace
{

    inline bool checkErrors(GLuint id)
    {

        // Get error log lenght.
        GLint loglen;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &loglen);

        // If present, print out error message.
        if (loglen)
        {
            GLchar *info = new GLchar[loglen];
            glGetProgramInfoLog(id, loglen, NULL, info);
            std::cerr << "[ShipRenderer] Error compiling shader stage [" << id << "]:" << std::endl << info << std::endl;
            delete[] info;
        }

        // Return result.
        return !loglen;

    }

    inline GLbitfield toBit(GLenum type)
    {
        switch (type)
        {
#define SHIV_SHADERBIT(Type) \
    case Type: return Type##_BIT
            SHIV_SHADERBIT(GL_VERTEX_SHADER);
            SHIV_SHADERBIT(GL_FRAGMENT_SHADER);
            SHIV_SHADERBIT(GL_GEOMETRY_SHADER);
            SHIV_SHADERBIT(GL_TESS_CONTROL_SHADER);
            SHIV_SHADERBIT(GL_TESS_EVALUATION_SHADER);  
            SHIV_SHADERBIT(GL_COMPUTE_SHADER);
#undef SHIV_SHADERBIT
            default:
                std::cerr << "[ShipRenderer] Unknown shader type of value " << type << "!" << std::endl; 
                return 0;
        }

    }


}

// Lifecycle
//-----------

ShaderStage::ShaderStage()
    : m_bit(0)
    , m_id(0)
{
}

ShaderStage::ShaderStage(GLenum type, int rid)
    : m_bit(toBit(type))
{
        
    // Get resource data.
    auto code = Resource(rid).str();
    auto codes = code.c_str();

    // Compile shader.
    m_id = glCreateShaderProgramv(type, 1, &codes);

    // Check for errors.
    if (!checkErrors(m_id)) return;

    // Prepare for materials
    if (type == GL_FRAGMENT_SHADER)
    {
        glProgramUniform1ui(m_id, ShaderPipeline::TEXTURE_LOCATION, 0);
    }

}

ShaderStage::~ShaderStage()
{
    glDeleteProgram(m_id);
}

// Assignments
//-------------

ShaderStage::ShaderStage(ShaderStage &&other)
    : m_bit(other.m_bit)
    , m_id(other.m_id)
{
    other.m_bit = 0;
    other.m_id = 0;
}

ShaderStage &ShaderStage::operator=(ShaderStage &&other)
{

    // Copy values
    m_bit = other.m_bit;
    m_id = other.m_id;

    // Invalidate other instance
    other.m_bit = 0;
    other.m_id = 0;

    // Return this instance
    return *this;

}

//----------------//
// INSTANTIATIONS //
//----------------//

ShaderStage ShaderStages::Vertex::cel;
ShaderStage ShaderStages::Fragment::cel;