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

export module Draw.Renderer;
import Draw.Render.Input;
import Draw.Render;
import Draw.Window;
import Draw.Timings;

namespace Draw
{

	using namespace std;
	using namespace glm;
	using namespace Draw;
	using namespace Draw::Render;
	using namespace Draw::Render::Input;

	static constexpr float NEAR_PLANE = 0.1F;
	static constexpr float FAR_PLANE = 1000.0F;

	export class SceneData
	{
	public:

		// -- Sun Light --
		// Contains info on how to render the sun light.
		// Used to generate correct lighting.

		struct SunLightInfo final
		{
			vec3 Direction{ -1.0F, -1.0F, 0.0F };
		};

		// -- Projection --
		// The projection is describes how to calculate the projection matrix in the MVP.

		struct PerspectiveProjectionInfo final
		{

			mat4 Calculate(vec2 windowSize) const
			{
				return perspective(FieldOfView, windowSize.x / windowSize.y, NEAR_PLANE, FAR_PLANE);
			}

			float FieldOfView{ radians(90.0F) };

		};

		struct OrthoProjectionInfo final
		{

			mat4 Calculate(vec2 windowSize) const
			{
				float width = (windowSize.x / windowSize.y) * Height;
				float heightHalf = Height / 2.0F;
				float widthHalf = width / 2.0F;
				return ortho(
					-widthHalf, widthHalf,
					-heightHalf, heightHalf,
					NEAR_PLANE, FAR_PLANE);
			}

			float Height{ 5.0F };

		};

		using ProjectionInfo = variant<PerspectiveProjectionInfo, OrthoProjectionInfo>;

		// -- Camera --
		// Contains info of the position and orientation of the camera.
		// Used to evaluate the view matrix in the MVP.

		struct CameraInfo final
		{

			mat4 Calculate()
			{
				return translate(lookAt({}, Direction, Up), -Position);
			}

			vec3 Position{ };
			vec3 Direction{ 0.0F, 0.0F, -1.0F };
			vec3 Up{ 0.0F, 1.0F, 0.0F };

		};

		// -- Clear Info --

		struct ClearInfo
		{
			vec4 Color{ 0.0F, 0.0F, 0.0F, 1.0F };
			GLenum Bits{ GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT };
		};

		// -- Depth Info --

		struct DepthInfo
		{
			bool Enabled { true };
			GLenum Comparison { GL_LESS };
		};

		// -- Scene Implementation --

		SceneData::ClearInfo Clear { };
		SceneData::DepthInfo Depth { };
		SceneData::CameraInfo Camera { };
		SceneData::ProjectionInfo Projection { };
		SceneData::SunLightInfo Sun { };

		// Constructors
		//--------------

		SceneData(const FrameBuffer* TargetFB)
			: m_TargetFB(TargetFB)
		{
			// Upload defaults
			UploadSun();
			UploadCamera();
			UploadProjection();
		}

		// Update functions
		//------------------

		void UploadCamera() noexcept
		{
			m_CurrentUBO.viewMtx = Camera.Calculate();
		}

		void UploadProjection() noexcept
		{

			const vec2 ViewportSize(m_TargetFB->Width(), m_TargetFB->Height());
			const size_t ProjectionType = Projection.index();

			// Exit if window is minimized
			if (ViewportSize.x == 0.0F || ViewportSize.y == 0.0F)
				return;

			switch (ProjectionType)
			{
#define SHIV_CALCPROJ(Index) \
    case Index: \
        m_CurrentUBO.projMtx = std::get<Index>(Projection).Calculate(ViewportSize); \
        break
				SHIV_CALCPROJ(0);
				SHIV_CALCPROJ(1);
#undef SHIV_CALCPROJ
			default:
				std::cerr << "[ShipRenderer] Unknown projection with id " << ProjectionType << "!" << std::endl;
				break;
			}
		}

		void UploadSun() noexcept
		{
			m_CurrentUBO.sunDir = Sun.Direction;
		}

		// Getter functions
		//------------------

		const auto& GetUBO() const noexcept
		{
			return m_CurrentUBO;
		}

		// Management functions
		//----------------------

		void BeginFrame()
		{

			// Change buffer
			m_CurrentFrameBuffer = (m_CurrentFrameBuffer + 1) % BUFFERING_AMOUNT;

			// Select target frame buffer
			m_TargetFB->Select();

			// Clear color
			if (Clear.Color.a > 0.0F)
			{
				glClearColor(Clear.Color.r, Clear.Color.g, Clear.Color.b, Clear.Color.a);
				glClear(Clear.Bits);
			}

			// Prepare drawing
			PrepareDraw();

		}

		void ProceedFrame()
		{
		
			// Select target frame buffer
			m_TargetFB->Select();

			// Prepare drawing
			PrepareDraw();
		
		}

		void PrepareDraw()
		{

			// Setup Depth
			if (Depth.Enabled)
			{
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(Depth.Comparison);
			}
			else
			{
				glDisable(GL_DEPTH_TEST);
			}

			// Bind scene UBO
			m_CurrentUBO.time = Timings::TimeSinceStart();
			*m_BuffersUBO[m_CurrentFrameBuffer].Pointer() = m_CurrentUBO;
			glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_BuffersUBO[m_CurrentFrameBuffer].Id());
		
		}

	private:

