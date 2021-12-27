module;

#include <Windows.h>
#include <WinUser.h>

#include <glad/glad.h>
#include <stb_image.h>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader.h>

#include <cstdint>
#include <streambuf>
#include <istream>
#include <assert.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <span>

#include "util/ShaderConstants.hpp"
#include "util/FNVHash.hpp"
#include "Vertex.hpp"

#include "resource.h"

export module Draw.Resources;
import Draw.Renderer.Input;
import Game.ShipInfo;

// -- Resource Store --
// A unified place for any Resource.

template <class T>
struct ResourceStore
{
	inline static std::unordered_map<std::string, std::unique_ptr<T>> value { };
};

export namespace Resources
{

	template <class T, class... Args>
	inline T* emplace(const std::string& name, Args&&... args)
	{
		return (ResourceStore<T>::value[name] = std::make_unique<T>(std::forward<Args>(args)...)).get();
	}

	template <class T>
	inline void remove(const std::string& name)
	{
		auto& resources = ResourceStore<T>::value;
		auto iter = resources.find(name);
		if (iter != resources.end())
			resources.erase(iter);
	}

	template <class T>
	inline T* find(const std::string& name)
	{
		auto& resources = ResourceStore<T>::value;
		auto iter = resources.find(name);
		return iter != resources.end() ? iter->second.get() : nullptr;
	}

	template <class T>
	inline const auto& map() noexcept
	{
		return ResourceStore<T>::value;
	}

	template <class T>
	inline void clear() noexcept
	{
		ResourceStore<T>::value.clear();
	}

}

// -- Input Memory Stream (modified version) --
// From https://stackoverflow.com/a/13059195
// Used to create a input stream of a fixed memory region.

struct membuf : std::streambuf {

	inline membuf(char const* base, size_t size)
	{
		char* p(const_cast<char*>(base));
		this->setg(p, p, p + size);
	}

};

struct imemstream : virtual membuf, std::istream {

    inline imemstream(char const* base, size_t size)
		: membuf(base, size)
		, std::istream(static_cast<std::streambuf*>(this))
	{
	}

};

// -- Resource --
// An abstraction to the resource in an executable.

export struct Resource
{

    inline Resource(int id, const char* type = "RT_RCDATA")
    {
        auto hResource = FindResourceA(nullptr, MAKEINTRESOURCEA(id), type);
        assert(hResource != 0);
        auto hMemory = LoadResource(nullptr, hResource);
        assert(hMemory != 0);
        m_size = SizeofResource(nullptr, hResource);
        m_data = LockResource(hMemory);
    }

    inline imemstream stream() const
    {
        return imemstream((const char*)m_data, m_size);
    }

    inline std::string str() const
    {
        return std::string((const char*)m_data, m_size);
    }

    inline void* data() const noexcept
    {
        return m_data;
    }

    inline uint32_t size() const noexcept
    {
        return m_size;
    }

private:
    
    uint32_t m_size;
    void* m_data;

};

// -- Shader Stage --
// This class represents a shader stage in the opengl shader pipeline.
// It compiles the content of a resource into a shader stage that can
// be used by the shader pipeline class.

namespace
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
			spdlog::warn("[ShipRenderer] Unknown shader type of value {}, can't convert into a shader bit.", type);
			return 0;
		}

	}

}

export class ShaderStage
{
public:

    inline ShaderStage() noexcept
        : m_bit(0)
        , m_id(0)
    {
    }

    inline ShaderStage(GLenum type, const Resource& res)
        : m_bit(toBit(type))
    {
        auto code = res.str();
        auto codes = code.c_str();
        m_id = glCreateShaderProgramv(type, 1, &codes);
        if (type == GL_FRAGMENT_SHADER)
            glProgramUniform1i(m_id, TEXTURE_LOCATION, 0);
    }

    inline ~ShaderStage()
    {
        glDeleteProgram(m_id);
    }

    inline ShaderStage(ShaderStage&& other) noexcept
        : m_bit(other.m_bit)
        , m_id(other.m_id)
    {
        other.m_bit = 0;
        other.m_id = 0;
    }

