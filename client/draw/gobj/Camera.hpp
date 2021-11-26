#ifndef SHIFVRSKY_CAMERA_HPP
#define SHIFVRSKY_CAMERA_HPP

#include <glm/vec3.hpp>

struct Camera final
{

    glm::vec3 position { };
    glm::vec3 direction { 0.0F, 0.0F, -1.0F };
    glm::vec3 up { 0.0F, 1.0F, 0.0F };

};

#endif // SHIFVRSKY_CAMERA_HPP