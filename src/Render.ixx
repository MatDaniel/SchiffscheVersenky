module;

#include <glad/glad.h>
#include <glm/vec4.hpp>

#include <utility>
#include <cassert>
#include <span>
#include <unordered_map>

#include "Vertex.hpp"
#include "util/ShaderConstants.hpp"
#include "util/FNVHash.hpp"

export module Draw.Render;
import Draw.Renderer.Input;

namespace Draw::Render
{

	// Shortcuts
	using namespace std;

	// -- Shader Stages --
	// This class represents a shader stage in the opengl shader pipeline.
	// It compiles the content of a resource into a shader stage that can
	// be used by the shader pipeline class.

	namespace ShaderStagesImpl
	{

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
				//spdlog::warn("[ShipRenderer] Unknown shader type of value {}, can't convert into a shader bit.", type);
				return 0;
			}

		}

	}

	export class ShaderStage
	{
	public:

		inline ShaderStage() noexcept
			: m_Bit(0)
			, m_Id(0)
		{
		}

		inline ShaderStage(GLenum type, const char* code)
			: m_Bit(ShaderStagesImpl::toBit(type))
		{
			m_Id = glCreateShaderProgramv(type, 1, &code);
			if (type == GL_FRAGMENT_SHADER)
				glProgramUniform1i(m_Id, TEXTURE_LOCATION, 0);
		}

		inline ~ShaderStage()
		{
			glDeleteProgram(m_Id);
		}

		inline ShaderStage(ShaderStage&& Other) noexcept
			: m_Bit(Other.m_Bit)
			, m_Id(Other.m_Id)
		{
			Other.m_Bit = 0;
			Other.m_Id = 0;
		}

		inline ShaderStage& operator=(ShaderStage&& Other) noexcept
		{
			glDeleteProgram(m_Id);
			m_Bit = Other.m_Bit;
			m_Id = Other.m_Id;
			Other.m_Bit = 0;
			Other.m_Id = 0;
			return *this;
		}

		ShaderStage(const ShaderStage&) = delete;
		ShaderStage& operator=(const ShaderStage&) = delete;

		inline GLbitfield Bit() const noexcept
		{
			return m_Bit;
		}

		inline GLuint Id() const noexcept
		{
			return m_Id;
		}

	private:

		GLbitfield m_Bit;
		GLuint m_Id;

	};

	// -- Shader Pipeline --
	// This class contains the full opengl shader pipeline,
	// it's a combination of different shader stages.

	export class ShaderPipeline
	{
	public:

		inline ShaderPipeline()
			: m_Id(0)
		{
		}

		template <typename... StagesVargs>
		inline ShaderPipeline(ShaderStage* Stage, StagesVargs*... Stages)
		{
			static_assert((std::is_same_v<StagesVargs, ShaderStage> && ...), "All arguments are required to be pointers to ShaderStage instances.");
			glCreateProgramPipelines(1, &m_Id);
			glUseProgramStages(m_Id, Stage->Bit(), Stage->Id());
			(glUseProgramStages(m_Id, Stages->Bit(), Stages->Id()), ...);
		}

		inline ~ShaderPipeline()
		{
			glDeleteProgramPipelines(1, &m_Id);
		}

		inline ShaderPipeline(ShaderPipeline&& Other) noexcept
			: m_Id(Other.m_Id)
		{
			Other.m_Id = 0;
		}

		inline ShaderPipeline& operator=(ShaderPipeline&& Other) noexcept
		{
			glDeleteProgramPipelines(1, &m_Id);
			m_Id = Other.m_Id;
			Other.m_Id = 0;
			return *this;
		}

		ShaderPipeline(const ShaderPipeline&) = delete;
		ShaderPipeline& operator=(const ShaderPipeline&) = delete;

		inline GLuint Id() const noexcept
		{
			return m_Id;
		}

	private:

		GLuint m_Id;

	};

	// -- Mapped Buffers --
	// Used as data storage in shaders.
	// `Mapped` means data can be uploaded through a pointer. 

	namespace MappedBuffersImpl
	{
		static constexpr GLenum PRESISTENT_FLAGS = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
	}

	export template <class T>
		class MappedBuffer
	{
	public:

		MappedBuffer()
			: m_Id(0)
		{
		}

		MappedBuffer(MappedBuffer&& Other)
			: m_Id(Other.m_Id)
		{
			Other.m_Id = 0;
		}

		MappedBuffer& operator=(MappedBuffer&& Other)
		{
			glDeleteBuffers(1, &this->m_Id);
			m_Id = Other.m_Id;
			Other.m_Id = 0;
			return *this;
		}

		MappedBuffer(const MappedBuffer& Other) = delete;
		MappedBuffer& operator=(const MappedBuffer& Other) = delete;

		~MappedBuffer()
		{
			glDeleteBuffers(1, &m_Id);
		}

		GLuint Id() const noexcept
		{
			return m_Id;
		}

	protected:

		GLuint m_Id;

	};

	export template <class T, GLsizeiptr Count = 1>
		class ConstMappedBuffer : public MappedBuffer<T>
	{
	public:

		ConstMappedBuffer()
			: MappedBuffer<T>()
		{
		}

		ConstMappedBuffer(span<const T> Data)
			: MappedBuffer<T>()
		{
			glCreateBuffers(1, &this->m_Id);
			glNamedBufferStorage(this->m_Id, sizeof(T) * Data.size(), Data.data(), 0);
		}

		ConstMappedBuffer(ConstMappedBuffer&& Other)
			: MappedBuffer<T>(move(Other))
		{
		}

		ConstMappedBuffer& operator=(ConstMappedBuffer&& Other)
		{
			MappedBuffer<T>::operator=(move(Other));
			return *this;
		}

		ConstMappedBuffer(const ConstMappedBuffer& Other) = delete;
		ConstMappedBuffer& operator=(const ConstMappedBuffer& Other) = delete;

	};

	export template <class T>
		class StaticMappedBuffer : public MappedBuffer<T>
	{
	public:

		StaticMappedBuffer()
			: MappedBuffer<T>()
			, m_Pointer(nullptr)
		{
		}

		StaticMappedBuffer(uint32_t Count, const void* InitialData = nullptr)
			: MappedBuffer<T>()
		{
			glCreateBuffers(1, &this->m_Id);
			glNamedBufferStorage(this->m_Id, sizeof(T) * Count, InitialData, MappedBuffersImpl::PRESISTENT_FLAGS);
			m_Pointer = (T*)glMapNamedBufferRange(this->m_Id, 0, sizeof(T) * Count, MappedBuffersImpl::PRESISTENT_FLAGS);
		}

		StaticMappedBuffer(span<const T> InititalData)
			: StaticMappedBuffer<T>(InititalData.size(), InititalData.data())
		{
		}

		StaticMappedBuffer(StaticMappedBuffer&& Other)
			: MappedBuffer<T>(move(Other))
			, m_Pointer(Other.m_Pointer)
		{
			Other.m_Pointer = nullptr;
		}

		StaticMappedBuffer& operator=(StaticMappedBuffer&& Other)
		{
			MappedBuffer<T>::operator=(move(Other));
			m_Pointer = Other.m_Pointer;
			Other.m_Pointer = nullptr;
			return *this;
		}

		StaticMappedBuffer(const StaticMappedBuffer& Other) = delete;
		StaticMappedBuffer& operator=(const StaticMappedBuffer& Other) = delete;

		T* Pointer() const noexcept
		{
			return m_Pointer;
		}

	private:

		T* m_Pointer;

	};

	export template <class T>
		class DynamicMappedBuffer : public MappedBuffer<T>
	{
	public:

		DynamicMappedBuffer()
			: MappedBuffer<T>()
			, m_Count(0)
			, m_Pointer(nullptr)
		{
		}

		DynamicMappedBuffer(GLsizeiptr Count)
			: DynamicMappedBuffer<T>()
		{
			Resize(Count);
		}

		DynamicMappedBuffer(DynamicMappedBuffer&& Other)
			: MappedBuffer<T>(move(Other))
			, m_Count(Other.m_Count)
			, m_Pointer(Other.m_Pointer)
		{
			Other.m_Count = 0;
			Other.m_Pointer = nullptr;
		}

		DynamicMappedBuffer& operator=(DynamicMappedBuffer&& Other)
		{
			MappedBuffer<T>::operator=(move(Other));
			m_Count = Other.m_Count;
			m_Pointer = Other.m_Pointer;
			Other.m_Count = 0;
			Other.m_Pointer = nullptr;
			return *this;
		}

		DynamicMappedBuffer(const DynamicMappedBuffer& Other) = delete;
		DynamicMappedBuffer& operator=(const DynamicMappedBuffer& Other) = delete;

		void Resize(GLsizeiptr Count) noexcept
		{
			if (m_Count < Count)
			{
				glDeleteBuffers(1, &this->m_Id);
				glCreateBuffers(1, &this->m_Id);
				glNamedBufferStorage(this->m_Id, sizeof(T) * Count, nullptr, MappedBuffersImpl::PRESISTENT_FLAGS);
				m_Pointer = (T*)glMapNamedBufferRange(this->m_Id, 0, sizeof(T) * Count, MappedBuffersImpl::PRESISTENT_FLAGS);
				m_Count = Count;
			}
		}

		T* Pointer() const noexcept
		{
			return m_Pointer;
		}

	private:

		T* m_Pointer;
		GLsizeiptr m_Count;

	};

	// -- Vertex Buffers --
	// Store vertices and indices data that can be bound to draw from it.

	namespace VertexBuffersImpl
	{

		void ConfigureVertexArray(GLuint VertexArray,
			const MappedBuffer<Vertex>& Vertices,
			const MappedBuffer<uint32_t>& Indices)
		{

			/* Per Vertex Data */

			// Describe the vertex buffer attributes
			glVertexArrayAttribFormat(VertexArray, POSITION_ATTRIBINDEX, sizeof(Vertex::position) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
			glVertexArrayAttribFormat(VertexArray, NORMAL_ATTRIBINDEX, sizeof(Vertex::normal) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
			glVertexArrayAttribFormat(VertexArray, TEXCOORDS_ATTRIBINDEX, sizeof(Vertex::texCoords) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoords));

			// Bind attributes to bindings
			glVertexArrayAttribBinding(VertexArray, POSITION_ATTRIBINDEX, 0);
			glVertexArrayAttribBinding(VertexArray, NORMAL_ATTRIBINDEX, 0);
			glVertexArrayAttribBinding(VertexArray, TEXCOORDS_ATTRIBINDEX, 0);

			// Enable vertex attributes
			glEnableVertexArrayAttrib(VertexArray, POSITION_ATTRIBINDEX);
			glEnableVertexArrayAttrib(VertexArray, NORMAL_ATTRIBINDEX);
			glEnableVertexArrayAttrib(VertexArray, TEXCOORDS_ATTRIBINDEX);

			// Bind the buffers to the vertex array object.
			glVertexArrayVertexBuffer(VertexArray, 0, Vertices.Id(), 0, sizeof(Vertex));
			glVertexArrayElementBuffer(VertexArray, Indices.Id());

			// Setup divisor per vertex
			glVertexArrayBindingDivisor(VertexArray, 0, 0);

			/* Per Instance Data */

			// Model matrix constants
			constexpr size_t rowSize = sizeof(decltype(InstanceData::modelMtx)::row_type);
			constexpr size_t colSize = sizeof(decltype(InstanceData::modelMtx)::col_type);
			constexpr size_t rowAmount = colSize / sizeof(float);

			// Describe the vertex buffer attributes
			for (GLuint i = 0; i < rowAmount; i++)
				glVertexArrayAttribFormat(VertexArray, MODELMTX_ATTRIBINDEX + i, rowSize / sizeof(float), GL_FLOAT, GL_FALSE, (GLuint)(offsetof(InstanceData, modelMtx) + i * rowSize));
			glVertexArrayAttribFormat(VertexArray, (GLuint)COLTILT_ATTRIBINDEX, sizeof(InstanceData::colorTilt) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(InstanceData, colorTilt));

			// Bind attributes to bindings
			for (GLuint i = 0; i < rowAmount; i++)
				glVertexArrayAttribBinding(VertexArray, MODELMTX_ATTRIBINDEX + i, 1);
			glVertexArrayAttribBinding(VertexArray, COLTILT_ATTRIBINDEX, 1);

			// Enable vertex attributes
			for (GLuint i = 0; i < rowAmount; i++)
				glEnableVertexArrayAttrib(VertexArray, MODELMTX_ATTRIBINDEX + i);
			glEnableVertexArrayAttrib(VertexArray, COLTILT_ATTRIBINDEX);

			// Setup divisor per instance
			glVertexArrayBindingDivisor(VertexArray, 1, 1);

		}

	}

	export template <typename VertexStorage, typename IndexStorage>
		class Mesh
	{
		static_assert(is_base_of_v<MappedBuffer<Vertex>, VertexStorage>);
		static_assert(is_base_of_v<MappedBuffer<uint32_t>, IndexStorage>);

	public:

		Mesh()
			: m_Vertices()
			, m_Indices()
		{
		}

		Mesh(VertexStorage&& Vertices, IndexStorage&& Indices)
			: m_Vertices(move(Vertices))
			, m_Indices(move(Indices))
		{
			glCreateVertexArrays(1, &m_VertexArray);
			VertexBuffersImpl::ConfigureVertexArray(m_VertexArray, m_Vertices, m_Indices);
		}

		~Mesh()
		{
			glDeleteVertexArrays(1, &m_VertexArray);
		}

		Mesh(Mesh<VertexStorage, IndexStorage>&& Other) noexcept
			: m_VertexArray(Other.m_VertexArray)
			, m_Vertices(move(Other.m_Vertices))
			, m_Indices(move(Other.m_Indices))
		{
			Other.m_VertexArray = 0;
		}

		Mesh& operator=(Mesh<VertexStorage, IndexStorage>&& Other) noexcept
		{
			glDeleteVertexArrays(1, &m_VertexArray);
			m_VertexArray = Other.m_VertexArray;
			m_Vertices = move(Other.m_Vertices);
			m_Indices = move(Other.m_Indices);
			Other.m_VertexArray = 0;
			return *this;
		}

		Mesh(const Mesh<VertexStorage, IndexStorage>&) = delete;
		Mesh<VertexStorage, IndexStorage>& operator=(const Mesh<VertexStorage, IndexStorage>&) = delete;

		GLuint VertexArray() const noexcept
		{
			return m_VertexArray;
		}

		const VertexStorage& Vertices() const noexcept
		{
			return m_Vertices;
		}

		const IndexStorage& Indices() const noexcept
		{
			return m_Indices;
		}

	private:

		GLuint m_VertexArray;
		VertexStorage m_Vertices;
		IndexStorage m_Indices;

	};

	export using ConstMesh = typename Mesh<ConstMappedBuffer<Vertex>, ConstMappedBuffer<uint32_t>>;

	// -- Textures --
	// Represent an image that can be used in shaders.

	namespace TexturesImpl
	{

		constexpr GLenum InternalFormatTranslation[4]
		{
			GL_R8,
			GL_RG8,
			GL_RGB8,
			GL_RGBA8
		};

		constexpr GLenum ImageFormatTranslation[4]
		{
			GL_RED,
			GL_RG,
			GL_RGB,
			GL_RGBA
		};

	}

	export struct TextureLayout {

		// Color Channels
		uint8_t Channels { 0 };

		// Image size
		uint32_t Width { 0 };
		uint32_t Height { 0 };

		// Texture Wrap
		GLenum WrapS;
		GLenum WrapT;

		TextureLayout Wrap(GLenum wrap) const
		{
			auto LayoutResult = *this;
			LayoutResult.WrapS = wrap;
			LayoutResult.WrapT = wrap;
			return LayoutResult;
		}

		TextureLayout WrapRepeat() const
		{
			return Wrap(GL_REPEAT);
		}

		// Texture Scaling
		GLenum MagFilter;
		GLenum MinFilter;

		TextureLayout FilterLinear() const
		{
			auto LayoutResult = *this;
			LayoutResult.MagFilter = GL_LINEAR;
			LayoutResult.MinFilter = GL_LINEAR_MIPMAP_LINEAR;
			return LayoutResult;
		}

		TextureLayout FilterNearest() const
		{
			auto LayoutResult = *this;
			LayoutResult.MagFilter = GL_NEAREST;
			LayoutResult.MinFilter = GL_NEAREST_MIPMAP_NEAREST;
			return LayoutResult;
		}

		// Pixel Data
		const void* Pixels{ nullptr };

	};

	export class Texture
	{
	public:

		Texture()
			: m_Id(0)
		{
		}

		Texture(const TextureLayout Layout)
		{

			// Ensure layout is valid
			assert(Layout.Width > 0 && Layout.Height > 0);
			assert(Layout.Channels > 0 && Layout.Channels <= 4);

			// Create a texture handle.
			glCreateTextures(GL_TEXTURE_2D, 1, &m_Id);

			// Configure texture parameters
			glTextureParameteri(m_Id, GL_TEXTURE_WRAP_S, Layout.WrapS);
			glTextureParameteri(m_Id, GL_TEXTURE_WRAP_T, Layout.WrapT);
			glTextureParameteri(m_Id, GL_TEXTURE_MAG_FILTER, Layout.MagFilter);
			glTextureParameteri(m_Id, GL_TEXTURE_MIN_FILTER, Layout.MinFilter);

			// Allocate texture on the gpu in vram
			glTextureStorage2D(m_Id,
				1, // Levels (Array)
				TexturesImpl::InternalFormatTranslation[Layout.Channels],
				Layout.Width,
				Layout.Height);

			// Upload pixels to vram
			if (Layout.Pixels)
				glTextureSubImage2D(m_Id,
					0, // Level (Array)
					0, // X Offset
					0, // Y Offset
					Layout.Width,
					Layout.Height,
					TexturesImpl::ImageFormatTranslation[Layout.Channels],
					GL_UNSIGNED_BYTE, Layout.Pixels);

			// Generate mipmap for the texture
			glGenerateTextureMipmap(m_Id);

		}

		~Texture()
		{
			glDeleteTextures(1, &m_Id);
		}

		Texture(Texture&& Other) noexcept
			: m_Id(Other.m_Id)
		{
			Other.m_Id = 0;
		}

		Texture& operator=(Texture&& Other) noexcept
		{
			glDeleteTextures(1, &m_Id);
			m_Id = Other.m_Id;
			Other.m_Id = 0;
			return *this;
		}

		Texture(const Texture&) = delete;
		Texture& operator=(const Texture&) = delete;

		GLuint Id() const noexcept
		{
			return m_Id;
		}

	private:

		GLuint m_Id;

	};

	// -- Materials --
	// Explain how to draw.

	export struct Material
	{
		
		Draw::Render::ShaderPipeline* Pipeline { nullptr };
		Draw::Render::Texture* Texture { nullptr };
		glm::vec4 ColorTilt { };

		bool operator==(const Draw::Render::Material& Other) const noexcept {
			return Pipeline == Other.Pipeline
				&& Texture == Other.Texture
				&& ColorTilt == Other.ColorTilt;
		}

	};

	// -- Index Ranges --
	// Contain information how to access the ebo to draw the model.

	export struct IndexRange
	{
		
		uint32_t offset { };
		uint32_t size { };

		bool operator==(const IndexRange& Other) const noexcept
		{
			return offset == Other.offset
				&& size == Other.size;
		}

	};

}

