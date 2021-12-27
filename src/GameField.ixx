module;

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include <vector>
#include <imgui.h>

#include "Vertex.hpp"
#include "resource.h"

export module Game.GameField;
import Draw.Resources;
import Draw.Renderer;
import Draw.Window;
import Game.ShipInfo;

constexpr float BorderSize = 0.1F;
constexpr glm::vec3 UpNormal = glm::vec3(0.0F, 1.0F, 0.0F);

static Model::IndexInfo FieldSqInfo = Model::IndexInfo(0, 6);
static Model::IndexInfo BackgroundInfo = Model::IndexInfo(6, 12);
static glm::vec4 OceanColor(0.18F, 0.33F, 1.0F, 1.0F);

uint32_t vert(std::vector<Vertex>& vertices, std::unordered_map<Vertex, uint32_t>& uniqueCache,
	const Vertex& vert)
{
	auto [iter, result] = uniqueCache.try_emplace(vert, (uint32_t)vertices.size());
	if (result)
		vertices.emplace_back(vert);
	return iter->second;
}

void appendSquare(float topEdge, float botEdge, float leftEdge, float rightEdge,
	std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
	std::unordered_map<Vertex, uint32_t>& uniqueCache)
{

	uint32_t positions[4]
	{
		vert(vertices, uniqueCache, Vertex(
			glm::vec3(topEdge, 0.0F, leftEdge),
			UpNormal
		)), // Top-Left
		vert(vertices, uniqueCache, Vertex(
			glm::vec3(botEdge, 0.0F, leftEdge),
			UpNormal
		)), // Bottom-Left
		vert(vertices, uniqueCache, Vertex(
			glm::vec3(topEdge, 0.0F, rightEdge),
			UpNormal
		)), // Top-Right
		vert(vertices, uniqueCache, Vertex(
			glm::vec3(botEdge, 0.0F, rightEdge),
			UpNormal
		)) // Bottom-Right
	};

	indices.reserve(6);
	indices.emplace_back(positions[0]);
	indices.emplace_back(positions[1]);
	indices.emplace_back(positions[2]);
	indices.emplace_back(positions[2]);
	indices.emplace_back(positions[1]);
	indices.emplace_back(positions[3]);

}

export class GameField
{
public:

	GameField(uint32_t width, uint32_t height)
		: m_width(width)
		, m_height(height)
		, m_model()
		, m_pipeline(Resources::find<ShaderPipeline>("Cel"))
		, m_texture(Resources::find<Texture>("Dummy"))
	{

		assert(width != 0 && height != 0); // Size is required to be at least 1x1

		for (size_t i = 0; i < ShipCount; i++)
			m_shipModels[i] = Resources::find<Model>(ShipInfos[i].resName);

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<Model::Part> parts;
		std::unordered_map<Vertex, uint32_t> uniqueCache;

		// Get unscaled bounds
		auto usBounds = width > height ? glm::vec2(1.0F, height / (float)width)
									   : glm::vec2(width / (float)height, 1.0F);
		float usSquareSize = usBounds.x / (float)width; // Get unscaled square
		float usBorderAxis = usSquareSize * BorderSize; // Get unscaled border size at an axis in a square
		float scale = (std::max(width, height) * usSquareSize + usBorderAxis) / 1.0F; // Get scale value
		float borderSize = usBorderAxis / 2.0F / scale; // Scale border, divide by two to get the border for an edge.
		m_sqaureSize = usSquareSize / scale; // Scale square size
		constexpr float edge = 0.5F;

		// Create square model
		appendSquare(m_sqaureSize / 2.0F - borderSize, borderSize - m_sqaureSize / 2.0F,
			         borderSize - m_sqaureSize / 2.0F, m_sqaureSize / 2.0F - borderSize,
				     vertices, indices, uniqueCache);

		// Create outer background model
		glm::vec2 topLeftPos;
		if (width < height)
		{
			float half = width * m_sqaureSize / 2.0F + borderSize;
			topLeftPos = glm::vec2(half, 0.5F);
			appendSquare(-edge, edge, edge, half,
				vertices, indices, uniqueCache);
			appendSquare(-edge, edge, -half, -edge,
				vertices, indices, uniqueCache);
		}
		else if (height < width)
		{
			float half = height * m_sqaureSize / 2.0F + borderSize;
			topLeftPos = glm::vec2(0.5F, half);
			appendSquare(-edge, -half, edge, -edge,
				vertices, indices, uniqueCache);
			appendSquare(half, edge, edge, -edge,
				vertices, indices, uniqueCache);
		}
		else
		{
			topLeftPos = glm::vec2(0.5F, 0.5F);
		}

		// Setup top-left position
		m_topLeftPos = glm::vec2(
		    borderSize - topLeftPos.x,
			topLeftPos.y - borderSize
		);

		// Setup border info: offset
		m_borderInfo.offset = indices.size();

		// Generate top and down bars
		appendSquare(2.0F * borderSize - topLeftPos.y, -topLeftPos.y, -topLeftPos.x, topLeftPos.x,
			vertices, indices, uniqueCache);
		appendSquare(topLeftPos.y, topLeftPos.y - 2.0F * borderSize, -topLeftPos.x,  topLeftPos.x,
			vertices, indices, uniqueCache);

		// Generate vertical bars
		for (uint32_t i = 0; i <= m_width; i++)
		{
			appendSquare(topLeftPos.y - 2.0F * borderSize,                    // Top
						 2.0F * borderSize - topLeftPos.y,                    // Bottom
						 i * m_sqaureSize - topLeftPos.x,                     // Left
				         i * m_sqaureSize - topLeftPos.x + 2.0F * borderSize, // Right
				vertices, indices, uniqueCache);
		}

		// Generate horizontal bars
		for (uint32_t y = 1; y < m_height; y++)
			for (uint32_t x = 0; x < m_width; x++)
			{
				appendSquare(2.0F * borderSize - topLeftPos.y + y * m_sqaureSize,  // Top
					         -topLeftPos.y + y * m_sqaureSize,                     // Bottom
						     -topLeftPos.x + x * m_sqaureSize + 2.0F * borderSize, // Left
					         -topLeftPos.x + (x + 1) * m_sqaureSize,               // Right
					vertices, indices, uniqueCache);
			}

		// Setup border info: size
		m_borderInfo.size = indices.size() - m_borderInfo.offset;

		// Setup per instance data
		auto fieldSqAmount = width * height;
		
		m_fieldSquaresColorTilt = new glm::vec4[fieldSqAmount];
		for (size_t i = 0; i < fieldSqAmount; i++)
			m_fieldSquaresColorTilt[i] = OceanColor;
		
		m_fieldSquareTransforms = new glm::mat4[fieldSqAmount];
		setTransform(glm::mat4(1.0F));

		// Upload model
		m_model = Model(vertices, indices, parts);

		// Calculate ship scaling
		float shipScale = m_sqaureSize / 10.0F;
		m_shipScale = glm::scale(glm::mat4(1.0F), glm::vec3(shipScale, shipScale, shipScale));

	}

