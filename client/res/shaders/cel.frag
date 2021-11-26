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
    float intensity = dot(normalize(-vec3(scene.sunDir)), normalize(Normal));
    if (intensity > 0.999)
    {
        FragColor = vec4(1.0);
    }
    else
    {
        if (intensity > 0.9)
        {
            intensity = 1.0;
        }
        else if (intensity > 0.5)
        {
            intensity = 0.7;
        }
        else if (intensity > -0.6)
        {
            intensity = 0.5;
        }
        else
        {
            intensity = 0.2;
        }
        FragColor = intensity * vec4(0.2, 0.6, 0.2, 1.0);
    }
}