export SHIV_FNV_HASH(Draw::Render::Material);
export SHIV_FNV_HASH(Draw::Render::IndexRange);

namespace Draw::Render
{
	// -- Models --

	/**
	  * @brief Represents a part of the loaded model with multiple materials attached to it.
	  */
	export struct ModelInfo
	{

		unordered_map<Material, IndexRange> Meshes { };

		ModelInfo() = default;
		
		ModelInfo(ModelInfo&& Other) noexcept
			: Meshes(move(Other.Meshes))
		{
		}

		ModelInfo(const ModelInfo& Other) noexcept
			: Meshes(Other.Meshes)
		{
		}

		ModelInfo& operator=(ModelInfo&& Other) noexcept
		{
			Meshes = move(Other.Meshes);
			return *this;
		}

		ModelInfo& operator=(const ModelInfo& Other)
		{
			Meshes = Other.Meshes;
			return *this;
		}

	};

	export template <typename TMesh>
	struct Model
	{
	
		// Properties
		TMesh Mesh;
		ModelInfo Info;
	
		Model()
			: Mesh()
			, Info()
		{
		}

		Model(TMesh&& Mesh, ModelInfo&& Info)
			: Mesh(move(Mesh))
			, Info(move(Info))
		{
		}

		Model(Model<TMesh>&& Other) noexcept
			: Mesh(move(Other.Mesh))
			, Info(move(Other.Info))
		{
		}

