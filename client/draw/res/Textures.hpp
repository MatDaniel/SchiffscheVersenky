#ifndef SHIFVRSKY_TEXTURES_HPP
#define SHIFVRSKY_TEXTURES_HPP

#include <glad/glad.h>

/**
 * @brief This class represents a texture.
 */
class Texture
{
public:

    // Constructors
    //--------------

    Texture();
    Texture(int rid);

    // Destructors
    //-------------

    ~Texture();

    // Assignments
    //-------------

    Texture(Texture&&);
    Texture(const Texture&) = delete;

    Texture &operator=(Texture&&);
    Texture &operator=(const Texture&) = delete;

    // Getters
    //---------

    inline GLuint id() const noexcept
    {
        return m_id;
    }

private:

    // Properties
    //------------

    GLuint m_id;

};

/**
 * @brief Contains instances of textures.
 */
namespace Textures
{
}

#endif // SHIFVRSKY_TEXTURES_HPP