#ifndef SHIFVRSKY_SCENERENDERER_HPP
#define SHIFVRSKY_SCENERENDERER_HPP

#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <unordered_map>

#include "gobj/Camera.hpp"
#include "gobj/Projection.hpp"
#include "gobj/SunLight.hpp"
#include "res/Materials.hpp"
#include "res/Textures.hpp"
#include "res/Models.hpp"
#include "Game.hpp"
#include "draw/util/Buffers.hpp"

struct SceneUBO
{

	glm::mat4 projMtx;
	glm::mat4 viewMtx;
	glm::vec3 sunDir;
	float time;

};

struct SceneInfo
{

	Camera camera;
	Projection projection;
	SunLight sun;

};

class SceneRenderer
{
public:

	// Constants
	//-----------

	static constexpr size_t BUFFERING_AMOUNT = Game::BUFFERING_AMOUNT;

	// Lifecycle functions
	//---------------------

	SceneRenderer();

	// Copy & Move [Disabled]
	//------------------------

	SceneRenderer(const SceneRenderer&) = delete;
	SceneRenderer(SceneRenderer&&) = delete;
	SceneRenderer &operator=(const SceneRenderer&) = delete;
	SceneRenderer &operator=(SceneRenderer&&) = delete;

	// Getters
	//---------

	inline SceneInfo &info() noexcept
	{
		return m_info;
	}

	inline const SceneInfo &info() const noexcept
	{
		return m_info;
	}

	// Update functions
	//------------------

	void uploadCamera() noexcept;
	void uploadProjection() noexcept;
	void uploadSun() noexcept;

	// Drawing commands
	//------------------

	inline void draw(const glm::mat4& mtx, const Model& model, const Model::IndexInfo& what, const Material& mat)
	{
		m_instCount++;
		auto& objectsToRender = m_queued[mat.pipeline][&model][mat.texture];
		auto [iter, result] = objectsToRender.try_emplace(what);
		if (result)
			m_cmdCount++;
		iter->second.emplace_back(mtx, mat.color_tilt);
	}

	inline void draw(const glm::mat4& mtx, const Model &model, const Model::Part &part)
	{
		for (auto &[mat, what] : part.meshes)
			draw(mtx, model, what, *mat);
	}

	inline void draw(const glm::mat4& mtx, const Model &model)
	{
		draw(mtx, model, model.all());
	}

	// Utility
	//---------

	void prepareVAOInst(GLuint vao); // TODO: Find a cleaner implementation
	void render();

private:

	// UBO Properties
	//------------

	SceneInfo m_info;
	SceneUBO m_currentUbo;
	StaticMappedBuffer<SceneUBO> m_uboBufs[BUFFERING_AMOUNT] {{},{}};

	// Queue Properties
	//------------------

	struct InstanceData
	{
		glm::mat4 modelMtx;
		glm::vec4 colorTilt;
	};

	struct IndirectCommand
	{
		GLuint  count { };
		GLuint  instanceCount { };
		GLuint  firstIndex { };
		GLuint  baseVertex { };
		GLuint  baseInstance { };
	};

	size_t m_instCount { 0 };
	size_t m_cmdCount { 0 };

	std::unordered_map<ShaderPipeline*,
		std::unordered_map<const Model*,
			std::unordered_map<Texture*,
				std::unordered_map<Model::IndexInfo,
					std::vector<InstanceData>>>>> m_queued;

	DynamicMappedBuffer<InstanceData> m_instBufs[BUFFERING_AMOUNT] {{},{}};
	DynamicMappedBuffer<IndirectCommand> m_cmdBufs[BUFFERING_AMOUNT] {{},{}};

	// Misc Properties
	//-----------------

	uint8_t m_currentFrameBuf = 0;

};

#endif // SHIFVRSKY_SCENERENDERER_HPP