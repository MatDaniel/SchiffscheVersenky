module;

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

export module Draw.Renderer.Input;

export struct SceneUBO
{
	glm::mat4 projMtx;
	glm::mat4 viewMtx;
	glm::vec3 sunDir;
	float time;
};

export struct InstanceData
{
	glm::mat4 modelMtx;
	glm::vec4 colorTilt;
};