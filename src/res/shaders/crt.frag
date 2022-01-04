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
} Scene;

layout (location = 0) uniform sampler2D Texture;

#define PI 3.1415926538

void main()
{	
    vec4 pixelColor = texture(Texture, TexCoords);
    vec2 resolution = textureSize(Texture, 0);
    float intensity = (sin(TexCoords.y * resolution.y * 10) + 1) / 2;
    float brightnessCurve = (sin(TexCoords.y * resolution.y / 100 + Scene.time / 10) + 1) / 2 + 3;
    FragColor = vec4(pixelColor.xyz * intensity * brightnessCurve, pixelColor.a);

}