    inline ShaderStage& operator=(ShaderStage&& other) noexcept
    {
        m_bit = other.m_bit;
        m_id = other.m_id;
        other.m_bit = 0;
        other.m_id = 0;
        return *this;
    }

    ShaderStage(const ShaderStage&) = delete;
    ShaderStage& operator=(const ShaderStage&) = delete;

    inline GLbitfield bit() const noexcept
    {
        return m_bit;
    }

    inline GLuint id() const noexcept
    {
        return m_id;
    }

private:

    GLbitfield m_bit;
    GLuint m_id;

};

// -- Shader Pipeline --
// This class contains the full opengl shader pipeline,
// it's a combination of different shader stages.

export class ShaderPipeline
{
public:

	inline ShaderPipeline()
		: m_id(0)
	{
	}

	template <typename... Stages>
	inline ShaderPipeline(ShaderStage* stage, Stages*... stages)
	{
		static_assert((std::is_same_v<Stages, ShaderStage> && ...), "All arguments are required to be pointers to ShaderStage instances.");
		glCreateProgramPipelines(1, &m_id);
		glUseProgramStages(m_id, stage->bit(), stage->id());
		(glUseProgramStages(m_id, stages->bit(), stages->id()), ...);
	}
	
    inline ~ShaderPipeline()
	{
		glDeleteProgramPipelines(1, &m_id);
	}

    inline ShaderPipeline(ShaderPipeline&& other) noexcept
		: m_id(other.m_id)
	{
		other.m_id = 0;
	}

    inline ShaderPipeline& operator=(ShaderPipeline&& other) noexcept
	{
		m_id = other.m_id;
		other.m_id = 0;
		return *this;
	}

	ShaderPipeline(const ShaderPipeline&) = delete;
	ShaderPipeline& operator=(const ShaderPipeline&) = delete;

	inline GLuint id() const noexcept
	{
		return m_id;
	}

private:

	GLuint m_id;

};

// -- Texture --
// Represents an image that can be used by opengl.
// It's loaded from a resource into gpu memory.

namespace
{

	constexpr GLenum ImgInternalFormats[4]
	{
		GL_R8,
		GL_RG8,
		GL_RGB8,
		GL_RGBA8
	};

	constexpr GLenum ImgFormats[4]
	{
		GL_RED,
		GL_RG,
		GL_RGB,
		GL_RGBA
	};

	inline void initTexture(GLuint id, GLuint channels, GLsizei width, GLsizei height, const void* data)
	{
		assert(channels > 0 && channels <= 4); // Invalid channel amount!
		GLenum format = ImgFormats[channels],
			iFormat = ImgInternalFormats[channels];

		// Check if the format is valid.
		if (format && iFormat)
		{

			// Setup texture parameters
			glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			// Upload texture to the gpu
			glTextureStorage2D(id, 1, iFormat, width, height);
			glTextureSubImage2D(id, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);

			// Generate mipmap for the texture
			glGenerateTextureMipmap(id);

		}
	}

}

export class Texture
{
public:

    inline Texture()
		: m_id(0)
	{
	}

	inline Texture(GLuint channels, GLsizei width, GLsizei height, const void* data)
	{
		
		// Create a texture handle.
		glCreateTextures(GL_TEXTURE_2D, 1, &m_id);

		// Initialize the texture
		initTexture(m_id, channels, width, height, data);

	}

	inline Texture(const Resource& res)
	{

		// Create a texture handle.
		glCreateTextures(GL_TEXTURE_2D, 1, &m_id);

		// Get image data.
		const stbi_uc* compressed_buf = static_cast<const stbi_uc*>(res.data());
		int length = static_cast<int>(res.size());

		// Uncompress image to a raw format.
		int width, height, channels;
		void* uncompressed_buf = stbi_load_from_memory(compressed_buf, length, &width, &height, &channels, 0);

		// Check for errors uncompressing the image.
		if (!uncompressed_buf)
		{
			std::cerr << "Error: Uncompressing the image failed!" << std::endl;
			return; // Abort
		}

		// Initialize the texture
		initTexture(m_id, channels, width, height, uncompressed_buf);

		// Remove the uncompressed texture from ram, because it's now in vram
		// and we don't want to process it any further.
		stbi_image_free(uncompressed_buf);

	}

