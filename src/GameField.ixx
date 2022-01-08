module;

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <unordered_map>
#include <vector>
#include <variant>

#include "Vertex.hpp"
#include "resource.h"

export module Game.GameField;
import Draw.Resources;
import Draw.Render;
import Draw.Renderer;
import Draw.Window;
import Game.ShipInfo;
import Network.Client;
import GameManagement;

using namespace GameManagement;
using namespace Network;
using namespace Draw;
using namespace std;
using namespace glm;

constexpr float BorderSize = 0.1F;
constexpr vec3 UpNormal = vec3(0.0F, 1.0F, 0.0F);

static Render::IndexRange FieldSquareRange { 0, 6 };
static Render::IndexRange BackgroundRange { 6, 12 };
static mat4 OceanVerticalTransforms[2]
{
	translate(mat4(1.0F), vec3(-1.0F, 0.0F, 0.0F)),
	translate(mat4(1.0F), vec3(1.0F, 0.0F, 0.0F))
};

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

// Types
export using CursorPosition = typename vec2;

using NoState = typename monostate;

struct SetupState
{

	// Constructor
	SetupState() = default;

	// Mouse
	CursorPosition CursorPos;
	ShipPosition SelectedShipPos;

	// Properties for selection
	ShipType SelectedShip{ Draw::ST_DESTROYER };

	// Properties for collision
	unordered_map<u8vec2, GmPlayerField::PlayFieldStatus> InvalidPlacementSquares { };

};

struct GameState
{

	// Mouse
	bool HideCursor { true };
	CursorPosition CursorPos;

};

using State = typename variant<NoState, SetupState, GameState>;

export class GameField
{
public:

