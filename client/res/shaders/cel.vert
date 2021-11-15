#version 450 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
// layout (binding = 1, location = 0) in mat4 aModlMtx;

layout (binding = 0, std140) uniform CameraUBO
{
    mat4 projMtx;
    mat4 viewMtx;
    vec4 camPos;
};
uniform mat4 model;

out vec2 TexCoords;
out vec3 WorldPos;
out vec3 Normal;
out vec3 CamPos;

void main()
{
    TexCoords = aTexCoord;
    WorldPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(model) * aNormal;
    CamPos = vec3(camPos);

    gl_Position = projMtx * viewMtx * vec4(WorldPos, 1.0);   
}