		Model& operator=(Model<TMesh>&& Other) noexcept
		{
			Mesh = move(Other.Mesh);
			Info = move(Other.Info);
			return *this;
		}


		Model(const Model<TMesh>& Other) = delete;
		Model& operator=(const Model<TMesh>& Other) = delete;

	};

	export using ConstModel = typename Model<ConstMesh>;

	// -- Frame buffers --
	// Used as a target to draw to.

	namespace FrameBuffersImpl
	{

		constexpr uint32_t PlaneIndices[6]
		{
			0, 1, 3, // TL -> TR -> BL
			1, 2, 3  // TR -> BR -> BL 
		};

		const Vertex InitialPlaneVertices[4]
		{

			// Top-Left
			Vertex {
				.position = { -0.5F, 0.0F, -0.5F },
				.normal = { 0.0F, 1.0F, 0.0F },
				.texCoords = { 0.0F, 1.0F }
			},

			// Top-Right
			Vertex {
				.position = { 0.5F, 0.0F, -0.5F },
				.normal = { 0.0F, 1.0F, 0.0F },
				.texCoords = { 1.0F, 1.0F }
			},

			// Bottom-Right
			Vertex {
				.position = { 0.5F, 0.0F, 0.5F },
				.normal = { 0.0F, 1.0F, 0.0F },
				.texCoords = { 1.0F, 0.0F }
			},

			// Bottom-Left
			Vertex {
				.position = { -0.5F, 0.0F, 0.5F },
				.normal = { 0.0F, 1.0F, 0.0F },
				.texCoords = { 0.0F, 0.0F }
			}

		};

	}

