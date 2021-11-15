#ifndef SHIFVRSKY_MATERIALS_HPP
#define SHIFVRSKY_MATERIALS_HPP

#include <glad/glad.h>
#include <unordered_map>

#include "Textures.hpp"
#include "util/fnv_hash.hpp"

/**
 * @brief This is a view of an embedded resource.
 */
class Material
{
public:

    // Constructors
    //--------------

    Material();
    Material(Texture *albedo, Texture *normal, Texture *mrao);

    // Utility
    //---------

    inline void use()
    {

        // Constant locations in the shader program
        constexpr GLuint ALBEDO_LOCATION = 0;
        constexpr GLuint NORMAL_LOCATION = 1;
        constexpr GLuint MRAO_LOCATION = 2;

        // Set the texture locations in the shader 
        glUniform1i(ALBEDO_LOCATION, 0);
        glUniform1i(NORMAL_LOCATION, 1);
        glUniform1i(MRAO_LOCATION, 2);

        // Bind the textures
        glBindTextureUnit(0, m_albedo->id());
        glBindTextureUnit(1, m_normal->id());
        glBindTextureUnit(2, m_mrao->id());

    }

private:

    // Properties
    //------------

    Texture *m_albedo;
    Texture *m_normal;
    Texture *m_mrao;

};

namespace Materials
{

    typedef std::unordered_map<std::string, Material*> InstanceMap;
    extern const InstanceMap map;

}

#endif // SHIFVRSKY_MATERIALS_HPP