	~GameField()
	{
		delete[] m_fieldSquaresColorTilt;
		delete[] m_fieldSquareTransforms;
	}

	void draw(SceneRenderer& renderer) const noexcept
	{

		Material material
		{
			.pipeline = m_pipeline,
			.texture = m_texture,
			.color_tilt = glm::vec4(1.0F, 1.0F, 1.0F, 1.0F) // White for border
		};

		// Render border
		renderer.draw(m_transform, &m_model, m_borderInfo, material);

		// Render background
		if (m_width != m_height)
		{
			material.color_tilt = OceanColor; // Change material to ocean blue
			renderer.draw(m_transform, &m_model, BackgroundInfo, material);
		}

		// Render field squares
		size_t i = 0;
		for (uint32_t y = 0; y < m_height; y++)
			for (uint32_t x = 0; x < m_width; x++, i++)
			{
				material.color_tilt = m_fieldSquaresColorTilt[i]; // Change material to the set field color
				renderer.draw(m_fieldSquareTransforms[i],
					&m_model, FieldSqInfo, material);
			}
			
	}

	void setTransform(glm::mat4 transform)
	{
		m_transform = transform;
		size_t i = 0;
		for (uint32_t y = 0; y < m_height; y++)
			for (uint32_t x = 0; x < m_width; x++, i++)
				m_fieldSquareTransforms[i] = calcTransform({ x + 0.5F, y + 0.5F });
	}

	size_t size() const noexcept
	{
		return m_width * m_height;
	}

	glm::vec4& color(size_t index) noexcept
	{
		return m_fieldSquaresColorTilt[index];
	}

	const glm::vec4& color(size_t index) const noexcept
	{
		return m_fieldSquaresColorTilt[index];
	}

	size_t index(uint32_t x, uint32_t y) const noexcept
	{
		return m_width * y + x;
	}