	inline ~Texture()
	{
		glDeleteTextures(1, &m_id);
	}

	inline Texture(Texture&& other) noexcept
		: m_id(other.m_id)
	{
		other.m_id = 0;
	}

	inline Texture& operator=(Texture&& other) noexcept
	{
		m_id = other.m_id;
		other.m_id = 0;
		return *this;
	}

	Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    inline GLuint id() const noexcept
    {
        return m_id;
    }

private:

    GLuint m_id;

};

// -- Material --
// Used to tell on how to draw something.

export struct Material
{
	ShaderPipeline* pipeline { nullptr };
	Texture* texture { nullptr };
	glm::vec4 color_tilt { };
};

export inline bool operator==(const Material& lhs, const Material& rhs) {
	return lhs.pipeline == rhs.pipeline
		&& lhs.texture == rhs.texture
        && lhs.color_tilt == rhs.color_tilt;
}

export SHIV_FNV_HASH(Material)

// -- Model --
// This class is a reference to a model in the gpu.
// It loads a model into the gpu. It then can be used
// in the opengl shader pipeline to render a model.

namespace to = tinyobj;

namespace
{

    /**
     * @brief A material reader that only returns the previously registered materials.
     */
    class DummyMaterialReader final : public to::MaterialReader
    {
    public:

        // An usable instance of the class. (singleton)
        static DummyMaterialReader instance;

        // Override of the process function that only returns all registered materials.
        virtual bool operator()(const std::string& matId,
            std::vector<to::material_t>* materials,
            std::map<std::string, int>* matMap, std::string* warn,
            std::string* err) override
        {
            if (materials->empty())
            {
                for (auto& mappedMat : Resources::map<Material>())
                {
                    matMap->emplace(mappedMat.first, (int)materials->size());
                    materials->push_back({ mappedMat.first });
                }
            }
            return true;
        }

        // Default virtual destructor for the abstract class.
        virtual ~DummyMaterialReader() override = default;

    private:

        // Disables constructing the instance from outside.
        DummyMaterialReader() = default;

    };

    // Static instancing of the default instance
    DummyMaterialReader DummyMaterialReader::instance;

    /**
     * @brief A raw mesh read from the input stream.
     */
    struct RawMesh
    {

        to::attrib_t attrib;
        std::vector<to::shape_t> shapes;
        std::vector<to::material_t> materials;
        std::string warn, err;

        /**
         * @brief Parses an embedded resource.
         * @param rid The id of the embedded resource to parse.
         * @retval Whether the model was parsed successfully or not.
         */
        inline bool load(const Resource& res)
        {

            // Load resource as stream.
            auto stream = res.stream();

            // Parse the resource.
            return to::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                &stream, &DummyMaterialReader::instance);

        }

    };

	inline void initModelBuffers(GLuint vao, GLuint vbo, GLuint ebo,
		std::span<Vertex> vertices, std::span<uint32_t> indices)
	{

		// Upload the vertices and indices to the gpu buffers.
		glNamedBufferStorage(vbo, sizeof(Vertex) * vertices.size(), vertices.data(), 0);
		glNamedBufferStorage(ebo, sizeof(uint32_t) * indices.size(), indices.data(), 0);

		// Describe the vertex buffer attributes
		glVertexArrayAttribFormat(vao, POSITION_ATTRIBINDEX, sizeof(Vertex::position) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
		glVertexArrayAttribFormat(vao, NORMAL_ATTRIBINDEX, sizeof(Vertex::normal) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
		glVertexArrayAttribFormat(vao, TEXCOORDS_ATTRIBINDEX, sizeof(Vertex::texCoords) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoords));

		// Bind attributes to bindings
		glVertexArrayAttribBinding(vao, POSITION_ATTRIBINDEX, 0);
		glVertexArrayAttribBinding(vao, NORMAL_ATTRIBINDEX, 0);
		glVertexArrayAttribBinding(vao, TEXCOORDS_ATTRIBINDEX, 0);

		// Enable vertex attributes
		glEnableVertexArrayAttrib(vao, POSITION_ATTRIBINDEX);
		glEnableVertexArrayAttrib(vao, NORMAL_ATTRIBINDEX);
		glEnableVertexArrayAttrib(vao, TEXCOORDS_ATTRIBINDEX);

		// Bind the buffers to the vertex array object.
		glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(Vertex));
		glVertexArrayElementBuffer(vao, ebo);

		// Setup divisor per vertex
		glVertexArrayBindingDivisor(vao, 0, 0);

		// Model matrix constants
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

}