		const FrameBuffer* m_TargetFB;
		SceneUBO m_CurrentUBO { };
		StaticMappedBuffer<SceneUBO> m_BuffersUBO[BUFFERING_AMOUNT] {{1},{1}};
		uint8_t m_CurrentFrameBuffer{ 0 };

	};

	// -- Renderer --
	// Here starts the renderer implementation. It collects, prepares,
	// uploads vertices and materials to the gpu and draws to the target
	// frame buffer using indirect commands.

	export class Renderer
	{
	public:

		Renderer(const Render::FrameBuffer* TargetFB)
			: m_TargetFB(TargetFB)
		{
		}

		Renderer(Renderer&& other) = delete;
		Renderer& operator=(Renderer&& other) = delete;
		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		// Draw functions
		//----------------

		void Draw(const mat4& Transformation, const GLuint VertexArray, const IndexRange& Range, const Material& Material)
		{
			m_InstanceCount++;
			auto& objectsToRender = m_Queued[Material.Pipeline][VertexArray][Material.Texture];
			auto [Iter, Result] = objectsToRender.try_emplace(Range);
			if (Result)
				m_CommandCount++;
			Iter->second.emplace_back(Transformation, Material.ColorTilt);
		}

		void Draw(const mat4& Transformation, const GLuint VertexArray, const ModelInfo& Info)
		{
			for (auto& [Material, Range] : Info.Meshes)
				Draw(Transformation, VertexArray, Range, Material);
		}

		void Draw(const mat4& Transformation, const GLuint VertexArray, const ModelInfo& Info,
			const vec4& ColorTiltOverride)
		{
			for (auto& [Material, Range] : Info.Meshes)
			{
				auto MaterialCopy = Material;
				MaterialCopy.ColorTilt = ColorTiltOverride;
				Draw(Transformation, VertexArray, Range, MaterialCopy
				);
			}
		}

		void Draw(const mat4& Transformation, const GLuint VertexArray, const ModelInfo& Info,
			const vec4& ColorTiltOverride, float Strength)
		{
			for (auto& [Material, Range] : Info.Meshes)
			{
				auto MaterialCopy = Material;
				MaterialCopy.ColorTilt *= 1.0F - Strength;
				MaterialCopy.ColorTilt += ColorTiltOverride * Strength;
				Draw(Transformation, VertexArray, Range, MaterialCopy);
			}
		}

		void Draw(const mat4& Transformation, const auto* Model)
		{
			Draw(Transformation, Model->Mesh.VertexArray(), Model->Info);
		}

		void Draw(const mat4& Transformation, const auto* Model,
			const vec4& ColorTiltOverride)
		{
			Draw(Transformation, Model->Mesh.VertexArray(), Model->Info, ColorTiltOverride);
		}

		void Draw(const mat4& Transformation, const auto* Model,
			const vec4& ColorTiltOverride, float Strength)
		{
			Draw(Transformation, Model->Mesh.VertexArray(), Model->Info, ColorTiltOverride, Strength);
		}

		// Render functions
		//------------------

		void Render()
		{

			// Update frame buffer index
			m_CurrentFrameBuffer = (m_CurrentFrameBuffer + 1) % 2;

			// Expand buffers if necessary
			m_BuffersInstances[m_CurrentFrameBuffer].Resize(m_InstanceCount);
			m_BuffersCommands[m_CurrentFrameBuffer].Resize(m_CommandCount);

			size_t instIndexOffset = 0, cmdIndexOffset = 0;
			for (auto& [pipeline, map] : m_Queued)
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
							m_BuffersCommands[m_CurrentFrameBuffer].Pointer()[cmdIndexOffset++] = cmd;

							// instance data
							auto instSize = vec.size() * sizeof(InstanceData);
							memcpy(m_BuffersInstances[m_CurrentFrameBuffer].Pointer() + instIndexOffset, vec.data(), instSize);
							instIndexOffset += vec.size();
						}

			size_t cmdOffset = 0;
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_BuffersCommands[m_CurrentFrameBuffer].Id());
			for (auto& [pipeline, map] : m_Queued)
			{
				glBindProgramPipeline(pipeline->Id());
				for (auto& [vao, map] : map)
				{

					// Bind the instancing vbo to the model vao.
					// Since the size maybe changed, we just do it for every model every frame.
					glVertexArrayVertexBuffer(vao, 1, m_BuffersInstances[m_CurrentFrameBuffer].Id(), 0, sizeof(InstanceData));

					// Bind the vao to the pipeline
					glBindVertexArray(vao);

					for (auto& [texture, map] : map)
					{
						if (texture != nullptr)
						{
							glBindTextureUnit(0, texture->Id());
						}
						glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, (const void*)cmdOffset, (GLsizei)map.size(), sizeof(IndirectCommand));
						cmdOffset += map.size() * sizeof(IndirectCommand);
					}
				}
			}

			m_InstanceCount = 0;
			m_CommandCount = 0;
			m_Queued.clear();

		}

	private:

		const FrameBuffer* m_TargetFB;

		size_t m_InstanceCount { 0 };
		size_t m_CommandCount { 0 };

		unordered_map<const ShaderPipeline*,
			unordered_map<GLuint, // Vertex Array Object
			unordered_map<const Texture*,
			unordered_map<IndexRange,
			vector<InstanceData>>>>> m_Queued;

		DynamicMappedBuffer<InstanceData> m_BuffersInstances[BUFFERING_AMOUNT] { {},{} };
		DynamicMappedBuffer<IndirectCommand> m_BuffersCommands[BUFFERING_AMOUNT] { {},{} };

		uint8_t m_CurrentFrameBuffer { 0 };

	};

}