	glm::vec2 cursorPos(const SceneRenderer& renderer)
	{

		// Calculate top left & top right corner position in the opengl canvas
		auto& ubo = renderer.ubo();
		glm::vec2 topLeft = ubo.projMtx * ubo.viewMtx * m_transform * glm::vec4(
			m_topLeftPos.y,
			0.0F,
			m_topLeftPos.x,
			1.0F
		);
		glm::vec2 botRight = ubo.projMtx * ubo.viewMtx * m_transform * glm::vec4(
			m_topLeftPos.y - (m_height * m_sqaureSize),
			0.0F,
			m_topLeftPos.x + (m_width * m_sqaureSize),
			1.0F
		);
		
		// Convert cursor glfw position to an opengl canvas position
		auto cursorPos = Window::Properties::CursorPos;
		auto winSize = Window::Properties::WindowSize;
		auto cursorPosGL = glm::vec2(
			(cursorPos.x / (winSize.x - 1)) *  2.0F - 1.0F,
			(cursorPos.y / (winSize.y - 1)) * -2.0F + 1.0F
		);

		// Calculate field size in the opengl canvas
		auto fieldSize = glm::abs(glm::vec2(
			glm::abs(topLeft.x + 1.0F) - glm::abs(botRight.x + 1.0F),
			glm::abs(topLeft.y - 1.0F) - glm::abs(botRight.y - 1.0F)
		));

		// Calculate size of squares in the opengl canvas
		auto sqSize = fieldSize / glm::vec2(m_width, m_height);

		// Calculate cursor position with (0;0) being the top-left corner
		auto cursorPosGLTL = glm::vec2(
			cursorPosGL.x - topLeft.x,
			topLeft.y - cursorPosGL.y
		);
		
		// Scale the position, so 1 unit equals a full square
		return cursorPosGLTL / sqSize;

	}

	ShipPosition shipPos(glm::vec2 pos, ShipType type)
	{

		// Get ship info
		auto& info = ShipInfos[type];
		bool odd = info.size % 2;

		// Get mouse location (without floating point)
		auto flooredPosition = glm::floor(pos);
		glm::vec2 limits{ m_width, m_height };

		// Get ship direction
		auto pointerTarget = glm::clamp(
			flooredPosition + glm::vec2(0.5F, 0.5F),
			{ 0.5F, 0.5F },
			limits - glm::vec2(0.5F, 0.5F)
		);
		auto shipDirection = pos - pointerTarget;
		bool horizontal = glm::abs(shipDirection.x) > glm::abs(shipDirection.y);

		// Setup Primary / Secondary
		typedef float glm::vec2::*FloatMemPointer;
		FloatMemPointer primary, secondary;
		if (horizontal)
		{
			primary = &glm::vec2::template x;
			secondary = &glm::vec2::template y;
		}
		else
		{
			primary = &glm::vec2::template y;
			secondary = &glm::vec2::template x;
		}

		// Get forward
		bool forward;
		float halfSize = info.size / 2.0F;
		if (pos.*primary < halfSize)
			forward = false;
		else if (pos.*primary > limits.*primary - halfSize)
			forward = true;
		else
			forward = shipDirection.*primary >= 0.0F;

		// Calculate position on the game field
		glm::vec2 result;
		result.*primary = flooredPosition.*primary;
		if (forward)
		{
			if (odd) result.*primary += 0.5F;
			result.*primary = glm::min(result.*primary, limits.*primary - halfSize);
		}
		else
		{
			if (odd) result.*primary -= 0.5F;
			result.*primary = glm::max(result.*primary, halfSize - 1.0F) + 1.0F;
		}
		result.*secondary = glm::clamp(flooredPosition.*secondary, 0.0F, limits.*secondary - 1.0F) + 0.5F;

		// Calculate looking direction of the ship (enum)
		uint8_t sdir = (horizontal ? SD_HORIZONTAL : SD_VERTICAL)
					 | (forward    ? SD_FORWARD    : SD_BACKWARDS);

		// Return ship position
		return ShipPosition {
			.position = result,
			.direction = (ShipDirection)sdir
		};

	}

	glm::mat4 calcTransform(glm::vec2 pos, float height = 0.0F)
	{
		return glm::translate(m_transform, glm::vec3(
			m_topLeftPos.y - pos.y * m_sqaureSize,
			height,
			m_topLeftPos.x + pos.x * m_sqaureSize)
		);
	}

	void render(SceneRenderer& renderer, ShipPosition& pos, ShipType type)
	{
		renderer.draw(calcTransform(pos.position, 1.0F) * m_shipScale * ShipRotations[pos.direction], m_shipModels[type]);
	}

private:

	// Info
	uint32_t m_width, m_height;
	glm::mat4 m_transform;
	
	// The created model
	Model m_model;

	// Common material data
	ShaderPipeline* m_pipeline;
	Texture* m_texture;

	// Field square data for each field
	glm::vec4* m_fieldSquaresColorTilt;
	glm::mat4* m_fieldSquareTransforms;

	// Field data required for translation
	glm::vec2 m_topLeftPos;
	float m_sqaureSize;

	// Border info
	Model::IndexInfo m_borderInfo;

	// Data for rendering ships
	glm::mat4 m_shipScale;
	Model* m_shipModels[ShipCount];
	
};