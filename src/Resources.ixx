module;

#include "BattleShip.h"

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

#include "util/FNVHash.hpp"
#include "Vertex.hpp"

#include "resource.h"

export module Draw.Resources;
import Draw.Render;
import Game.ShipInfo;
using namespace Draw;

export SpdLogger ResourceLog;

// -- Resource Store --
// A unified place for any Resource.

template <class T>
struct ResourceStore
{
	inline static std::unordered_map<std::string, std::unique_ptr<T>> value { };
};

export namespace Draw::Resources
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

namespace Draw::Resources::Loaders
{

	// Shortcuts
	using namespace Draw;

	// -- Textures --

	Render::Texture LoadTexture(const Resource& ImageResource, const Render::TextureLayout& InitialLayout)
	{

		// Get image data.
		const stbi_uc* CompressedImageBuffer = static_cast<const stbi_uc*>(ImageResource.data());
		int ResourceLength = static_cast<int>(ImageResource.size());

		// Uncompress image to a raw format.
		int OutWidth, OutHeight, OutChannels;
		void* UncompressedImageBuffer = stbi_load_from_memory(CompressedImageBuffer, ResourceLength,
			&OutWidth, &OutHeight, &OutChannels, InitialLayout.Channels);

		// Check for errors uncompressing the image.
		if (!UncompressedImageBuffer)
		{
			SPDLOG_LOGGER_CRITICAL(ResourceLog, "Failed to uncompress image!");
			return { }; // Return uninitialized texture
		}

		// Create the texture resource
		auto InputLayout = InitialLayout;
		InputLayout.Channels = OutChannels;
		InputLayout.Width = OutWidth;
		InputLayout.Height = OutHeight;
		InputLayout.Pixels = UncompressedImageBuffer;
		Render::Texture Result{ InputLayout };

		// Remove the uncompressed texture from ram, because it's now in vram
		// and we don't want to process it any further.
		stbi_image_free(UncompressedImageBuffer);

		// Return the created texture
		return Result;

	}

	// -- Shader Stages --

	Render::ShaderStage LoadStage(GLenum ShaderType, const Resource& StageResource)
	{
		auto Code = StageResource.str();
		return { ShaderType, Code.c_str() };
	}

