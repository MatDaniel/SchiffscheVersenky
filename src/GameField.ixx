module;

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
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
import Network.Client;
import GameManagment;

using namespace GameManagment;
using namespace Network;
using namespace Draw;
using namespace std;
using namespace glm;

constexpr float BorderSize = 0.1F;
constexpr glm::vec3 UpNormal = glm::vec3(0.0F, 1.0F, 0.0F);

static Model::IndexInfo FieldSqInfo = Model::IndexInfo(0, 6);
static Model::IndexInfo BackgroundInfo = Model::IndexInfo(6, 12);
static vec4 BorderColor(1.0F, 1.0F, 1.0F, 1.0F);
static vec4 OceanColor(0.18F, 0.33F, 1.0F, 1.0F);
static vec4 CollisionColor(0.75F, 0.1F, 0.1F, 1.0F);
static vec4 IndirectCollisionColor(0.93F, 0.93F, 0.115F, 1.0F);

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

	GameField(const GameManager2& manager, GmPlayerField* playerField)
		: m_width(manager.InternalFieldDimensions.XComponent)
		, m_height(manager.InternalFieldDimensions.YComponent)
		, m_model()
		, m_pipeline(Resources::find<ShaderPipeline>("Cel"))
		, m_texture(Resources::find<Texture>("Dummy"))
		, m_gameManager(manager)
		, m_playerField(playerField)
	{

		assert(m_width != 0 && m_height != 0); // Size is required to be at least 1x1

		for (size_t i = 0; i < ShipTypeCount; i++)
			m_shipModels[i] = Resources::find<Model>(ShipInfos[i].resName);

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<Model::Part> parts;
		std::unordered_map<Vertex, uint32_t> uniqueCache;

		// Get unscaled bounds
		auto usBounds = m_width > m_height ? glm::vec2(1.0F, m_height / (float)m_width)
									       : glm::vec2(m_width / (float)m_height, 1.0F);
		float usSquareSize = usBounds.x / (float)m_width; // Get unscaled square
		float usBorderAxis = usSquareSize * BorderSize; // Get unscaled border size at an axis in a square
		float scale = (std::max(m_width, m_height) * usSquareSize + usBorderAxis) / 1.0F; // Get scale value
		float borderSize = usBorderAxis / 2.0F / scale; // Scale border, divide by two to get the border for an edge.
		m_sqaureSize = usSquareSize / scale; // Scale square size
		constexpr float edge = 0.5F;

		// Create square model
		appendSquare(m_sqaureSize / 2.0F - borderSize, borderSize - m_sqaureSize / 2.0F,
			         borderSize - m_sqaureSize / 2.0F, m_sqaureSize / 2.0F - borderSize,
				     vertices, indices, uniqueCache);

		// Create outer background model
		glm::vec2 topLeftPos;
		if (m_width < m_height)
		{
			float half = m_width * m_sqaureSize / 2.0F + borderSize;
			topLeftPos = glm::vec2(half, 0.5F);
			appendSquare(-edge, edge, edge, half,
				vertices, indices, uniqueCache);
			appendSquare(-edge, edge, -half, -edge,
				vertices, indices, uniqueCache);
		}
		else if (m_height < m_width)
		{
			float half = m_height * m_sqaureSize / 2.0F + borderSize;
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
		m_fieldSquareTransforms = new glm::mat4[m_width * m_height];
		setTransform(glm::mat4(1.0F));

		// Upload model
		m_model = Model(vertices, indices, parts);

		// Calculate ship scaling
		m_shipScaleValue = m_sqaureSize / 10.0F;
		m_shipScale = glm::scale(glm::mat4(1.0F), glm::vec3(m_shipScaleValue, m_shipScaleValue, m_shipScaleValue));

	}

	~GameField()
	{
		delete[] m_fieldSquareTransforms;
	}

	void setTransform(glm::mat4 transform)
	{
		m_transform = transform;
		size_t i = 0;
		for (uint32_t y = 0; y < m_height; y++)
			for (uint32_t x = 0; x < m_width; x++, i++)
				m_fieldSquareTransforms[i] = CalcTransform({ x + 0.5F, y + 0.5F });
	}

	size_t size() const noexcept
	{
		return m_width * m_height;
	}

	size_t index(uint32_t x, uint32_t y) const noexcept
	{
		return m_width * y + x;
	}

	void DrawBackground(SceneRenderer& renderer)
	{
		Material material
		{
			.pipeline = m_pipeline,
			.texture = m_texture
		};

		// Render border
		material.color_tilt = BorderColor;
		renderer.draw(m_transform, &m_model, m_borderInfo, material);

		// Render background
		if (m_width != m_height)
		{
			material.color_tilt = OceanColor;
			renderer.draw(m_transform, &m_model, BackgroundInfo, material);
		}
	}

	void DrawPlacedShips(SceneRenderer& renderer)
	{
		auto shipStates = m_playerField->GetShipStateList();
		for (auto& shipState : shipStates)
		{
			auto [type, pos] = DrawCastPos(shipState);
			DrawShip(renderer, type, pos);
		}
	}

	void DrawSquares(SceneRenderer& renderer, auto colorGetter)
	{
		Material material
		{
			.pipeline = m_pipeline,
			.texture = m_texture
		};

		size_t i = 0;
		for (uint8_t y = 0; y < m_height; y++)
			for (uint8_t x = 0; x < m_width; x++, i++)
			{
				material.color_tilt = colorGetter(x, y);
				renderer.draw(m_fieldSquareTransforms[i],
					&m_model, FieldSqInfo, material);
			}
	}

	void DrawShip(SceneRenderer& renderer, ShipType type, ShipPosition pos)
	{
		renderer.draw(CalcTransform(pos.position) * m_shipScale * ShipRotations[pos.direction], m_shipModels[type]);
	}

	// Setup phase
	//-------------

	void SetupPhase_DrawSquares(SceneRenderer& renderer)
	{
		DrawSquares(renderer, [this](auto x, auto y) -> vec4 {
			auto iter = m_InvalidPlacementSquares.find({ x, y });
			if (iter != m_InvalidPlacementSquares.end())
			{
				switch (iter->second)
				{
				case GmPlayerField::STATUS_DIRECT_COLLIDE:
					return CollisionColor;
				case GmPlayerField::STATUS_INDIRECT_COLLIDE:
					return IndirectCollisionColor;
				default:
					break;
				}
			}
			return OceanColor; // TODO: Get colors by game info
			});
	}

	void SetupPhase_PlaceShip(SceneRenderer& renderer, ShipType type)
	{

		// Get rendering variables
		auto selected = GetCursorPos(renderer);
		if (selected.x < 0.0F || selected.x > m_width || selected.y < 0.0F || selected.y > m_height)
			return; // Don't run if cursor is out of bounds.
		auto pos = GetShipPos(selected, type);
		
		// Convert rendering variables to network compatible ones
		auto shipClass = NetCastType(type);
		auto shipOrientation = NetCastRot(pos.direction);
		auto shipPosition = NetCastPos(type, pos);

		// Probe the state before placing the ship
		// and place it if no invalid states are returned.
		auto result = m_playerField->ProbeShipPlacement(shipClass, shipOrientation, shipPosition);
		if (result.empty())
			m_playerField->PlaceShipBypassSecurityChecks(shipClass, shipOrientation, shipPosition);
	}

	void SetupPhase_PreviewPlacement(SceneRenderer& renderer, ShipType type)
	{

		// Get ship position from cursor position
		auto selected = GetCursorPos(renderer);
		auto pos = GetShipPos(selected, type);

		// Draw ship with the cursor position
		DrawShip(renderer, type, pos);
		
		// Cast rendering variables to network compatible ones
		auto shipClass = NetCastType(type);
		auto shipOrientation = NetCastRot(pos.direction);
		auto shipPosition = NetCastPos(type, pos);

		// Ensure it's a new probe
		if (m_LastProbedShipClass == shipClass
			&& m_LastProbedShipOrientation == shipOrientation
			&& m_LastProbedPoint == shipPosition)
			return;
		
		// Set them to the last probed variables
		m_LastProbedShipClass = shipClass;
		m_LastProbedShipOrientation = shipOrientation;
		m_LastProbedPoint = shipPosition;

		// Probe the ship placement with these variables
		auto result = m_playerField->ProbeShipPlacement(shipClass, shipOrientation, shipPosition);

		// Clear state
		m_InvalidPlacementSquares.clear();
		
		// Save collision states to preview them on the field
		for (auto& [coords, status] : result)
		{
			switch (status)
			{
			case GmPlayerField::STATUS_DIRECT_COLLIDE:
			case GmPlayerField::STATUS_INDIRECT_COLLIDE:
				m_InvalidPlacementSquares.try_emplace(u8vec2{ coords.XComponent, coords.YComponent }, status);
				break;
			default:
				break;
			}
		}
	}

	// Utility
	//---------

	/**
	 * @brief Returns the location of the mouse on the game field.
	 */
	vec2 GetCursorPos(const SceneRenderer& renderer)
	{

		// Calculate top left & top right corner position in the opengl canvas
		auto& ubo = renderer.ubo();
		vec2 topLeft = ubo.projMtx * ubo.viewMtx * m_transform * vec4(
			m_topLeftPos.y,
			0.0F,
			m_topLeftPos.x,
			1.0F
		);
		vec2 botRight = ubo.projMtx * ubo.viewMtx * m_transform * vec4(
			m_topLeftPos.y - (m_height * m_sqaureSize),
			0.0F,
			m_topLeftPos.x + (m_width * m_sqaureSize),
			1.0F
		);

		// Convert cursor glfw position to an opengl canvas position
		auto cursorPos = Window::Properties::CursorPos;
		auto winSize = Window::Properties::WindowSize;
		auto cursorPosGL = glm::vec2(
			(cursorPos.x / (winSize.x - 1)) * 2.0F - 1.0F,
			(cursorPos.y / (winSize.y - 1)) * -2.0F + 1.0F
		);

		// Calculate field size in the opengl canvas
		auto fieldSize = abs(vec2(
			abs(topLeft.x + 1.0F) - abs(botRight.x + 1.0F),
			abs(topLeft.y - 1.0F) - abs(botRight.y - 1.0F)
		));

		// Calculate size of squares in the opengl canvas
		auto sqSize = fieldSize / vec2(m_width, m_height);

		// Calculate cursor position with (0;0) being the top-left corner
		auto cursorPosGLTL = vec2(
			cursorPosGL.x - topLeft.x,
			topLeft.y - cursorPosGL.y
		);

		// Scale the position, so 1 unit equals a full square
		return cursorPosGLTL / sqSize;

	}

	/**
	 * @brief Converts a cursor position to a ship position
	 *        for a specific ship type.
	 */
	ShipPosition GetShipPos(vec2 pos, ShipType type)
	{

		// Get ship info
		auto& info = ShipInfos[type];
		bool odd = info.size % 2;

		// Get mouse location (without floating point)
		auto flooredPosition = floor(pos);
		vec2 limits{ m_width, m_height };

		// Get ship direction
		auto pointerTarget = clamp(
			flooredPosition + vec2(0.5F, 0.5F),
			{ 0.5F, 0.5F },
			limits - vec2(0.5F, 0.5F)
		);
		auto shipDirection = pos - pointerTarget;
		bool horizontal = abs(shipDirection.x) > abs(shipDirection.y);

		// Setup Primary / Secondary
		typedef float vec2::* FloatMemPointer;
		FloatMemPointer primary, secondary;
		if (horizontal)
		{
			primary = &vec2::template x;
			secondary = &vec2::template y;
		}
		else
		{
			primary = &vec2::template y;
			secondary = &vec2::template x;
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
		vec2 result;
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
		result.*secondary = clamp(flooredPosition.*secondary, 0.0F, limits.*secondary - 1.0F) + 0.5F;

		// Calculate looking direction of the ship (enum)
		uint8_t sdir = (horizontal ? SD_HORIZONTAL : SD_VERTICAL)
			| (forward ? SD_FORWARD : SD_BACKWARDS);

		// Return ship position
		return ShipPosition{
			.position = result,
			.direction = (ShipDirection)sdir
		};

	}

	mat4 CalcTransform(const vec2& pos, float height = 0.0F) const
	{
		return translate(m_transform, vec3(
			m_topLeftPos.y - pos.y * m_sqaureSize,
			height,
			m_topLeftPos.x + pos.x * m_sqaureSize)
		);
	}

private:

	// Info
	uint8_t m_width, m_height;
	mat4 m_transform;
	
	// The created model
	Model m_model;

	// Common material data
	ShaderPipeline* m_pipeline;
	Texture* m_texture;

	// Field square data for each field
	mat4* m_fieldSquareTransforms;

	// Field data required for translation
	vec2 m_topLeftPos;
	float m_sqaureSize;

	// Border info
	Model::IndexInfo m_borderInfo;

	// Data for rendering ships
	float m_shipScaleValue;
	mat4 m_shipScale;
	Model* m_shipModels[ShipTypeCount];
	
	// Ship collision
	ShipClass m_LastProbedShipClass;
	ShipRotation m_LastProbedShipOrientation;
	PointComponent m_LastProbedPoint;
	unordered_map<u8vec2, GmPlayerField::PlayFieldStatus> m_InvalidPlacementSquares { };

	// Game manager
	const GameManager2& m_gameManager;
	GmPlayerField* m_playerField;

};