	GameField(const GameManager2& manager, GmPlayerField* playerField, GmPlayerField* opponentField, mat4 transform = mat4(1.0F))
		: m_Width(manager.InternalFieldDimensions.XComponent)
		, m_Height(manager.InternalFieldDimensions.YComponent)
		, m_GameManager(manager)
		, m_PlayerField(playerField)
		, m_OpponentField(opponentField)
	{

		assert(m_Width != 0 && m_Height != 0); // Size is required to be at least 1x1

		for (size_t i = 0; i < ShipTypeCount; i++)
			m_ShipModels[i] = Resources::find<Render::ConstModel>(ShipInfos[i].resName);

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::unordered_map<Vertex, uint32_t> uniqueCache;

		// Get unscaled bounds
		auto usBounds = m_Width > m_Height ? glm::vec2(1.0F, m_Height / (float)m_Width)
									       : glm::vec2(m_Width / (float)m_Height, 1.0F);
		float usSquareSize = usBounds.x / (float)m_Width; // Get unscaled square
		float usBorderAxis = usSquareSize * BorderSize; // Get unscaled border size at an axis in a square
		float usBorder = usBorderAxis / 2.0F; // Get unscaled border size at an edge in a square
		float scale = (std::max(m_Width, m_Height) * usSquareSize + usBorderAxis) / 1.0F; // Get scale value
		float borderAxis = usBorderAxis/ scale; // Scale border
		float borderSize = borderAxis / 2.0F; // Divide by two to get the border for an edge
		m_SqaureSize = usSquareSize / scale; // Scale square size
		float squareScale = m_SqaureSize - borderAxis; // Used later for transform
		constexpr float edge = 0.5F;

		// Create square model
		appendSquare(0.5F, -0.5F,
			         -0.5F, 0.5F,
			         vertices, indices, uniqueCache);

		// Create outer background model
		glm::vec2 topLeftPos;
		if (m_Width < m_Height)
		{
			float half = m_Width * m_SqaureSize / 2.0F + borderSize;
			topLeftPos = glm::vec2(half, 0.5F);
			appendSquare(-edge, edge, edge, half,
				vertices, indices, uniqueCache);
			appendSquare(-edge, edge, -half, -edge,
				vertices, indices, uniqueCache);
		}
		else if (m_Height < m_Width)
		{
			float half = m_Height * m_SqaureSize / 2.0F + borderSize;
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
		m_TopLeftPos = glm::vec2(
		    borderSize - topLeftPos.x,
			topLeftPos.y - borderSize
		);

		// Setup border info: offset
		m_BorderRange.offset = indices.size();

		// Generate top and down bars
		appendSquare(2.0F * borderSize - topLeftPos.y, -topLeftPos.y, -topLeftPos.x, topLeftPos.x,
			vertices, indices, uniqueCache);
		appendSquare(topLeftPos.y, topLeftPos.y - 2.0F * borderSize, -topLeftPos.x,  topLeftPos.x,
			vertices, indices, uniqueCache);

		// Generate vertical bars
		for (uint32_t i = 0; i <= m_Width; i++)
		{
			appendSquare(topLeftPos.y - 2.0F * borderSize,                    // Top
						 2.0F * borderSize - topLeftPos.y,                    // Bottom
						 i * m_SqaureSize - topLeftPos.x,                     // Left
				         i * m_SqaureSize - topLeftPos.x + 2.0F * borderSize, // Right
				vertices, indices, uniqueCache);
		}

		// Generate horizontal bars
		for (uint32_t y = 1; y < m_Height; y++)
			for (uint32_t x = 0; x < m_Width; x++)
			{
				appendSquare(2.0F * borderSize - topLeftPos.y + y * m_SqaureSize,  // Top
					         -topLeftPos.y + y * m_SqaureSize,                     // Bottom
						     -topLeftPos.x + x * m_SqaureSize + 2.0F * borderSize, // Left
					         -topLeftPos.x + (x + 1) * m_SqaureSize,               // Right
					vertices, indices, uniqueCache);
			}

		// Setup border info: size
		m_BorderRange.size = indices.size() - m_BorderRange.offset;

		// Upload model
		m_GameFieldMesh = Render::ConstMesh({ vertices }, { indices });

		// Setup per instance data
		m_FieldSquareTransforms = new mat4[m_Width * m_Height];
		size_t i = 0;
		for (uint32_t y = 0; y < m_Height; y++)
			for (uint32_t x = 0; x < m_Width; x++, i++)
				m_FieldSquareTransforms[i] = glm::scale(TranslateByIdentity({ x + 0.5F, y + 0.5F }), { squareScale, squareScale, squareScale });

		// Setup hit line
		for (uint8_t i = 0; i < Draw::ShipTypeCount; i++)
		{
			float markerHitScale = m_SqaureSize * ShipInfos[i].size - borderAxis;
			m_MarkerHitTransforms[i] = glm::scale(mat4(1.0F), { markerHitScale, squareScale, squareScale * 0.2F });
		}

		// Calculate ship scaling
		float ShipScaleValue = m_SqaureSize / 10.0F;
		m_ShipScale = glm::scale(mat4(1.0F), vec3(ShipScaleValue, ShipScaleValue, ShipScaleValue));

	}

	~GameField()
	{
		delete[] m_FieldSquareTransforms;
		GameManager2::ManualReset();
	}

	size_t size() const noexcept
	{
		return m_Width * m_Height;
	}

	size_t index(uint32_t x, uint32_t y) const noexcept
	{
		return m_Width * y + x;
	}

	void DrawBackground(Renderer& Renderer, const mat4& TargetTransform = mat4(1.0F))
	{

		// Render border
		Renderer.Draw(TargetTransform, m_GameFieldMesh.VertexArray(), m_BorderRange, *m_BorderMaterial);

		// Render background
		if (m_Width != m_Height)
			Renderer.Draw(TargetTransform, m_GameFieldMesh.VertexArray(), BackgroundRange, *m_WaterMaterial);

	}

	void DrawPlacedShips(Renderer& Renderer, const mat4& TargetTransform = mat4(1.0F))
	{
		auto shipStates = m_PlayerField->GetShipStateList();
		for (auto& shipState : shipStates)
		{
			auto [type, pos] = DrawCastPos(shipState);
			DrawShip(Renderer, type, pos, TargetTransform);
		}
	}

	void DrawSquares(Renderer& Renderer, auto MaterialGetter) const
	{
		size_t i = 0;
		for (uint8_t y = 0; y < m_Height; y++)
			for (uint8_t x = 0; x < m_Width; x++, i++)
				Renderer.Draw(m_FieldSquareTransforms[i], m_GameFieldMesh.VertexArray(), FieldSquareRange, *MaterialGetter(x, y));
	}

	void DrawSquares(Renderer& Renderer, auto MaterialGetter, const mat4& TargetTransform) const
	{
		size_t i = 0;
		for (uint8_t y = 0; y < m_Height; y++)
			for (uint8_t x = 0; x < m_Width; x++, i++)
				Renderer.Draw(TargetTransform * m_FieldSquareTransforms[i], m_GameFieldMesh.VertexArray(), FieldSquareRange, *MaterialGetter(x, y));
	}

	template <typename ...VArgs>
	void DrawShip(Renderer& renderer, ShipType type, ShipPosition pos, const mat4& TargetTransform, VArgs&&... VarArgs)
	{
		renderer.Draw(TranslateBy(TargetTransform, pos.position) * m_ShipScale * ShipRotations[pos.direction], m_ShipModels[type], forward<VArgs>(VarArgs)...);
	}

	void DrawOcean(Renderer& Renderer)
	{
		for (auto i = 0; i < 2; i++)
			Renderer.Draw(OceanVerticalTransforms[i], m_GameFieldMesh.VertexArray(), FieldSquareRange, *m_WaterMaterial);
		for (auto i = 0; i < 2; i++)
			Renderer.Draw(m_EdgeOceanTransforms[i], m_GameFieldMesh.VertexArray(), FieldSquareRange, *m_WaterMaterial);
	}


	void DrawOcean(Renderer& Renderer, const mat4& TargetTransform)
	{
		for (auto i = 0; i < 2; i++)
			Renderer.Draw(TargetTransform * OceanVerticalTransforms[i], m_GameFieldMesh.VertexArray(), FieldSquareRange, *m_WaterMaterial);
		for (auto i = 0; i < 2; i++)
			Renderer.Draw(TargetTransform * m_EdgeOceanTransforms[i], m_GameFieldMesh.VertexArray(), FieldSquareRange, *m_WaterMaterial);
	}

	// Game
	//------

	void Game_UpdateState()
	{
		m_State.emplace<GameState>();
	}
	
	void Game_DrawSquares(Renderer& Renderer, const mat4& TargetTransform = mat4(1.0F)) const
	{
		DrawSquares(Renderer, [this](auto x, auto y) -> Render::Material* {
			return m_WaterMaterial;
		}, TargetTransform);
	}

	// Enemy
	//-------

	void Enemy_DrawBackground(Renderer& renderer)
	{
		renderer.Draw(glm::mat4(1.0F), m_GameFieldMesh.VertexArray(), m_BorderRange, *m_EnemyBorderMaterial);
	}

	void Enemy_DrawHits(Renderer& renderer)
	{
		size_t i = 0;
		for (uint8_t y = 0; y < m_Height; y++)
			for (uint8_t x = 0; x < m_Width; x++, i++)
			{
				PointComponent TargetPosition {
					.XComponent = x,
					.YComponent = y
				};
				switch (m_OpponentField->GetCellStateByCoordinates(TargetPosition))
				{
				case CELL_WAS_SHOT_EMPTY:
					renderer.Draw(m_FieldSquareTransforms[i], m_MarkerFailModel);
					break;
				case CELL_SHIP_WAS_HIT:
					if (m_OpponentField->GetShipEntryForCordinate(TargetPosition)->Destroyed)
						renderer.Draw(m_FieldSquareTransforms[i], m_MarkerShipModel, vec4(0.0F, 0.0F, 0.0F, 1.0), 0.75F);
					else
						renderer.Draw(m_FieldSquareTransforms[i], m_MarkerShipModel);
					break;
				default:
					break;
				}
			}
	}

	void Enemy_UpdateCursorPosition(const SceneData& SceneRendererData, const SceneData& ScreenRendererData, const mat4& ScreenTransform)
	{
		auto CursorPos = Enemy_GetCursorPosition(SceneRendererData, ScreenRendererData, ScreenTransform);
		if (CursorPos.x < 0.0F || CursorPos.x > m_Width || CursorPos.y < 0.0F || CursorPos.y > m_Height)
		{
			// Hide the cursor out of bounds.
			get<GameState>(m_State).HideCursor = true;
			return;
		}
		PointComponent TargetPosition {
			.XComponent = (uint8_t)CursorPos.x,
			.YComponent = (uint8_t)CursorPos.y
		};

		switch (m_OpponentField->GetCellStateByCoordinates(TargetPosition))
		{
		case CELL_WAS_SHOT_EMPTY:
		case CELL_SHIP_WAS_HIT:
			get<GameState>(m_State).HideCursor = true;
			break;
		default:
			get<GameState>(m_State).HideCursor = false;
			get<GameState>(m_State).CursorPos = CursorPos;
			break;
		}
	}
	
	CursorPosition Enemy_GetCursorPosition(const SceneData& SceneRendererData, const SceneData& ScreenRendererData, const mat4& ScreenTransform)
	{
		vec2 ScreenTopLeft(-0.5F, 0.5F);
		vec2 ScreenBotRight(0.5F, -0.5F);
		vec2 ScreenInternalSize(2.0F, 2.0F);
		vec4 SceneIntermideateTopLeft(ScreenTransform * vec4(0.5F, 0.0F,  -0.5F, 1.0F));
		vec4 SceneIntermideateBotRight(ScreenTransform * vec4(-0.5F, 0.0F, 0.5F, 1.0F));
		vec2 SceneTopLeft(SceneIntermideateTopLeft.z, SceneIntermideateTopLeft.x);
		vec2 SceneBotRight(SceneIntermideateBotRight.z, SceneIntermideateBotRight.x);
		vec2 ScenePos(GetCursorPosByArea(SceneRendererData, 1, 1, SceneTopLeft, SceneBotRight));
		return { GetCursorPosByArea(ScreenRendererData, m_Width, m_Height, ScreenTopLeft, ScreenBotRight, ScenePos, ScreenInternalSize) };
	}

	void Enemy_Selection(Renderer& renderer)
	{
		if (!get<GameState>(m_State).HideCursor)
		{
			auto CursorPos = get<GameState>(m_State).CursorPos;
			auto XCoord = clamp<int32_t>(CursorPos.x, 0, m_Width - 1);
			auto YCoord = clamp<int32_t>(CursorPos.y, 0, m_Height - 1);
			renderer.Draw(m_FieldSquareTransforms[index(XCoord, YCoord)], m_MarkerSelectModel);
		}
	}

	void Enemy_DrawShips(Renderer& Renderer) const
	{
		auto shipStates = m_OpponentField->GetShipStateList();
		for (auto& shipState : shipStates)
		{
			auto [type, pos] = DrawCastPos(shipState);
			Enemy_DrawShip(Renderer, type, pos);
		}
	}

	void Enemy_DrawShip(Renderer& Renderer, ShipType Type, ShipPosition Position) const
	{
		mat4 Transform{ TranslateByIdentity(Position.position) * m_MarkerHitTransforms[Type] * ShipRotations[Position.direction] };
		Renderer.Draw(Transform, m_GameFieldMesh.VertexArray(), FieldSquareRange, *m_MarkerHitMaterial);
	}

	void Enemy_Hit()
	{
		if (!get<GameState>(m_State).HideCursor)
		{
			auto CursorPos = get<GameState>(m_State).CursorPos;
			PointComponent TargetPosition {
				.XComponent = (uint8_t)CursorPos.x,
				.YComponent = (uint8_t)CursorPos.y
			};
			if (m_OpponentField->StrikeCellAndUpdateShipList(TargetPosition))
			{
				get<GameState>(m_State).HideCursor = true;
			}
		}
	}

	// Setup phase
	//-------------

	void SetupPhase_UpdateState()
	{
		m_State.emplace<SetupState>();
	}

	void SetupPhase_PrepareCursor(const SceneData& RendererData)
	{
		auto& State = get<SetupState>(m_State);
		State.CursorPos = GetCursorPos(RendererData);
		State.SelectedShipPos = GetShipPos(State.CursorPos, State.SelectedShip);
	}

	void SetupPhase_DrawSquares(Renderer& Renderer) const
	{
		DrawSquares(Renderer, [this](auto x, auto y) -> Render::Material* {
			auto iter = get<SetupState>(m_State).InvalidPlacementSquares.find({ x, y });
			if (iter != get<SetupState>(m_State).InvalidPlacementSquares.end())
			{
				switch (iter->second)
				{
				case GmPlayerField::STATUS_DIRECT_COLLIDE:
					return m_CollisionMaterial;
				case GmPlayerField::STATUS_INDIRECT_COLLIDE:
					return m_CollisionIndirectMaterial;
				default:
					break;
				}
			}
			return m_WaterMaterial;
		});
	}

	void SetupPhase_PlaceShip()
	{

		// Get rendering variables
		auto CursorHoverPos = get<SetupState>(m_State).CursorPos;
		if (CursorHoverPos.x < 0.0F || CursorHoverPos.x > m_Width || CursorHoverPos.y < 0.0F || CursorHoverPos.y > m_Height)
			return; // Don't run if cursor is out of bounds.
		auto ShipPosition = get<SetupState>(m_State).SelectedShipPos;
		
		// Convert rendering variables to network compatible ones
		auto NetShipClass = NetCastType(get<SetupState>(m_State).SelectedShip);
		auto NetShipOrientation = NetCastRot(ShipPosition.direction);
		auto NetShipPosition = NetCastPos(get<SetupState>(m_State).SelectedShip, ShipPosition);

		// Probe the state before placing the ship and ensure no invalid states are returned.
		auto result = m_PlayerField->ProbeShipPlacement(NetShipClass, NetShipOrientation, NetShipPosition);
		if (result.size())
			return;

		auto PlacedShipCounts = m_PlayerField->GetNumberOfShipsPlacedAndInverse(true);
		m_PlayerField->PlaceShipBypassSecurityChecks(NetShipClass, NetShipOrientation, NetShipPosition);

		// Ensure no more units are available of the current ship type,
		// but the game field is not completed yet and there are units
		// of different ship types left.
		if (PlacedShipCounts.ShipCounts[NetShipClass] > 1 || PlacedShipCounts.GetTotalCount() <= 1)
			return;

		// Try to find any ship with units available that is larger than the current ship.
		for (uint8_t NextShipClass = NetShipClass + 1; NextShipClass < ShipTypeCount; NextShipClass++)
			if (PlacedShipCounts.ShipCounts[NextShipClass])
			{
				get<SetupState>(m_State).SelectedShip = DrawCastType((ShipClass)NextShipClass);
				return;
			}

		// Try to find any ship with units available that is smaller than the current ship.
		for (uint8_t NextShipClass = NetShipClass - 1; NextShipClass < ShipTypeCount; NextShipClass--)
			if (PlacedShipCounts.ShipCounts[NextShipClass])
			{
				get<SetupState>(m_State).SelectedShip = DrawCastType((ShipClass)NextShipClass);
				return;
			}
				
	}

	void SetupPhase_DestroyShip()
	{
		// Get rendering variables
		auto CursorHoverPos = get<SetupState>(m_State).CursorPos;
		if (CursorHoverPos.x < 0.0F || CursorHoverPos.x > m_Width || CursorHoverPos.y < 0.0F || CursorHoverPos.y > m_Height)
			return; // Don't run if cursor is out of bounds.
		PointComponent TargetPoint {
			.XComponent = (uint8_t)CursorHoverPos.x,
			.YComponent = (uint8_t)CursorHoverPos.y
		};

		auto OldPlacedShips = m_PlayerField->GetNumberOfShipsPlacedAndInverse(false);
		if (m_PlayerField->RemoveShipFromField(TargetPoint))
		{
			auto PlacedShips = m_PlayerField->GetNumberOfShipsPlacedAndInverse(false);
			for (uint8_t i = 0; i < ShipTypeCount; i++)
				if (OldPlacedShips.ShipCounts[i] != PlacedShips.ShipCounts[i])
				{
					get<SetupState>(m_State).SelectedShip = (ShipType)i;
					return;
				}
		}
	}

	void SetupPhase_PreviewPlacement(Renderer& Renderer)
	{

		// Get ship position
		auto ShipPosition = get<SetupState>(m_State).SelectedShipPos;

		// Draw ship with the cursor position
		DrawShip(Renderer, get<SetupState>(m_State).SelectedShip, ShipPosition, mat4(1.0F), vec4(0.235F, 0.76F, 0.13F, 1.0F));
		
		// Cast rendering variables to network compatible ones
		auto NetShipClass = NetCastType(get<SetupState>(m_State).SelectedShip);
		auto NetShipOrientation = NetCastRot(ShipPosition.direction);
		auto NetShipPosition = NetCastPos(get<SetupState>(m_State).SelectedShip, ShipPosition);

		// Probe the ship placement with these variables
		auto ProbeResult = m_PlayerField->ProbeShipPlacement(NetShipClass, NetShipOrientation, NetShipPosition);

		// Clear state
		get<SetupState>(m_State).InvalidPlacementSquares.clear();
		
		// Save collision states to preview them on the field
		for (auto& [Coords, Status] : ProbeResult)
		{
			switch (Status)
			{
			case GmPlayerField::STATUS_DIRECT_COLLIDE:
			case GmPlayerField::STATUS_INDIRECT_COLLIDE:
				get<SetupState>(m_State).InvalidPlacementSquares.try_emplace(u8vec2{ Coords.XComponent, Coords.YComponent }, Status);
				break;
			default:
				break;
			}
		}
	}

	void SetupPhase_ShipSelection(Renderer& Renderer, const SceneData& RendererData, bool* WasLeftClicked)
	{
		auto SquareSize = 0.15F;
		auto ScaleValue = SquareSize / 10.0F;
		auto ShipScale = scale(mat4(1.0F), vec3(ScaleValue, ScaleValue, ScaleValue));
		auto ShipDistance = SquareSize + 0.02F;
		auto PlacedShipCounts = m_PlayerField->GetNumberOfShipsPlacedAndInverse(true);
		for (uint8_t i = 0, n = 0; i < ShipTypeCount; i++)
		{
			if (PlacedShipCounts.ShipCounts[i])
			{
				auto CurrentShip = (ShipType)i;
				auto ShipSize = ShipInfos[i].size;
				auto ShipYOffset = (2.0F - n++) * ShipDistance;
				vec2 TopLeft{
					0.55F,
					ShipYOffset + SquareSize / 2.0F
				};
				vec2 BotRight{
					0.55F + SquareSize * ShipSize,
					ShipYOffset - SquareSize / 2.0F
				};
				auto CursorPos = GetCursorPosByArea(RendererData, 1, 1, TopLeft, BotRight);
				bool IsCursorHovering = CursorPos.x >= 0.0F && CursorPos.x <= 1.0F
					&& CursorPos.y >= 0.0F && CursorPos.y <= 1.0F;
				auto ShipTransform = translate(mat4(1.0F), vec3(ShipYOffset, 0.0F, 0.55F + SquareSize * ShipSize / 2.0F));
				if (*WasLeftClicked && IsCursorHovering)
				{
					get<SetupState>(m_State).SelectedShip = CurrentShip;
					*WasLeftClicked = false; // Reset
				}
				if (i == get<SetupState>(m_State).SelectedShip)
					Renderer.Draw(ShipTransform * ShipScale, m_ShipModels[i], vec4(0.0F, 0.0F, 1.0F, 1.0F));
				else if (IsCursorHovering)
					Renderer.Draw(ShipTransform * ShipScale, m_ShipModels[i], vec4(0.0F, 0.0F, 1.0F, 1.0F), 0.5F);
				else
					Renderer.Draw(ShipTransform * ShipScale, m_ShipModels[i]);
			}
		}
	}

	bool SetupPhase_IsFinished()
	{
		return m_PlayerField->GetNumberOfShipsPlaced() >= m_GameManager.InternalShipCount.GetTotalCount();
	}

	// Utility
	//---------

	/**
	 * @brief Returns the location of the mouse on the game field.
	 */
	CursorPosition GetCursorPosByArea(const SceneData& RendererData,
		uint32_t Width, uint32_t Height,
		vec2 InTopLeft, vec2 InBotRight,
		vec2 CursorWindowPosition, vec2 WindowSize)
	{

		// Calculate top left & top right corner position in the opengl canvas
		auto& ubo = RendererData.GetUBO();
		vec2 topLeft = ubo.projMtx * ubo.viewMtx * vec4(
			InTopLeft.y,
			0.0F,
			InTopLeft.x,
			1.0F
		);
		vec2 botRight = ubo.projMtx * ubo.viewMtx * vec4(
			InBotRight.y,
			0.0F,
			InBotRight.x,
			1.0F
		);

		// Convert cursor glfw position to an opengl canvas position
		auto cursorPosGL = glm::vec2(
			(CursorWindowPosition.x / (WindowSize.x - 1)) * 2.0F - 1.0F,
			(CursorWindowPosition.y / (WindowSize.y - 1)) * -2.0F + 1.0F
		);

		// Calculate field size in the opengl canvas
		auto fieldSize = abs(vec2(
			abs(topLeft.x + 1.0F) - abs(botRight.x + 1.0F),
			abs(topLeft.y - 1.0F) - abs(botRight.y - 1.0F)
		));

		// Calculate size of squares in the opengl canvas
		auto sqSize = fieldSize / vec2(Width, Height);

		// Calculate cursor position with (0;0) being the top-left corner
		auto cursorPosGLTL = vec2(
			cursorPosGL.x - topLeft.x,
			topLeft.y - cursorPosGL.y
		);

		// Scale the position, so 1 unit equals a full square
		return cursorPosGLTL / sqSize;

	}

	/**
	 * @brief Returns the location of the mouse on the game field.
	 */
	CursorPosition GetCursorPosByArea(const SceneData& RendererData,
		uint32_t Width, uint32_t Height,
		vec2 InTopLeft, vec2 InBotRight)
	{

		return GetCursorPosByArea(RendererData,
			Width, Height,
			InTopLeft, InBotRight,
			Window::Properties::CursorPos, Window::Properties::WindowSize);

	}
	
	/**
	 * @brief Returns the location of the mouse on the game field.
	 */
	CursorPosition GetCursorPos(const SceneData& RendererData)
	{

		return GetCursorPosByArea(RendererData, m_Width, m_Height,

			// Top Left
			{
				m_TopLeftPos.x,
				m_TopLeftPos.y
			},

			// Bottom Right
			{
				m_TopLeftPos.x + (m_Width * m_SqaureSize),
				m_TopLeftPos.y - (m_Height * m_SqaureSize)
			}

		);

	}

	/**
	 * @brief Converts a cursor position to a ship position
	 *        for a specific ship type.
	 */
	ShipPosition GetShipPos(CursorPosition CursorPos, ShipType Type)
	{

		// Get ship info
		auto& info = ShipInfos[Type];
		bool odd = info.size % 2;

		// Get mouse location (without floating point)
		auto flooredPosition = floor(CursorPos);
		vec2 limits{ m_Width, m_Height };

		// Get ship direction
		auto pointerTarget = clamp(
			flooredPosition + vec2(0.5F, 0.5F),
			{ 0.5F, 0.5F },
			limits - vec2(0.5F, 0.5F)
		);
		auto shipDirection = CursorPos - pointerTarget;
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
		if (CursorPos.*primary < halfSize)
			forward = false;
		else if (CursorPos.*primary > limits.*primary - halfSize)
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

	mat4 TranslateBy(const mat4& InitialTransformation,  const vec2& pos, float height = 0.0F) const
	{
		return translate(mat4(1.0F), vec3(
			m_TopLeftPos.y - pos.y * m_SqaureSize,
			height,
			m_TopLeftPos.x + pos.x * m_SqaureSize)
		);
	}

	mat4 TranslateByIdentity(const vec2& pos, float height = 0.0F) const
	{
		return TranslateBy(mat4(1.0F), pos, height);
	}
	
	void RecalculateOcean(const Render::FrameBuffer& TargetFB, float Height, float LeftShift = 0.0F)
	{
		float OceanWidth = (TargetFB.Width() * Height / TargetFB.Height() - 1.0F) / 2.0F;
		RecalculateOcean(OceanWidth + LeftShift, OceanWidth - LeftShift);
	}

	float RecalculateOceanWithMidLane(const Render::FrameBuffer& TargetFB, float Height, float MidLane)
	{
		float OceanWidth = (TargetFB.Width() * Height / TargetFB.Height() - 1.0F) / 2.0F;
		float OutWidth = OceanWidth - MidLane;
		RecalculateOcean(OutWidth, MidLane / 2.0F);
		return OutWidth;
	}

	void RecalculateOcean(float Left, float Right)
	{
		m_EdgeOceanTransforms[0] = scale(
			translate(mat4(1.0F), vec3(0.0F, 0.0F, 0.5F + Right / 2.0F)),
			vec3(3.0F, 1.0F, Right)
		);
		m_EdgeOceanTransforms[1] = scale(
			translate(mat4(1.0F), vec3(0.0F, 0.0F, -0.5F - Left / 2.0F)),
			vec3(3.0F, 1.0F, Left)
		);
	}

	void SwapFieldTargets(float DistanceMid, float DistanceOut)
	{
		swap(m_PlayerField, m_OpponentField);
		m_EdgeOceanTransforms[0] = glm::translate(mat4(1.0F), { 0.0F, 0.0F, DistanceMid }) * m_EdgeOceanTransforms[0];
		m_EdgeOceanTransforms[1] = glm::translate(mat4(1.0F), { 0.0F, 0.0F, DistanceOut }) * m_EdgeOceanTransforms[1];
	}

private:

	// General Properties
	uint8_t m_Width, m_Height;

	// Properties for translation
	vec2 m_TopLeftPos;
	float m_SqaureSize;

	// The created model
	Render::ConstMesh m_GameFieldMesh { };

	// Common material data
	Render::Material* m_WaterMaterial { Resources::find<Render::Material>("GameField_Water") };
	Render::Material* m_BorderMaterial { Resources::find<Render::Material>("GameField_Border") };
	Render::Material* m_CollisionMaterial { Resources::find<Render::Material>("GameField_Collision") };
	Render::Material* m_CollisionIndirectMaterial { Resources::find<Render::Material>("GameField_CollisionIndirect") };
	Render::Material* m_MarkerHitMaterial { Resources::find<Render::Material>("Screen_MarkerHit") };

	// Enemy field and screen properties
	Render::Material* m_EnemyBorderMaterial { Resources::find<Render::Material>("Screen_GameField") };
	Render::ConstModel* m_MarkerSelectModel { Resources::find<Render::ConstModel>("Markers/Select") };
	Render::ConstModel* m_MarkerShipModel { Resources::find<Render::ConstModel>("Markers/Ship") };
	Render::ConstModel* m_MarkerFailModel { Resources::find<Render::ConstModel>("Markers/Fail") };

	// Transforms for the line shown on the screen when a ship is completely down.
	mat4 m_MarkerHitTransforms[Draw::ShipTypeCount];

	// Field square data for each field
	mat4* m_FieldSquareTransforms;
	mat4 m_EdgeOceanTransforms[2];

	// Border info
	Render::IndexRange m_BorderRange;

	// Data for rendering ships
	mat4 m_ShipScale;
	Render::ConstModel* m_ShipModels[ShipTypeCount];

	// Internal Manager (Logic)
	const GameManager2& m_GameManager;
	GmPlayerField* m_PlayerField;
	GmPlayerField* m_OpponentField;

	// State
	State m_State;

};