export class Model
{
public:

	/**
	 * @brief Contains information on how to access the ebo in the model to draw a specific part of the model.
	 */
	struct IndexInfo
	{
		uint32_t offset { };
		uint32_t size { };
	};

    /**
     * @brief Represents a part of the loaded model with multiple materials attached to it.
     */
    struct Part
    {
        
		std::string name { };
        std::unordered_map<Material, IndexInfo> meshes { };

		Part(std::string name = "")
			: name(name)
		{
		}

		Part(Part&& other) noexcept
			: name(std::move(other.name))
			, meshes(std::move(other.meshes))
		{
		}

		Part(const Part& other) noexcept
			: name(other.name)
			, meshes(other.meshes)
		{
		}

		Part& operator=(Part&& other) noexcept
		{
			name = std::move(other.name);
			meshes = std::move(other.meshes);
			return *this;
		}

		Part& operator=(const Part& other)
		{
			name = other.name;
			meshes = other.meshes;
			return *this;
		}

    };

    inline Model()
		: m_vao(0)
		, m_vbo(0)
		, m_ebo(0)
		, m_parts()
	{
	}

	inline Model(std::span<Vertex> vertices, std::span<uint32_t> indices, std::span<Part> parts)
		: m_parts()
	{

		// Create the buffers
		glCreateVertexArrays(1, &m_vao);
		glCreateBuffers(2, m_buffers);

		// Upload data and describe the buffers
		initModelBuffers(m_vao, m_vbo, m_ebo, vertices, indices);

		// Copy the parts from the span to the internal vector
		m_parts.reserve(parts.size());
		for (auto& part : parts)
			m_parts.emplace_back(std::move(part));

	}