	// -- Models --

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
					for (auto& mappedMat : Resources::map<Render::Material>())
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

	}

	Render::ConstModel LoadModel(const Resource& ModelResource)
	{

		// Parse embedded resource mesh file with tinyobj_loader.
		RawMesh raw;
		if (!raw.load(ModelResource))
			SPDLOG_LOGGER_CRITICAL(ResourceLog, "Failed to load model from resource: {}{}", raw.err, raw.warn);




		// The data is currently split into multiple buffers, but to properly use the data,
		// we need to serialize it into two huge buffers - vertices and indices.
		// The sorting of the vertices is irrelevant. The indices, on the other hand,
		// should be sorted by material to minimize throughput at draw calls to the gpu.
		// So first we collect the indices by material and the vertices we already
		// throw into one buffer that we'll send to the gpu later.

		// The vertex buffer that will be uploaded to the gpu.
		std::vector<Vertex> vertices { };

		// Used to identify and avoid duplicate vertices.
		std::unordered_map<Vertex, uint32_t> uniqueVertices { };

		// Used to sort out the indices by material.
		std::unordered_map<Render::Material, std::vector<uint32_t>> stagedIndices{ };
		size_t total_indices = 0; // This variable is used later to allocate
								  // a buffer once which can hold all indices.
								  // This variable will increment on each index
								  // processing.

		Render::ModelInfo info; // A model part that describes the model.

		for (const auto& shape : raw.shapes)
		{

			// Get total model-part indices
			const size_t shape_faces = shape.mesh.material_ids.size();

			// Loop through each face in the shape
			for (size_t fi = 0; fi < shape_faces; fi++, total_indices += 3)
			{

				// Used to find material from a raw shape by face index
				static const auto getMaterialBy = [](RawMesh& raw, const to::shape_t& shape, size_t face_index) -> Render::Material
				{

					// Get material id
					auto material_id = shape.mesh.material_ids[face_index];

					// Ensure the material is valid
					if (material_id < 0)
						return { };

					// Get material name
					auto material_name = raw.materials[material_id].name;

					// Find material by name
					auto& matMap = Resources::map<Render::Material>();
					auto iter = matMap.find(material_name);
					return iter != matMap.end() ? *iter->second : Render::Material();

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

			// Set material info (stage 1)
			for (auto& [mat, curMatIndices] : stagedIndices)
			{

				// Get material indices.
				auto& matIndices = info.Meshes[mat];

				// Update part indices info for that material.
				matIndices.size += static_cast<uint32_t>(curMatIndices.size()) - matIndices.size;

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
			info.Meshes[iIter.first].offset = indices_offset;

			// Copy material indices to the new single buffer.
			uint32_t* dstData = indices + indices_offset;
			uint32_t* srcData = iIter.second.data();
			size_t srcSize = iIter.second.size();
			std::memcpy(dstData, srcData, sizeof(uint32_t) * srcSize);

			// Update offset of the main indices buffer.
			indices_offset += static_cast<uint32_t>(srcSize);

		}



		// The data is now ready to be uploaded to the gpu.
		// This means, we have to upload the data to the gpu and
		// describe the content in the buffer for opengl.

		Render::ConstModel Result {
			Render::ConstMesh {
				Render::ConstMappedBuffer<Vertex>(std::span(vertices)),
				Render::ConstMappedBuffer<uint32_t>(std::span(indices, total_indices))
			},
			std::move(info)
		};

		// Cleanup
		delete[] indices;

		// Return model
		return Result;

	}

}

// -- Resource Store: Lifecycle --
// This will initialize all resources or clean up them
// depending on which methods are called.

export namespace Draw::Resources
{

	using namespace Draw;

	void Init()
	{

		// Shader Stages
		auto* DefaultVertShader = Resources::emplace<Render::ShaderStage>("Vert/Default", Loaders::LoadStage(GL_VERTEX_SHADER, Resource(IDR_VSHA_DEFAULT)));
		auto* CelFragShader = Resources::emplace<Render::ShaderStage>("Frag/Cel", Loaders::LoadStage(GL_FRAGMENT_SHADER, Resource(IDR_FSHA_CEL)));
		auto* CrtFragShader = Resources::emplace<Render::ShaderStage>("Frag/Crt", Loaders::LoadStage(GL_FRAGMENT_SHADER, Resource(IDR_FSHA_CRT)));
		auto* SolidFragShader = Resources::emplace<Render::ShaderStage>("Frag/Solid", Loaders::LoadStage(GL_FRAGMENT_SHADER, Resource(IDR_FSHA_SOLID)));
		auto* WaterFragShader = Resources::emplace<Render::ShaderStage>("Frag/Water", Loaders::LoadStage(GL_FRAGMENT_SHADER, Resource(IDR_FSHA_WATER)));

		// Shader Pipelines
		auto* CelProgram = Resources::emplace<Render::ShaderPipeline>("Cel", DefaultVertShader, CelFragShader);
		auto* CrtProgram = Resources::emplace<Render::ShaderPipeline>("Crt", DefaultVertShader, CrtFragShader);
		auto* SolidProgram = Resources::emplace<Render::ShaderPipeline>("Solid", DefaultVertShader, SolidFragShader);
		auto* WaterProgram = Resources::emplace<Render::ShaderPipeline>("Water", DefaultVertShader, WaterFragShader);

		// Texture
		constexpr char DummyPixels[3] { 0, 0, 0 };
		auto DummyLayout = Render::TextureLayout(1, 1, 3, &DummyPixels).WrapRepeat().FilterNearest();
		auto* DummyTexture = Resources::emplace<Render::Texture>("Dummy", DummyLayout);

		// Materials
		Resources::emplace<Render::Material>("Iron", CelProgram, DummyTexture, glm::vec4(0.8F, 0.8F, 0.8F, 1.0F));
		Resources::emplace<Render::Material>("GameField_Water", WaterProgram, DummyTexture, glm::vec4(0.0F, 0.0F, 1.0F, 0.5F));
		Resources::emplace<Render::Material>("GameField_Border", WaterProgram, DummyTexture, glm::vec4(1.0F, 1.0F, 1.0F, 0.85F));
		Resources::emplace<Render::Material>("GameField_Collision", WaterProgram, DummyTexture, glm::vec4(1.0F, 0.0F, 0.0F, 1.0F));
		Resources::emplace<Render::Material>("GameField_CollisionIndirect", WaterProgram, DummyTexture, glm::vec4(1.0F, 1.0F, 0.0F, 1.0F));
		Resources::emplace<Render::Material>("Screen_PostProcessing", CrtProgram, nullptr, glm::vec4());
		Resources::emplace<Render::Material>("Screen_GameField", SolidProgram, DummyTexture, glm::vec4(0.19F, 0.75F, 0.25F, 1.0F));
		Resources::emplace<Render::Material>("Screen_MarkerSelect", SolidProgram, DummyTexture, glm::vec4(0.725F, 0.0F, 0.0F, 1.0F));
		Resources::emplace<Render::Material>("Screen_MarkerShip", SolidProgram, DummyTexture, glm::vec4(0.725F, 0.0F, 0.0F, 1.0F));
		Resources::emplace<Render::Material>("Screen_MarkerFail", SolidProgram, DummyTexture, glm::vec4(1.0F, 1.0F, 1.0F, 1.0F));
		Resources::emplace<Render::Material>("Screen_MarkerHit", SolidProgram, DummyTexture, glm::vec4(0.18125F, 0.0F, 0.0F, 1.0F));

		// Models
		for (size_t i = 0; i < ShipTypeCount; i++)
			Resources::emplace<Render::ConstModel>(ShipInfos[i].resName, Loaders::LoadModel(Resource(ShipInfos[i].resId)));
		Resources::emplace<Render::ConstModel>("Markers/Select", Loaders::LoadModel(Resource(IDR_MESH_MARK_SLCT)));
		Resources::emplace<Render::ConstModel>("Markers/Ship", Loaders::LoadModel(Resource(IDR_MESH_MARK_SHIP)));
		Resources::emplace<Render::ConstModel>("Markers/Fail", Loaders::LoadModel(Resource(IDR_MESH_MARK_FAIL)));
		Resources::emplace<Render::ConstModel>("ScreenFrame", Loaders::LoadModel(Resource(IDR_MESH_SCRN_FRAM)));

	}

	void CleanUp()
	{
		Resources::clear<Render::ConstModel>();
		Resources::clear<Render::Material>();
		Resources::clear<Render::ShaderStage>();
		Resources::clear<Render::ShaderPipeline>();
		Resources::clear<Render::Texture>();
	}

}