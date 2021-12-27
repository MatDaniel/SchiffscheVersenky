#version 450 core
#extension GL_ARB_enhanced_layouts : enable

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in mat4 aModlMtx; // Takes up 3,4,5,6
layout (location = 7) in vec4 aColTilt;

layout (location = 0) out vec3 Normal;
layout (location = 1) out vec2 TexCoords;
layout (location = 2) out vec3 WorldPos;
layout (location = 3) out vec4 ColTilt;

out gl_PerVertex
{
    vec4 gl_Position;
};

// Will be updated each frame at the start.
layout (binding = 0, std140) uniform SceneUBO
{
    layout (offset = 0) mat4 projMtx;
    layout (offset = 64) mat4 viewMtx;
    layout (offset = 128) vec3 sunDir;
    layout (offset = 140) float time;
} scene;

void main()
{
    TexCoords = aTexCoord;
    ColTilt = aColTilt;
    Normal = aNormal;
    WorldPos = vec3(aModlMtx * vec4(aPos, 1.0));
    
    gl_Position = scene.projMtx * scene.viewMtx * vec4(WorldPos, 1.0);
}