	export class FrameBuffer
	{
	public:

		FrameBuffer()
			: m_Id(0)
			, m_Width(0)
			, m_Height(0)
		{
		}

		~FrameBuffer()
		{
			glDeleteFramebuffers(1, &m_Id);
		}

		FrameBuffer(FrameBuffer&& Other) noexcept
			: m_Id(Other.m_Id)
			, m_Width(Other.m_Width)
			, m_Height(Other.m_Height)
		{
			Other.m_Id = 0;
			Other.m_Height = 0;
			Other.m_Width = 0;
		}

		FrameBuffer& operator=(FrameBuffer&& Other) noexcept
		{
			glDeleteFramebuffers(1, &m_Id);
			m_Id = Other.m_Id;
			m_Width = Other.m_Width;
			m_Height = Other.m_Height;
			Other.m_Id = 0;
			Other.m_Height = 0;
			Other.m_Width = 0;
			return *this;
		}

		FrameBuffer(const FrameBuffer&) = delete;
		FrameBuffer& operator=(const FrameBuffer&) = delete;

		GLuint Id() const noexcept
		{
			return m_Id;
		}

		uint32_t Width() const noexcept
		{
			return m_Width;
		}

		uint32_t Height() const noexcept
		{
			return m_Height;
		}

		void Select() const noexcept
		{
			glBindFramebuffer(GL_FRAMEBUFFER, m_Id);
			glViewport(0, 0, m_Width, m_Height);
		}

