#include "SceneRenderer.hpp"

#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

#include "Game.hpp"

constexpr GLenum flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;

SceneRenderer::SceneRenderer()
    : m_info()
{
    // Upload defaults to scene ubo
    uploadCamera();
    auto defProj = PerspectiveProjection();
    defProj.fov = 90.0F;
    m_info.projection = defProj;
    uploadProjection();
    uploadSun();
}

void SceneRenderer::initVAOInstancing(size_t buf, GLuint vao)
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
    const glm::vec2 size = Game::windowSize();
    const size_t type = m_info.projection.index();

    switch (type)
    {
        case 0:
            m_currentUbo.projMtx = std::get<0>(m_info.projection).calc(size); // Upload as projection
            break;    
        case 1:
            m_currentUbo.projMtx = std::get<1>(m_info.projection).calc(size); // Upload as ortho
            break;
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

    // Expand instance buffer if necessary
    m_instBufs[m_currentFrameBuf].resize(m_instCount);
    m_cmdBufs[m_currentFrameBuf].resize(m_cmdCount);

    size_t instOffset = 0, cmdOffset = 0;
    for (auto& [pipeline, map] : m_queued)
        for (auto& [model, map] : map)
            for (auto& [texture, map] : map)
            {
                for (auto& [what, vec] : map)
                {
                    // command
                    IndirectCommand cmd { };
                    cmd.firstIndex = what.offset;
                    cmd.count = what.size;
                    cmd.instanceCount = (GLuint)vec.size();
                    cmd.baseInstance = (GLuint)(instOffset/sizeof(InstanceData));
                    std::memcpy(m_cmdBufs[m_currentFrameBuf].ptr() + cmdOffset, &cmd, sizeof(IndirectCommand));
                    cmdOffset += sizeof(IndirectCommand);

                    // instance data
                    auto dataSize = vec.size() * sizeof(InstanceData);
                    std::memcpy(m_instBufs[m_currentFrameBuf].ptr() + instOffset, vec.data(), dataSize);
                    instOffset += dataSize;
                }
            }

    cmdOffset = 0;
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_cmdBufs[m_currentFrameBuf].id());
    for (auto& [pipeline, map] : m_queued)
    {
        glBindProgramPipeline(pipeline->id());
        for (auto& [model, map] : map)
        {

            // Get the vao of the model
            GLuint vao = model->vao(m_currentFrameBuf);

            // Bind the instancing vbo to the model vao.
            // Since the size maybe changed, we just do it for every model every frame.
            glVertexArrayVertexBuffer(vao, 1, m_instBufs[m_currentFrameBuf].id(), 0, sizeof(InstanceData));

            // Bind the vao to the pipeline
            glBindVertexArray(vao);

            for (auto& [texture, map] : map)
            {
                if (texture != nullptr)
                {
                    glBindTextureUnit(0, texture->id());
                }
                glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (const void*)cmdOffset, (GLsizei)map.size(), 0);
                cmdOffset += map.size() * sizeof(IndirectCommand);
            }
        }
    }

    m_instCount = 0;
    m_cmdCount = 0;
    m_queued.clear();

}