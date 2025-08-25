#pragma once
#include <SimpleMath.h>
#include "Renderer/RendererEnums.h"
#include "Renderer/Graphics/IndexBuffer.h"
#include "Renderer/Graphics/VertexBuffer.h"

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;
	using namespace TEN::Renderer;
	using namespace TEN::Renderer::Graphics;

	struct RendererHudBar
	{
		static constexpr auto COLOR_COUNT = 5;
		static constexpr auto SIZE_DEFAULT = Vector2(150.0f, 10.0f);

		VertexBuffer<Vertex> VertexBufferBorder;
		IndexBuffer	 IndexBufferBorder;
		VertexBuffer<Vertex> InnerVertexBuffer;
		IndexBuffer	 InnerIndexBuffer;

		/*
			Initializes status bar for rendering. Coordinates are set in screen space.
			Colors are set in 5 vertices as described in the diagram:
			0-------1
			| \   / |
			|  >2<  |
			| /   \ |
			3-------4
		*/
		RendererHudBar(ID3D11Device* devicePtr, const Vector2& pos, const Vector2& size, int borderSize, std::array<Vector4, COLOR_COUNT> colors)
		{
			constexpr auto VERTEX_COUNT = 5;
			constexpr auto UV_COUNT = 5;
			constexpr auto BORDER_UV_COUNT = 16;
			constexpr auto INDEX_COUNT = 12;
			constexpr auto BORDER_INDEX_COUNT = 56;

			auto barVertices = std::array<Vector3, VERTEX_COUNT>
			{
				Vector3(pos.x, HUD_ZERO_Y + pos.y, 0.5f),
					Vector3(pos.x + size.x, HUD_ZERO_Y + pos.y, 0.5f),
					Vector3(pos.x + (size.x * 0.5f), HUD_ZERO_Y + (pos.y + (size.y * 0.5f)), 0.5f),
					Vector3(pos.x, HUD_ZERO_Y + pos.y + size.y, 0.5f),
					Vector3(pos.x + size.x, HUD_ZERO_Y + (pos.y + size.y), 0.5f),
			};

			auto hudBorderSize = Vector2(
				borderSize * (DISPLAY_SPACE_RES.x / DISPLAY_SPACE_RES.y),
				borderSize);

			auto barBorderVertices = std::array<Vector3, 16>
			{
				// Top left
				Vector3(pos.x - hudBorderSize.x, HUD_ZERO_Y + (pos.y - hudBorderSize.y), 0.0f),
					Vector3(pos.x, HUD_ZERO_Y + (pos.y - hudBorderSize.y), 0.0f),
					Vector3(pos.x, HUD_ZERO_Y + pos.y, 0.0f),
					Vector3(pos.x - hudBorderSize.x, HUD_ZERO_Y + pos.y, 0.0f),

					// Top right
					Vector3(pos.x + size.x, HUD_ZERO_Y + (pos.y - hudBorderSize.y), 0.0f),
					Vector3(pos.x + size.x + hudBorderSize.x, HUD_ZERO_Y + (pos.y - hudBorderSize.y), 0.0f),
					Vector3(pos.x + size.x + hudBorderSize.x, HUD_ZERO_Y + pos.y, 0.0f),
					Vector3(pos.x + size.x, HUD_ZERO_Y + pos.y, 0.0f),

					// Bottom right
					Vector3(pos.x + size.x, HUD_ZERO_Y + (pos.y + size.y), 0.0f),
					Vector3(pos.x + size.x + hudBorderSize.x, HUD_ZERO_Y + (pos.y + size.y), 0.0f),
					Vector3(pos.x + size.x + hudBorderSize.x, HUD_ZERO_Y + ((pos.y + size.y) + hudBorderSize.y), 0.0f),
					Vector3(pos.x + size.x, HUD_ZERO_Y + ((pos.y + size.y) + hudBorderSize.y), 0.0f),

					// Bottom left
					Vector3(pos.x - hudBorderSize.x, HUD_ZERO_Y + (pos.y + size.y), 0.0f),
					Vector3(pos.x, HUD_ZERO_Y + (pos.y + size.y), 0.0f),
					Vector3(pos.x, HUD_ZERO_Y + ((pos.y + size.y) + hudBorderSize.y), 0.0f),
					Vector3(pos.x - hudBorderSize.x, HUD_ZERO_Y + ((pos.y + size.y) + hudBorderSize.y), 0.0f)
			};

			auto barUVs = std::array<Vector2, UV_COUNT>
			{
				Vector2::Zero,
					Vector2(1.0f, 0.0f),
					Vector2::One * 0.5f,
					Vector2(0.0f, 1.0f),
					Vector2::One,
			};
			auto barBorderUVs = std::array<Vector2, BORDER_UV_COUNT>
			{
				// Top left
				Vector2::Zero,
					Vector2(0.25f, 0.0f),
					Vector2::One * 0.25f,
					Vector2(0.0f, 0.25f),

					// Top right
					Vector2(0.75f, 0.0f),
					Vector2(1.0f, 0.0f),
					Vector2(1.0f, 0.25f),
					Vector2(0.75f, 0.25f),

					// Bottom right
					Vector2::One * 0.75f,
					Vector2(1.0f, 0.75f),
					Vector2::One,
					Vector2(0.75f, 1.0f),

					// Bottom left
					Vector2(0.0f, 0.75f),
					Vector2(0.25f, 0.75f),
					Vector2(0.25f, 1.0f),
					Vector2(0.0f, 1.0f)
			};

			auto barIndices = std::array<int, INDEX_COUNT>
			{
				2, 1, 0,
					2, 0, 3,
					2, 3, 4,
					2, 4, 1
			};
			auto barBorderIndices = std::array<int, BORDER_INDEX_COUNT>
			{
				// Top left
				0, 1, 3, 1, 2, 3,

					// Top center
					1, 4, 2, 4, 7, 2,

					// Top right
					4, 5, 7, 5, 6, 7,

					// Right
					7, 6, 8, 6, 9, 8,

					// Bottom right
					8, 9, 11, 9, 10, 11,

					// Bottom
					13, 8, 14, 8, 11, 14,

					// Bottom left
					12, 13, 15, 13, 14, 15,

					// Left
					3, 2, 12, 2, 13, 12,

					// Center
					2, 7, 13, 7, 8, 13
			};

			auto vertices = std::array<Vertex, VERTEX_COUNT>{};
			for (int i = 0; i < VERTEX_COUNT; i++)
			{
				vertices[i].Position = barVertices[i];
				vertices[i].Color = colors[i];
				vertices[i].UV = barUVs[i];
			}

			InnerVertexBuffer = VertexBuffer<Vertex>(devicePtr, (int)vertices.size(), &vertices[0]);
			InnerIndexBuffer = IndexBuffer(devicePtr, (int)barIndices.size(), barIndices.data());

			auto borderVertices = std::array<Vertex, barBorderVertices.size()>{};
			for (int i = 0; i < barBorderVertices.size(); i++)
			{
				borderVertices[i].Position = barBorderVertices[i];
				borderVertices[i].Color = Vector4::One;
				borderVertices[i].UV = barBorderUVs[i];
			}

			VertexBufferBorder = VertexBuffer<Vertex>(devicePtr, (int)borderVertices.size(), &borderVertices[0]);
			IndexBufferBorder = IndexBuffer(devicePtr, (int)barBorderIndices.size(), barBorderIndices.data());
		}
	};
}
