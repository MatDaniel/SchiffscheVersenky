#include "Textures.hpp"

#include <glad/glad.h>
#include <stb_image.h>

#include <iostream>

#include "Resource.hpp"

//---------//
// TEXTURE //
//---------//

// Constructors
//--------------

Texture::Texture()
    : m_id(0)
{
}

Texture::Texture(int rid)
{

    // Create a texture handle.
    glCreateTextures(GL_TEXTURE_2D, 1, &m_id);

    // Get the image resource.
    Resource resource(rid);
        
    // Get image data.
    const stbi_uc *compressed_buf = static_cast<const stbi_uc*>(resource.data());
    int length = static_cast<int>(resource.size());

    // Uncompress image to a raw format.
    int width, height, channels;
    void* uncompressed_buf = stbi_load_from_memory(compressed_buf, length, &width, &height, &channels, 0);

    // Check for errors uncompressing the image.
    if (!uncompressed_buf)
    {
        std::cerr << "Error: Uncompressing the image with the resource id " << rid << " failed!" << std::endl;
        return; // Abort
    }

    // Select the correct opengl format from the enum.
    GLenum format, iFormat;
    switch (channels)
    {
        case 1:
            format = GL_RED;
            iFormat = GL_R8;
            break;
        case 2:
            format = GL_RG;
            iFormat = GL_RG8;
            break;
        case 3:
            format = GL_RGB;
            iFormat = GL_RGB8;
            break;
        case 4:
            format = GL_RGBA;
            iFormat = GL_RGBA8;
            break;
        default:
            format = 0;
            iFormat = 0;
            std::cerr << "Error: Could not load any texture. Invalid channel amount " << channels << '!' << std::endl;
            break;
    }

    // Check if the format is valid.
    if (format && iFormat)
    {

        // Setup texture parameters
        glTextureParameteri(m_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(m_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(m_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Upload texture to the gpu
        glTextureStorage2D(m_id, 1, iFormat, width, height);
        glTextureSubImage2D(m_id, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, uncompressed_buf);

        // Generate mipmap for the texture
        glGenerateTextureMipmap(m_id);

    }

    // Remove the uncompressed texture from ram, because it's in vram
    // and we don't want to process it any further.
    stbi_image_free(uncompressed_buf);

}

// Destructors
//-------------

Texture::~Texture()
{
    glDeleteTextures(1, &m_id);
}

// Assignments
//-------------

Texture::Texture(Texture &&other)
    : m_id(other.m_id)
{
    other.m_id = 0;
}

Texture &Texture::operator=(Texture &&other)
{

    // Copy values
    m_id = other.m_id;

    // Invalidate other instance
    other.m_id = 0;

    // Return this instance
    return *this;

}

//----------------//
// INSTANTIATIONS //
//----------------//
