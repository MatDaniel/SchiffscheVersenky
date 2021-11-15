#include "ShaderPrograms.hpp"

#include <glad/glad.h>

#include <iostream>

#include "Resource.hpp"

static void checkLinkErrors(GLuint id)
{

#ifndef NDEBUG

    // Properties
    GLint result;
    GLchar info[1024];
    
    // Get status
    glGetProgramiv(id, GL_COMPILE_STATUS, &result);

    if (!result)
    {

        // Print out error message to the user.
        glGetProgramInfoLog(id, 1024, NULL, info);
        std::cerr << "[ShipRenderer] Error linking shader program [" << id << "]:" << std::endl << info << std::endl;

    }

#endif

}

static void checkCompileErrors(GLuint id)
{

#ifndef NDEBUG

    // Properties
    GLint result;
    GLchar info[1024];
    
    // Get status
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);

    if (!result)
    {

        // Print out error message to the user.
        glGetShaderInfoLog(id, 1024, NULL, info);
        std::cerr << "[ShipRenderer] Error compiling shader [" << id << "]:" << std::endl << info << std::endl;

    }

#endif

}

//-------------//
// SHADER PART //
//-------------//

// Constructors
//--------------

ShaderProgram::Part::Part()
    : m_id(0)
{
}

ShaderProgram::Part::Part(GLenum type, int rid)
    : m_id(glCreateShader(type))
{

    // Get shader resource.
    Resource resource(rid);
        
    // Get resource data.
    const char *code = static_cast<const char*>(resource.data());
    GLint size = static_cast<GLint>(resource.size());

    // Compile shader.
    glShaderSource(m_id, 1, &code, &size);
    glCompileShader(m_id);

    // Check for compile errors.
    checkCompileErrors(m_id);

}

ShaderProgram::Part::Part(Part &&other)
    : m_id(other.m_id)
{
    other.m_id = 0;
}

// Destructors
//-------------

ShaderProgram::Part::~Part()
{
    glDeleteShader(m_id);
}

// Assignments
//-------------

ShaderProgram::Part &ShaderProgram::Part::operator=(Part &&other)
{

    // Copy values
    m_id = other.m_id;

    // Invalidate other instance
    other.m_id = 0;

    // Return this instance
    return *this;

}

//----------------//
// SHADER PROGRAM //
//----------------//

// Constructors
//--------------

ShaderProgram::ShaderProgram()
    : m_id(0)
{
}

ShaderProgram::ShaderProgram(std::initializer_list<Part> parts)
    : m_id(glCreateProgram())
{

    // Attach all shader parts to the program.
    for (const Part &part : parts)
        glAttachShader(m_id, part.id());

    // Link the program with the compiled shaders.
    glLinkProgram(m_id);

}

ShaderProgram::ShaderProgram(ShaderProgram &&other)
    : m_id(other.m_id)
{
    other.m_id = 0;
}

// Destructors
//-------------

ShaderProgram::~ShaderProgram()
{
    glDeleteProgram(m_id);
}

// Assignments
//-------------

ShaderProgram &ShaderProgram::operator=(ShaderProgram &&other)
{

    // Copy values
    m_id = other.m_id;

    // Invalidate other instance
    other.m_id = 0;

    // Return this instance
    return *this;

}

//----------------//
// INSTANTIATIONS //
//----------------//

ShaderProgram ShaderPrograms::cel;