module;

#include <glad/glad.h>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

export module Draw.Render.Input;

export namespace Draw::Render::Input
{

	// Uniforms
	constexpr GLuint TEXTURE_LOCATION = 0;

	// Input buffers
	constexpr GLuint POSITION_ATTRIBINDEX = 0;
	constexpr GLuint NORMAL_ATTRIBINDEX = 1;
	constexpr GLuint TEXCOORDS_ATTRIBINDEX = 2;
	constexpr GLuint MODELMTX_ATTRIBINDEX = 3; // Takes up 3,4,5,6
	constexpr GLuint COLTILT_ATTRIBINDEX = 7;

	struct SceneUBO
	{
		glm::mat4 projMtx;
		glm::mat4 viewMtx;
		glm::vec3 sunDir;
		float time;
	};

	struct InstanceData
	{
		glm::mat4 modelMtx;
		glm::vec4 colorTilt;
	};

}