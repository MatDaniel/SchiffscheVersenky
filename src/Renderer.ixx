module;

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <unordered_map>
#include <variant>
#include <typeinfo>
#include <type_traits>
#include <utility>
#include <array>
#include <iostream>

#include "util/ShaderConstants.hpp"

export module Draw.Renderer;
import Draw.Renderer.Input;
import Draw.Resources;
import Draw.Buffers;
import Draw.Engine;
import Draw.Window;
import Draw.Timings;

// -- Camera --
// Contains info of the position and orientation of the camera.
// Used to evaluate the view matrix in the MVP.

export struct Camera final
{

	glm::vec3 position { };
	glm::vec3 direction { 0.0F, 0.0F, -1.0F };
	glm::vec3 up { 0.0F, 1.0F, 0.0F };

};

// -- Sun Light --
// Contains info on how to render the sun light.
// Used to generate correct lighting.

export struct SunLight final
{
	glm::vec3 direction { -1.0F, -1.0F, 0.0F };
};

// -- Projection --
// The projection is describes how to calculate the projection matrix in the MVP.

static constexpr float NEAR_PLANE = 0.1F;
static constexpr float FAR_PLANE = 1000.0F;

export struct PerspectiveProjection final
{

	inline glm::mat4 calc(glm::vec2 windowSize) const
	{
		return glm::perspective(fov, windowSize.x / windowSize.y, NEAR_PLANE, FAR_PLANE);
	}

	float fov { glm::radians(90.0F) };

};

export struct OrthoProjection final
{
	
	inline glm::mat4 calc(glm::vec2 windowSize) const
	{
		float width = (windowSize.x / windowSize.y) * height;
		float heightHalf = height / 2.0F;
		float widthHalf = width / 2.0F;
		return glm::ortho(
			-widthHalf, widthHalf,
			-heightHalf, heightHalf,
			NEAR_PLANE, FAR_PLANE);
	}

	float height { 5.0F };

};

export using Projection = std::variant<PerspectiveProjection, OrthoProjection>;

// -- SceneRenderer & it's utilities --
// The scene renderer combines the resources and scene info,
// prepares the data for the gpu and sends the draw commands.
// Used to render the scene.

export struct SceneInfo
{
	Camera camera { };
	Projection projection { };
	SunLight sun { };
};

namespace
{
	struct IndirectCommand
	{
		GLuint  count{ };
		GLuint  instanceCount{ };
		GLuint  firstIndex{ };
		GLuint  baseVertex{ };
		GLuint  baseInstance{ };
	};
}

export class SceneRenderer
{
public:

	SceneRenderer()
		: m_info()
	{
		// Upload defaults
		uploadSun();
		uploadCamera();
		uploadProjection();
	}

	SceneRenderer(SceneRenderer&& other) = delete;
	SceneRenderer& operator=(SceneRenderer&& other) = delete;
	SceneRenderer(const SceneRenderer&) = delete;
	SceneRenderer& operator=(const SceneRenderer&) = delete;

	SceneInfo& info() noexcept
	{
		return m_info;
	}

	const SceneInfo& info() const noexcept
	{
		return m_info;
	}

	const SceneUBO& ubo() const noexcept
	{
		return m_currentUbo;
	}

	// Update functions
	//------------------

	inline void uploadCamera() noexcept
	{
		glm::mat4 view;
		view = glm::lookAt(glm::vec3(), m_info.camera.direction, m_info.camera.up);
		view = glm::translate(view, -m_info.camera.position);
		m_currentUbo.viewMtx = view;
	}
		
	inline void uploadProjection() noexcept
	{
		constexpr float NEAR_PLANE = 0.1F;
		constexpr float FAR_PLANE = 1000.0F;

		const glm::vec2 size = Window::Properties::WindowSize;
		const size_t type = m_info.projection.index();

		// Exit if window is minimized
		if (size.x == 0.0F || size.y == 0.0F)
			return;

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
	
	inline void uploadSun() noexcept
	{
		m_currentUbo.sunDir = m_info.sun.direction;
	}

	inline void draw(const glm::mat4& mtx, const Model* model, const Model::IndexInfo& what, const Material& mat)
	{
		m_instCount++;
		auto& objectsToRender = m_queued[mat.pipeline][model][mat.texture];
		auto [iter, result] = objectsToRender.try_emplace(what);
		if (result)
			m_cmdCount++;
		iter->second.emplace_back(mtx, mat.color_tilt);
	}

	inline void draw(const glm::mat4& mtx, const Model* model, const Model::Part& part)
	{
		for (auto& [mat, what] : part.meshes)
			draw(mtx, model, what, mat);
	}

	inline void draw(const glm::mat4& mtx, const Model* model)
	{
		for (auto& part : model->parts())
			draw(mtx, model, part);
	}

	inline void render()
	{

		// Update frame buffer index
		m_currentFrameBuf = (m_currentFrameBuf + 1) % 2;

		// Bind SceneUBO
		m_currentUbo.time = Timings::TimeSinceStart();
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
						IndirectCommand cmd{ };
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

private:

	SceneInfo m_info;
	SceneUBO m_currentUbo;
	StaticMappedBuffer<SceneUBO> m_uboBufs[BUFFERING_AMOUNT] {{},{}};

	size_t m_instCount{ 0 };
	size_t m_cmdCount{ 0 };

	std::unordered_map<const ShaderPipeline*,
		std::unordered_map<const Model*,
		std::unordered_map<const Texture*,
		std::unordered_map<Model::IndexInfo,
		std::vector<InstanceData>>>>> m_queued;

	DynamicMappedBuffer<InstanceData> m_instBufs[BUFFERING_AMOUNT] {{},{}};
	DynamicMappedBuffer<IndirectCommand> m_cmdBufs[BUFFERING_AMOUNT] {{},{}};

	uint8_t m_currentFrameBuf = 0;

};