		static const FrameBuffer Default;

	protected:

		GLuint m_Id;
		uint32_t m_Width, m_Height;

	};

	export class DefaultFrameBuffer
		: public FrameBuffer
	{
	public:

		DefaultFrameBuffer(uint32_t Width, uint32_t Height)
			: FrameBuffer()
		{
			Resize(Width, Height);
		}

		void Resize(uint32_t Width, uint32_t Height)
		{
			m_Width = Width;
			m_Height = Height;
		}

	};

	export class TextureFrameBuffer
		: public FrameBuffer
	{
	public:

		// Type definitions
		using Mesh = typename Render::Mesh<StaticMappedBuffer<Vertex>, ConstMappedBuffer<uint32_t>>;

		TextureFrameBuffer()
			: FrameBuffer()
			, m_PlaneMesh()
			, m_UnderlyingLayout()
			, m_UndelyingTexture()
		{
		}

		TextureFrameBuffer(const TextureLayout& Layout)
			: FrameBuffer()
			, m_PlaneMesh({span(FrameBuffersImpl::InitialPlaneVertices)}, {span(FrameBuffersImpl::PlaneIndices)})
			, m_UnderlyingLayout(Layout)
			, m_UndelyingTexture(m_UnderlyingLayout)
		{

			// Set framebuffer viewport
			m_Width = m_UnderlyingLayout.Width;
			m_Height = m_UnderlyingLayout.Height;

			// Create frame buffer
			glCreateFramebuffers(1, &m_Id);

			// Apply texture to the frame buffer
			glNamedFramebufferTexture(m_Id, GL_COLOR_ATTACHMENT0, m_UndelyingTexture.Id(), 0);

		}

		TextureFrameBuffer(TextureFrameBuffer&& Other) noexcept
			: FrameBuffer(move(Other))
			, m_PlaneMesh(move(Other.m_PlaneMesh))
			, m_UnderlyingLayout(Other.m_UnderlyingLayout)
			, m_UndelyingTexture(move(Other.m_UndelyingTexture))
		{
		}

		TextureFrameBuffer& operator=(TextureFrameBuffer&& Other) noexcept
		{
			FrameBuffer::operator=(move(Other));
			m_PlaneMesh = move(Other.m_PlaneMesh);
			m_UnderlyingLayout = Other.m_UnderlyingLayout;
			m_UndelyingTexture = move(Other.m_UndelyingTexture);
			return *this;
		}

		TextureFrameBuffer(const TextureFrameBuffer&) = delete;
		TextureFrameBuffer& operator=(const TextureFrameBuffer&) = delete;

		const Texture& UndelyingTexture() const noexcept
		{
			return m_UndelyingTexture;
		}

		const Mesh& PlaneMesh() const noexcept
		{
			return m_PlaneMesh;
		}

		const void Resize(uint32_t Width, uint32_t Height)
		{

			// Setup 
			m_Width = Width;
			m_Height = Height;

			// Rebuild texture if necessary
			bool RebuildTexture = false;
			if (m_Width > m_UnderlyingLayout.Width)
			{
				m_UnderlyingLayout.Width = m_Width;
				RebuildTexture = true;
			}
			if (m_Height > m_UnderlyingLayout.Height)
			{
				m_UnderlyingLayout.Height = m_Height;
				RebuildTexture = true;
			}
			if (RebuildTexture)
			{
				m_UndelyingTexture = Texture(m_UnderlyingLayout);
				glNamedFramebufferTexture(m_Id, GL_COLOR_ATTACHMENT0, m_UndelyingTexture.Id(), 0);
			}

			// Update vertices
			Vertex* vertices = m_PlaneMesh.Vertices().Pointer();
			float UVx = m_Width / (float)m_UnderlyingLayout.Width,
				  UVy = m_Height / (float)m_UnderlyingLayout.Height;
			vertices[0].texCoords.x = UVx; // Top-Left
			vertices[2].texCoords.y = UVy; // Bottom-Right
			vertices[1].texCoords = { UVx, UVy }; // Top-Right

		}

	private:

		Mesh m_PlaneMesh;
		TextureLayout m_UnderlyingLayout;
		Texture m_UndelyingTexture;

	};

}