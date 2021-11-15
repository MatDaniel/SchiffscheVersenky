#version 450 core

out vec4 FragColor;

in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Normal;
in vec3 CamPos;

// directional light info 
layout (binding = 1, std140) uniform SunLight
{
    vec4 direction;
} sunLight;

// material info
struct Material
{
    sampler2D albedoMap;
    sampler2D normalMap;
    // r = metallic, g = roughness, b = ambient occlusion
    sampler2D mraoMap;
};
layout (location = 0) uniform Material material;

void main()
{
    float intensity = dot(normalize(-vec3(sunLight.direction)), normalize(Normal));
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