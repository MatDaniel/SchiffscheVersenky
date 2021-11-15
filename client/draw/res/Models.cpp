#include "Models.hpp"

#include <tiny_obj_loader.h>

#include <vector>
#include <unordered_map>
#include <iostream>

#include "Resource.hpp"
#include "util/imemstream.hpp"

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
        virtual bool operator()(const std::string &matId,
            std::vector<to::material_t> *materials,
            std::map<std::string, int> *matMap, std::string *warn,
            std::string *err) override
        {
            if (materials->empty())
            {
                for (auto& mappedMat : Materials::map)
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

        // Disables construting the instance from outside.
        DummyMaterialReader() = default;

    };

    // Static instancing of the default instace
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
         * @retval Whether the material was parsed successfully or not.
         */
        inline bool load(int rid)
        {

            // Load resource as stream.
            auto stream = Resource(rid).stream();

            // Parse the resource.
            return to::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                               &stream, &DummyMaterialReader::instance);

        }

    };

}

Model::Model()
    : m_vao(0)
    , m_vbo(0)
    , m_ebo(0)
    , m_all("ALL")
    , m_parts()
{
}

Model::Model(int rid)
    : m_all("ALL")
    , m_parts()
{

    // -- Create the buffers --
    // Even though, we won't upload data to them until later,
    // in case the model loading fails, we have allocated
    // valid buffers in opengl, so that we won't conflict
    // with any other opengl buffers, when using them.
    glCreateVertexArrays(1, &m_vao);
    glCreateBuffers(2, m_buffers);

    // Parse embedded resource mesh file with tinyobj_loader.
    RawMesh raw;
    if (!raw.load(rid))
    {
        std::cerr << "[ShipRenderer] Could not load model from resource " << rid << ": " << std::endl << raw.err << raw.warn;
        return; // Exit, we can't process any data.
    }




    //
    // The data is currently split into multiple buffers, but to properly use the data,
    // we need to serialize it into two huge buffers - vertices and indices.
    // The sorting of the vertices is irrelevant. The indices, on the other hand,
    // should be sorted by material to minimize throughput at draw calls to the gpu.
    // So first we collect the indices by material and the vertices we already
    // throw into one buffer that we'll send to the gpu later.
    //

    // The vertex buffer that will be uploaded to the gpu.
    std::vector<Vertex> vertices { };

    // Used to identify and avoid duplicate vertices.
    std::unordered_map<Vertex, uint32_t> uniqueVertices { };

    // Used to sort out the indices by material.
    std::unordered_map<Material*, std::vector<uint32_t>> stagedIndices { };
    size_t total_indices = 0; // This variable is used later to allocate
                              // a buffer once which can hold all indices.
                              // This variable will increment on each index
                              // processing.

    for (const auto &shape : raw.shapes)
    {

        // Get total model-part indices
        const size_t shape_faces = shape.mesh.material_ids.size();

        // Loop through each face in the shape
        for (size_t fi = 0; fi < shape_faces; fi++, total_indices += 3)
        {

            // Used to find material from a raw shape by face index
            static const auto getMaterialBy = [](RawMesh &raw, const to::shape_t &shape, size_t face_index) -> Material*
            {

                // Get material id
                auto material_id = shape.mesh.material_ids[face_index];

                // Ensure the material is valid
                if (material_id < 0)
                    return nullptr;

                // Get material name
                auto material_name = raw.materials[material_id].name;

                // Find material by name
                auto iter = Materials::map.find(material_name);
                return iter != Materials::map.end() ? iter->second : nullptr;

            };

            // Get material
            auto material = getMaterialBy(raw, shape, fi);
            auto &mIndices = stagedIndices[material];

            for (size_t vi = 0; vi < 3; vi++)
            {

                // Get index data from mesh at vertex index.
                auto index_data = shape.mesh.indices[(3 * fi) + vi];

                // Create an empty vertex.
                Vertex vertex;

                // Apply the positon coordinates to the vertex.
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
                auto [iter,result] = uniqueVertices.try_emplace(vertex, static_cast<uint32_t>(vertices.size()));
                if (result)
                    vertices.emplace_back(vertex);

                // Add index to the materials index buffer
                mIndices.emplace_back(iter->second);

            }

        }

        // Initialize a new part
        auto &part = m_parts.emplace_back(shape.name);

        // Set material info (stage 1)
        for (auto &[mat, curMatIndices] : stagedIndices)
        {

            // Get material indices.
            auto &allMatIndices = m_all.meshes[mat];
            auto &partMatIndices = part.meshes[mat];
            
            // Apply current part indices info for that material.
            partMatIndices.offset = allMatIndices.size;
            partMatIndices.size = static_cast<uint32_t>(curMatIndices.size()) - allMatIndices.size;

            // Update all part indices info for that material.
            allMatIndices.size = partMatIndices.offset + partMatIndices.size;
            
        }

    }




    //
    // The indices buffers by material are filled, the data can now be easily
    // serialized into one huge indices buffer.
    //

    // Create a indices buffer with a suitable size.
    uint32_t *indices = new uint32_t[total_indices];
    uint32_t indices_offset = 0;

    for (auto &iIter : stagedIndices)
    {

        // Update offsets for the parts (stage 2)
        m_all.meshes[iIter.first].offset = indices_offset;
        for (auto &part : m_parts)
        {
            auto mIter = part.meshes.find(iIter.first);
            if (mIter != part.meshes.end())
                mIter->second.offset += indices_offset;
        }

        // Copy material indices to the new single buffer.
        uint32_t *dstData = indices + indices_offset;
        uint32_t *srcData = iIter.second.data();
        size_t srcSize = iIter.second.size();
        std::memcpy(dstData, srcData, sizeof(uint32_t) * srcSize);

        // Update offset of the main indices buffer.
        indices_offset += static_cast<uint32_t>(srcSize);

    }




    //
    // The data is now ready to be uploaded to the gpu.
    // This means, we have to upload the data to the gpu and
    // describe the content in the buffer for opengl.
    //

    // Constants
    constexpr GLuint POSITION_ATTRIBINDEX = 0;
    constexpr GLuint NORMAL_ATTRIBINDEX = 1;
    constexpr GLuint TEXCOORDS_ATTRIBINDEX = 2;

    // Bind the buffers to the vertex array object.
    glVertexArrayVertexBuffer(m_vao, POSITION_ATTRIBINDEX, m_vbo, 0, sizeof(Vertex));
    glVertexArrayVertexBuffer(m_vao, NORMAL_ATTRIBINDEX, m_vbo, 0, sizeof(Vertex));
    glVertexArrayVertexBuffer(m_vao, TEXCOORDS_ATTRIBINDEX, m_vbo, 0, sizeof(Vertex));
    glVertexArrayElementBuffer(m_vao, m_ebo);

    // Upload the vertices and indices to the gpu buffers.
    glNamedBufferData(m_vbo, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
    glNamedBufferData(m_ebo, sizeof(uint32_t) * total_indices, indices, GL_STATIC_DRAW);

    // Enable vertex attributes
    glEnableVertexArrayAttrib(m_vao, POSITION_ATTRIBINDEX);
    glEnableVertexArrayAttrib(m_vao, NORMAL_ATTRIBINDEX);
    glEnableVertexArrayAttrib(m_vao, TEXCOORDS_ATTRIBINDEX);

    // Describe the vertex buffer attributes
    glVertexArrayAttribFormat(m_vao, POSITION_ATTRIBINDEX, sizeof(Vertex::position) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    glVertexArrayAttribFormat(m_vao, NORMAL_ATTRIBINDEX, sizeof(Vertex::normal) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
    glVertexArrayAttribFormat(m_vao, TEXCOORDS_ATTRIBINDEX, sizeof(Vertex::texCoords) / sizeof(float), GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoords));

    // Bind attributes to bindings
    glVertexArrayAttribBinding(m_vao, POSITION_ATTRIBINDEX, 0);
    glVertexArrayAttribBinding(m_vao, NORMAL_ATTRIBINDEX, 0);
    glVertexArrayAttribBinding(m_vao, TEXCOORDS_ATTRIBINDEX, 0);

    // Cleanup
    delete[] indices;

}

Model::Model(Model &&other) noexcept
    : m_vao(other.m_vao)
    , m_vbo(other.m_vbo)
    , m_ebo(other.m_ebo)
    , m_all(std::move(other.m_all))
    , m_parts(std::move(other.m_parts))
{
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
}

Model& Model::operator=(Model &&other) noexcept
{

    // Copy values
    m_vao = other.m_vao;
    m_vbo = other.m_vbo;
    m_ebo = other.m_ebo;
    m_all = std::move(other.m_all);
    m_parts = std::move(other.m_parts);

    // Invalidate other mesh
    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;

    // Return this instance
    return *this;

}

Model::~Model()
{
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(2, m_buffers);
}


//----------------//
// INSTANTIATIONS //
//----------------//

Model Models::teapot;