#ifndef SHIFVRSKY_MODELS_HPP
#define SHIFVRSKY_MODELS_HPP

#include <glad/glad.h>

#include "../../draw/util/Vertex.hpp"
#include "Materials.hpp"
#include "../../util/fnv_hash.hpp"
#include "../Game.hpp"

/**
 * @brief Represents a model with multiple shapes, just like a scene.
 */
class Model
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
     *        Can be drawn either alone or with the whole overarching model.
     */
    struct Part
    {
        std::string name { };
        std::unordered_map<Material*, IndexInfo> meshes { };
    };

    // Constants
    //-----------

    static constexpr size_t BUFFERING_AMOUNT = Game::BUFFERING_AMOUNT;

    // Constructors
    //--------------

    Model();
    Model(int rid);

    // Destructors
    //-------------

    ~Model();

    // Assignments
    //-------------

    Model(Model&&) noexcept;
    Model(const Model&) = delete;

    Model &operator=(Model&&) noexcept;
    Model &operator=(const Model&) = delete;

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

    inline const std::vector<Part> &parts() const noexcept
    {
        return m_parts;
    }

    inline const Part &all() const noexcept
    {
        return m_all;
    }

    inline Part find(const std::string& name) const noexcept
    {

        // Return first part with the name
        for (auto& part : m_parts)
            if (part.name == name)
                return part;

        return { }; // or return an null model part,
                    // it won't draw any trianagles.

    }

private:

    // Properties
    //------------

    // OpenGL Handles

    GLuint m_vao;
    union {
        struct {
            GLuint m_vbo;
            GLuint m_ebo;
        };
        GLuint m_buffers[2];
    };

    // Info

    Part m_all;
    std::vector<Part> m_parts;

};

inline bool operator==(const Model::IndexInfo& lhs, const Model::IndexInfo& rhs) {
    return lhs.offset == rhs.offset
        && lhs.size == rhs.size;
}

SHIV_FNV_HASH(Model::IndexInfo)

namespace Models
{

    extern Model water;
    extern Model teapot;

}

#endif // SHIFVRSKY_MODELS_HPP