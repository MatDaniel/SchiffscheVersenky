#version 450 core

out vec4 FragColor;

layout (location = 0) in vec3 Normal;
layout (location = 1) in vec2 TexCoords;
layout (location = 2) in vec3 WorldPos;
layout (location = 3) in vec4 ColTilt;

// Will be updated each frame at the start.
layout (binding = 0, std140) uniform SceneUBO
{
    layout (offset = 0) mat4 projMtx;
    layout (offset = 64) mat4 viewMtx;
    layout (offset = 128) vec3 sunDir;
    layout (offset = 140) float time;
} scene;

layout (location = 0) uniform sampler2D tex;

void main()
{
    FragColor = vec4(mix(texture(tex, TexCoords).xyz, ColTilt.xyz, ColTilt.w), 1.0);
}