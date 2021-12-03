#include "SceneRenderer.hpp"

#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

#include "Game.hpp"

constexpr GLenum flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

SceneRenderer::SceneRenderer()
    : m_info()
{
    uploadSun(); // Upload default sun location to the scene ubo
}

void SceneRenderer::prepareVAOInst(GLuint vao)
{

    // Constants
    constexpr GLuint MODELMTX_ATTRIBINDEX = ShaderPipeline::MODELMTX_ATTRIBINDEX;
    constexpr GLuint COLTILT_ATTRIBINDEX = ShaderPipeline::COLTILT_ATTRIBINDEX;
    // for model matrix
    constexpr size_t rowSize = sizeof(decltype(InstanceData::modelMtx)::row_type);
    constexpr size_t colSize = sizeof(decltype(InstanceData::modelMtx)::col_type);
    constexpr size_t rowAmount = colSize / sizeof(float);

    // Describe the vertex buffer attributes
    for (GLuint i = 0; i < rowAmount; i++)
        glVertexArrayAttribFormat(vao, MODELMTX_ATTRIBINDEX + i, rowSize / sizeof(float), GL_FLOAT, GL_FALSE, (GLuint)(offsetof(InstanceData, modelMtx) + i * rowSize));
    glVertexArrayAttribFormat(vao, (GLuint)COLTILT_ATTRIBINDEX, sizeof(InstanceData::colorTilt) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(InstanceData, colorTilt));

    // Bind attributes to bindings
    for (GLuint i = 0; i < rowAmount; i++)
        glVertexArrayAttribBinding(vao, MODELMTX_ATTRIBINDEX + i, 1);
    glVertexArrayAttribBinding(vao, COLTILT_ATTRIBINDEX, 1);

    // Enable vertex attributes
    for (GLuint i = 0; i < rowAmount; i++)
        glEnableVertexArrayAttrib(vao, MODELMTX_ATTRIBINDEX + i);
    glEnableVertexArrayAttrib(vao, COLTILT_ATTRIBINDEX);

    // Setup divisor per instance
    glVertexArrayBindingDivisor(vao, 1, 1);

}

void SceneRenderer::uploadCamera() noexcept
{
    glm::mat4 view;
    view = glm::lookAt(glm::vec3(), m_info.camera.direction, m_info.camera.up);
    view = glm::translate(view, -m_info.camera.position);
    m_currentUbo.viewMtx = view;
}

void SceneRenderer::uploadProjection() noexcept
{

    constexpr float NEAR_PLANE = 0.1F;
    constexpr float FAR_PLANE = 1000.0F;

    const glm::vec2 size = Game::windowSize();
    const size_t type = m_info.projection.index();

    switch (type)
    {
#define SHIV_CALCPROJ(Index) \
    case Index: \
        m_currentUbo.projMtx = std::get<Index>(m_info.projection).calc(size); \
        break
        SHIV_CALCPROJ(0);
        SHIV_CALCPROJ(1);
#undef SHIV_CALCPROJ
        default:
            std::cerr << "[ShipRenderer] Unknown projection with id " << type << "!" << std::endl;
            break;
    }
}

void SceneRenderer::uploadSun() noexcept
{
    m_currentUbo.sunDir = m_info.sun.direction; // Upload
}

void SceneRenderer::render()
{

    // Update frame buffer index
    m_currentFrameBuf = (m_currentFrameBuf + 1) % 2;

    // Bind SceneUBO
    m_currentUbo.time = Game::time();
    *m_uboBufs[m_currentFrameBuf].ptr() = m_currentUbo;
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_uboBufs[m_currentFrameBuf].id());

    // Expand buffers if necessary
    m_instBufs[m_currentFrameBuf].resize(m_instCount);
    m_cmdBufs[m_currentFrameBuf].resize(m_cmdCount);

    size_t instIndexOffset = 0, cmdIndexOffset = 0;
    for (auto& [pipeline, map] : m_queued)
        for (auto& [model, map] : map)
            for (auto& [texture, map] : map)
                for (auto& [what, vec] : map)
                {
                    // command
                    IndirectCommand cmd { };
                    cmd.firstIndex = what.offset;
                    cmd.count = what.size;
                    cmd.instanceCount = (GLuint)vec.size();
                    cmd.baseInstance = (GLuint)instIndexOffset;
                    m_cmdBufs[m_currentFrameBuf].ptr()[cmdIndexOffset++] = cmd;

                    // instance data
                    auto instSize = vec.size() * sizeof(InstanceData);
                    std::memcpy(m_instBufs[m_currentFrameBuf].ptr() + instIndexOffset, vec.data(), instSize);
                    instIndexOffset += vec.size();
                }

    size_t cmdOffset = 0;
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_cmdBufs[m_currentFrameBuf].id());
    for (auto& [pipeline, map] : m_queued)
    {
        glBindProgramPipeline(pipeline->id());
        for (auto& [model, map] : map)
        {

            // Bind the instancing vbo to the model vao.
            // Since the size maybe changed, we just do it for every model every frame.
            glVertexArrayVertexBuffer(model->vao(), 1, m_instBufs[m_currentFrameBuf].id(), 0, sizeof(InstanceData));

            // Bind the vao to the pipeline
            glBindVertexArray(model->vao());

            for (auto& [texture, map] : map)
            {
                if (texture != nullptr)
                {
                    glBindTextureUnit(0, texture->id());
                }
                glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (const void*)cmdOffset, (GLsizei)map.size(), sizeof(IndirectCommand));
                cmdOffset += map.size() * sizeof(IndirectCommand);
            }
        }
    }

    m_instCount = 0;
    m_cmdCount = 0;
    m_queued.clear();

}