    inline Model(const Resource& res)
		: m_parts()
	{

		// Create the buffers:
		// Even though, we won't upload data to them until later,
		// in case the model loading fails, we have allocated
		// valid buffers in opengl, so that we won't conflict
		// with any other opengl buffers, when using them.
		glCreateVertexArrays(1, &m_vao);
		glCreateBuffers(2, m_buffers);

		// Parse embedded resource mesh file with tinyobj_loader.
		RawMesh raw;
		if (!raw.load(res))
		{
			spdlog::warn("[ShipRenderer] Could not load model from resource: {}{}", raw.err, raw.warn);
			return; // Exit, we can't process any data.
		}




		// The data is currently split into multiple buffers, but to properly use the data,
		// we need to serialize it into two huge buffers - vertices and indices.
		// The sorting of the vertices is irrelevant. The indices, on the other hand,
		// should be sorted by material to minimize throughput at draw calls to the gpu.
		// So first we collect the indices by material and the vertices we already
		// throw into one buffer that we'll send to the gpu later.

		// The vertex buffer that will be uploaded to the gpu.
		std::vector<Vertex> vertices{ };

		// Used to identify and avoid duplicate vertices.
		std::unordered_map<Vertex, uint32_t> uniqueVertices{ };

		// Used to sort out the indices by material.
		std::unordered_map<Material, std::vector<uint32_t>> stagedIndices{ };
		size_t total_indices = 0; // This variable is used later to allocate
								  // a buffer once which can hold all indices.
								  // This variable will increment on each index
								  // processing.

		Part all("$ALL"); // A model part that describes the whole model.

		for (const auto& shape : raw.shapes)
		{

			// Get total model-part indices
			const size_t shape_faces = shape.mesh.material_ids.size();

			// Loop through each face in the shape
			for (size_t fi = 0; fi < shape_faces; fi++, total_indices += 3)
			{

				// Used to find material from a raw shape by face index
				static const auto getMaterialBy = [](RawMesh& raw, const to::shape_t& shape, size_t face_index) -> Material
				{

					// Get material id
					auto material_id = shape.mesh.material_ids[face_index];

					// Ensure the material is valid
					if (material_id < 0)
						return { };

					// Get material name
					auto material_name = raw.materials[material_id].name;

					// Find material by name
					auto& matMap = Resources::map<Material>();
					auto iter = matMap.find(material_name);
					return iter != matMap.end() ? *iter->second : Material();

				};

				// Get material
				auto material = getMaterialBy(raw, shape, fi);
				auto& mIndices = stagedIndices[material];

				for (size_t vi = 0; vi < 3; vi++)
				{

					// Get index data from mesh at vertex index.
					auto index_data = shape.mesh.indices[(3 * fi) + vi];

					// Create an empty vertex.
					Vertex vertex;

					// Apply the position coordinates to the vertex.
					vertex.position = {
						raw.attrib.vertices[3 * index_data.vertex_index + 0],
						raw.attrib.vertices[3 * index_data.vertex_index + 1],
						raw.attrib.vertices[3 * index_data.vertex_index + 2]
					};

					// Apply the normal vector to the vertex.
					vertex.normal = {
						raw.attrib.normals[3 * index_data.normal_index + 0],
						raw.attrib.normals[3 * index_data.normal_index + 1],
						raw.attrib.normals[3 * index_data.normal_index + 2]
					};

					// Apply the uv coordinates to the vertex.
					vertex.texCoords = {
						raw.attrib.texcoords[2 * index_data.texcoord_index + 0],
						1.0F - raw.attrib.texcoords[2 * index_data.texcoord_index + 1]
					};

					// Add the vertex to the buffer and get its index or of the existing vertex.
					auto [iter, result] = uniqueVertices.try_emplace(vertex, (uint32_t)vertices.size());
					if (result)
						vertices.emplace_back(vertex);

					// Add index to the materials index buffer
					mIndices.emplace_back(iter->second);

				}

			}

			// Initialize a new part
			auto& part = m_parts.emplace_back(shape.name);

			// Set material info (stage 1)
			for (auto& [mat, curMatIndices] : stagedIndices)
			{

				// Get material indices.
				auto& allMatIndices = all.meshes[mat];
				auto& partMatIndices = part.meshes[mat];

				// Apply current part indices info for that material.
				partMatIndices.offset = allMatIndices.size;
				partMatIndices.size = static_cast<uint32_t>(curMatIndices.size()) - allMatIndices.size;

				// Update all part indices info for that material.
				allMatIndices.size = partMatIndices.offset + partMatIndices.size;

			}

		}




		// The indices buffers by material are filled, the data can now be easily
		// serialized into one huge indices buffer.

		// Create a indices buffer with a suitable size.
		uint32_t* indices = new uint32_t[total_indices];
		uint32_t indices_offset = 0;

		for (auto& iIter : stagedIndices)
		{

			// Update offsets for the parts (stage 2)
			all.meshes[iIter.first].offset = indices_offset;
			for (auto& part : m_parts)
			{
				auto mIter = part.meshes.find(iIter.first);
				if (mIter != part.meshes.end())
					mIter->second.offset += indices_offset;
			}

			// Copy material indices to the new single buffer.
			uint32_t* dstData = indices + indices_offset;
			uint32_t* srcData = iIter.second.data();
			size_t srcSize = iIter.second.size();
			std::memcpy(dstData, srcData, sizeof(uint32_t) * srcSize);

			// Update offset of the main indices buffer.
			indices_offset += static_cast<uint32_t>(srcSize);

		}

		m_parts.emplace_back(std::move(all));




		// The data is now ready to be uploaded to the gpu.
		// This means, we have to upload the data to the gpu and
		// describe the content in the buffer for opengl.

		initModelBuffers(m_vao, m_vbo, m_ebo,
			std::span(vertices), std::span(indices, total_indices));
		

		// Cleanup
		delete[] indices;

	}

