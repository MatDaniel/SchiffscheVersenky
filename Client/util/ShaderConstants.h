#ifndef SHIFVRSKY_CONSTANTS_HPP
#define SHIFVRSKY_CONSTANTS_HPP

#include <glad/glad.h>

// Uniforms
constexpr GLuint TEXTURE_LOCATION = 0;

// Input buffers
constexpr GLuint POSITION_ATTRIBINDEX = 0;
constexpr GLuint NORMAL_ATTRIBINDEX = 1;
constexpr GLuint TEXCOORDS_ATTRIBINDEX = 2;
constexpr GLuint MODELMTX_ATTRIBINDEX = 3; // Takes up 3,4,5,6
constexpr GLuint COLTILT_ATTRIBINDEX = 7;

// Misc
constexpr size_t BUFFERING_AMOUNT = 2;

#endif // SHIFVRSKY_CONSTANTS_HPP