#include "ShaderPipelines.hpp"

// Lifecycle
//-----------

ShaderPipeline::ShaderPipeline()
    : m_id(0)
{
}

ShaderPipeline::ShaderPipeline(std::initializer_list<ShaderStage*> stages)
{

    // Create pipeline
    glCreateProgramPipelines(1, &m_id);

    // Apply stages to pipeline
    for (auto stage : stages)
        glUseProgramStages(m_id, stage->bit(), stage->id());

}

ShaderPipeline::~ShaderPipeline()
{
    glDeleteProgramPipelines(1, &m_id);
}

// Assignments
//-------------

ShaderPipeline::ShaderPipeline(ShaderPipeline &&other)
    : m_id(other.m_id)
{
    other.m_id = 0;
}

ShaderPipeline &ShaderPipeline::operator=(ShaderPipeline &&other)
{

    // Copy
    m_id = other.m_id;

    // Invalidate
    other.m_id = 0;

    // Exit
    return *this;

}

//----------------//
// INSTANTIATIONS //
//----------------//

ShaderPipeline ShaderPipelines::cel;