    inline ~Model()
	{
		glDeleteVertexArrays(1, &m_vao);
		glDeleteBuffers(2, m_buffers);
	}

    // Assignments
    //-------------

    inline Model(Model&& other) noexcept
		: m_vao(other.m_vao)
		, m_vbo(other.m_vbo)
		, m_ebo(other.m_ebo)
		, m_parts(std::move(other.m_parts))
	{
		other.m_vao = 0;
		other.m_vbo = 0;
		other.m_ebo = 0;
	}

    Model& operator=(Model&& other) noexcept
	{
		m_vao = other.m_vao;
		m_vbo = other.m_vbo;
		m_ebo = other.m_ebo;
		m_parts = std::move(other.m_parts);
		other.m_vao = 0;
		other.m_vbo = 0;
		other.m_ebo = 0;
		return *this;
	}

	Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // Getters
    //---------

    inline GLuint vao() const noexcept
    {
        return m_vao;
    }

    inline GLuint vbo() const noexcept
    {
        return m_vbo;
    }

    inline GLuint ebo() const noexcept
    {
        return m_ebo;
    }

    inline const std::vector<Part>& parts() const noexcept
    {
        return m_parts;
    }

    inline const Part* find(const std::string& name) const noexcept
    {

        // Return first part with the name
        for (auto& part : m_parts)
            if (part.name == name)
                return &part;

        return nullptr; // or return an null model part,
                        // it won't draw any triangles.

    }

private:

    GLuint m_vao;
    union {
        struct {
            GLuint m_vbo;
            GLuint m_ebo;
        };
        GLuint m_buffers[2];
    };

    std::vector<Part> m_parts;

};

export inline bool operator==(const Model::IndexInfo& lhs, const Model::IndexInfo& rhs) {
    return lhs.offset == rhs.offset
        && lhs.size == rhs.size;
}

export SHIV_FNV_HASH(Model::IndexInfo)

// -- Resource Store: Lifecycle --
// This will initialize all resources or clean up them
// depending on which methods are called.

export namespace Resources
{

	void Init()
	{

		// Shader Stages
		auto* cel_vert = Resources::emplace<ShaderStage>("VERT:Cel", GL_VERTEX_SHADER, Resource(IDR_VSHA_CEL));
		auto* cel_frag = Resources::emplace<ShaderStage>("FRAG:Cel", GL_FRAGMENT_SHADER, Resource(IDR_FSHA_CEL));

		// Shader Pipelines
		auto* cel_prog = Resources::emplace<ShaderPipeline>("Cel", cel_vert, cel_frag);

		// Texture
		constexpr char dummy_texdata[3]{ 0, 0, 0 };
		auto* dummy_tex = Resources::emplace<Texture>("Dummy", 3, 1, 1, &dummy_texdata);

		// Materials
		Resources::emplace<Material>("Border", cel_prog, dummy_tex, glm::vec4(1.0F, 1.0F, 1.0F, 1.0F));
		Resources::emplace<Material>("Water", cel_prog, dummy_tex, glm::vec4(0.18F, 0.33F, 1.0F, 1.0F));
		Resources::emplace<Material>("Iron", cel_prog, dummy_tex, glm::vec4(0.8F, 0.8F, 0.8F, 1.0F));

		// Models
		Resources::emplace<Model>("Teapot", Resource(IDR_MESH_TEAPOT));
		for (size_t i = 0; i < ShipCount; i++)
			Resources::emplace<Model>(ShipInfos[i].resName, Resource(ShipInfos[i].resId));

	}

	void CleanUp()
	{
		Resources::clear<Model>();
		Resources::clear<Material>();
		Resources::clear<ShaderPipeline>();
		Resources::clear<ShaderStage>();
		Resources::clear<Texture>();
	}

}