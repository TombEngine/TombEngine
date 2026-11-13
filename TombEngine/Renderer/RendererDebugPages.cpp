#include "framework.h"
#include "Renderer/Renderer.h"

#include <cfloat>
#include <cstdarg>
#include <imgui.h>

#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/itemdata/creature_info.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/room.h"
#include "Renderer/Graphics/VRAMTracker.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"
#include "Specific/Input/InputAction.h"
#include "Specific/level.h"
#include "Specific/Structures/BitField.h"

using namespace TEN::Collision::Point;
using namespace TEN::Input;
using namespace TEN::Utils;

// Debug pages are drawn as non-interactive ImGui windows, so they never capture mouse or keyboard input.
// Each page is a two-column table of label and value rows.

namespace TEN::Renderer
{
	constexpr auto DEBUG_PAGE_WINDOW_FLAGS =
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_AlwaysAutoResize;

	constexpr auto DEBUG_PAGE_WINDOW_POS	= ImVec2(10.0f, 10.0f);
	constexpr auto DEBUG_PAGE_WINDOW_ALPHA	= 0.7f;
	constexpr auto DEBUG_PAGE_LABEL_WIDTH	= 230.0f;
	constexpr auto BYTES_PER_MEGABYTE		= 1024.0f * 1024.0f;

	static bool BeginDebugPage(const char* title)
	{
		ImGui::SetNextWindowPos(DEBUG_PAGE_WINDOW_POS, ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(DEBUG_PAGE_WINDOW_ALPHA);

		if (!ImGui::Begin(title, nullptr, DEBUG_PAGE_WINDOW_FLAGS))
		{
			ImGui::End();
			return false;
		}

		if (!ImGui::BeginTable("##DebugPageRows", 2, ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::End();
			return false;
		}

		ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, DEBUG_PAGE_LABEL_WIDTH);
		ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);
		return true;
	}

	static void EndDebugPage()
	{
		ImGui::EndTable();
		ImGui::End();
	}

	static void DebugPageSection(const char* title)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextDisabled("%s", title);
	}

	static void DebugPageRow(const char* label, const char* format, ...)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);

		va_list args;
		va_start(args, format);
		ImGui::TextV(format, args);
		va_end(args);
	}

	static const char* GetMoodName(MoodType mood)
	{
		switch (mood)
		{
		case MoodType::Attack:
			return "Attack";

		case MoodType::Stalk:
			return "Stalk";

		case MoodType::Escape:
			return "Escape";

		case MoodType::Bored:
			return "Bored";
		}

		return "Unknown";
	}

	static void DrawPathfindingEnemyRows(int playerBoxID, const std::vector<int>& creatures)
	{
		if (PathfindingDisplayIndex < 0)
		{
			if (!creatures.empty())
				DebugPageRow("Hint", "Push TAB to scroll through enemies");

			return;
		}

		if (creatures.empty() || creatures.size() <= PathfindingDisplayIndex)
			return;

		auto& enemy = g_Level.Items[creatures[PathfindingDisplayIndex]];
		const auto& creature = *(CreatureInfo*)enemy.Data;
		auto zoneType = creature.LOT.Zone;
		const auto& zones = g_Level.Zones[(int)zoneType][(int)FlipStatus];

		DebugPageSection("Enemy");
		DebugPageRow("Player zone number", "%d", playerBoxID == NO_VALUE ? NO_VALUE : zones[playerBoxID]);
		DebugPageRow("Name", "%s", enemy.Name.c_str());
		DebugPageRow("Box number", "%d", enemy.BoxNumber);
		DebugPageRow("Zone type", "%d", zoneType);
		DebugPageRow("Zone number", "%d", enemy.BoxNumber == NO_VALUE ? NO_VALUE : zones[enemy.BoxNumber]);
		DebugPageRow("Mood", "%s", GetMoodName(creature.Mood));
	}

	bool Renderer::HasDebugOverlayContent() const
	{
		return (_debugPage != RendererDebugPage::None || !_debugMessages.empty() || !_debugLabels.empty());
	}

	void Renderer::DrawDebugMessages()
	{
		if (_debugMessages.empty())
			return;

		const auto& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - DEBUG_PAGE_WINDOW_POS.x, DEBUG_PAGE_WINDOW_POS.y), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
		ImGui::SetNextWindowBgAlpha(DEBUG_PAGE_WINDOW_ALPHA);

		if (ImGui::Begin("Debug Messages", nullptr, DEBUG_PAGE_WINDOW_FLAGS))
		{
			for (const auto& message : _debugMessages)
			{
				if (message.empty())
				{
					ImGui::NewLine();
					continue;
				}

				ImGui::TextUnformatted(message.c_str());
			}
		}

		ImGui::End();
	}

	void Renderer::DrawDebugLabels()
	{
		constexpr auto FONT_SIZE_BASE_MULT = 0.5f;
		constexpr auto SHADOW_OFFSET	   = ImVec2(1.0f, 1.0f);
		constexpr auto SHADOW_COLOR		   = IM_COL32(0, 0, 0, 255);

		if (_debugLabels.empty())
			return;

		auto* drawList = ImGui::GetBackgroundDrawList();
		auto* font = ImGui::GetFont();
		auto displaySize = ImGui::GetIO().DisplaySize;
		auto factor = ImVec2(displaySize.x / DISPLAY_SPACE_RES.x, displaySize.y / DISPLAY_SPACE_RES.y);

		for (const auto& label : _debugLabels)
		{
			float fontSize = ImGui::GetFontSize() * (FONT_SIZE_BASE_MULT + label.Scale);
			auto textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.String.c_str());

			// Labels are horizontally centered on their position.
			auto pos = ImVec2((label.Position.x * factor.x) - (textSize.x / 2.0f), label.Position.y * factor.y);
			auto color = ImGui::ColorConvertFloat4ToU32(ImVec4(label.Color.x, label.Color.y, label.Color.z, label.Color.w));

			drawList->AddText(font, fontSize, ImVec2(pos.x + SHADOW_OFFSET.x, pos.y + SHADOW_OFFSET.y), SHADOW_COLOR, label.String.c_str());
			drawList->AddText(font, fontSize, pos, color, label.String.c_str());
		}
	}

	void Renderer::DrawDebugPages()
	{
		switch (_debugPage)
		{
		case RendererDebugPage::RendererStats:
			DrawRendererStatsPage();
			break;

		case RendererDebugPage::MemoryStats:
			DrawMemoryStatsPage();
			break;

		case RendererDebugPage::DimensionStats:
			DrawDimensionStatsPage();
			break;

		case RendererDebugPage::PlayerStats:
			DrawPlayerStatsPage();
			break;

		case RendererDebugPage::InputStats:
			DrawInputStatsPage();
			break;

		case RendererDebugPage::CollisionStats:
			DrawCollisionStatsPage();
			break;

		case RendererDebugPage::PathfindingStats:
			DrawPathfindingStatsPage();
			break;

		case RendererDebugPage::PortalStats:
			DrawPortalStatsPage();
			break;

		case RendererDebugPage::CollisionMeshStats:
			if (BeginDebugPage("Collision Mesh Stats"))
				EndDebugPage();

			break;

		case RendererDebugPage::WireframeMode:
			if (BeginDebugPage("Wireframe Mode"))
				EndDebugPage();

			break;

		default:
			// None and SkyDebug are handled elsewhere.
			break;
		}
	}

	void Renderer::DrawRendererStatsPage()
	{
		if (!BeginDebugPage("Renderer Stats"))
			return;

		DebugPageRow("FPS", "%3.2f", _fps);
		DebugPageRow("Resolution", "%d x %d", _graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
		DebugPageRow("GPU", "%s", g_Configuration.AdapterName.c_str());

		DebugPageSection("Timings");
		DebugPageRow("Update time", "%d", _timeUpdate);
		DebugPageRow("Frame time", "%d", _timeFrame);
		DebugPageRow("ControlPhase() time", "%d", ControlPhaseTime);

		DebugPageSection("Draw calls");
		DebugPageRow("Total", "%d", _numDrawCalls);
		DebugPageRow("    Rooms", "%d", _numRoomsDrawCalls);
		DebugPageRow("    Movables", "%d", _numMoveablesDrawCalls);
		DebugPageRow("    Statics", "%d", _numStaticsDrawCalls);
		DebugPageRow("    Instanced statics", "%d", _numInstancedStaticsDrawCalls);
		DebugPageRow("    Sprites", "%d", _numSpritesDrawCalls);
		DebugPageRow("    Instanced sprites", "%d", _numInstancedSpritesDrawCalls);
		DebugPageRow("Sorted", "%d", _numSortedRoomsDrawCalls + _numSortedMoveablesDrawCalls + _numSortedStaticsDrawCalls + _numSortedSpritesDrawCalls);
		DebugPageRow("    Rooms", "%d", _numSortedRoomsDrawCalls);
		DebugPageRow("    Movables", "%d", _numSortedMoveablesDrawCalls);
		DebugPageRow("    Statics", "%d", _numSortedStaticsDrawCalls);
		DebugPageRow("    Sprites", "%d", _numSortedSpritesDrawCalls);
		DebugPageRow("Shadow map", "%d", _numShadowMapDrawCalls);
		DebugPageRow("Debris", "%d", _numDebrisDrawCalls);

		DebugPageSection("Geometry");
		DebugPageRow("Triangles", "%d", _numTriangles);
		DebugPageRow("Sprites", "%d", _debugSpriteCount);
		DebugPageRow("Sorted faces", "%d", _debugSortedFaceCount);

		DebugPageSection("Buffers");
		DebugPageRow("Constant buffer updates", "%d (PerDraw: %d)", _numConstantBufferUpdates, _numPerDrawUpdates);
		DebugPageRow("Constant buffer upload", "%.2f MiB", (double)_numConstantBufferBytes / (double)BYTES_PER_MEGABYTE);
		DebugPageRow("Material updates", "%d requested, %d executed", _numRequestedMaterialsUpdates, _numExecutedMaterialsUpdates);

		EndDebugPage();
	}

	void Renderer::DrawMemoryStatsPage()
	{
		if (!BeginDebugPage("Memory Stats"))
			return;

		auto& vram = Graphics::VRAMTracker::Get();
		const auto& adapter = vram.GetAdapterInfo();
		auto toMB = [](size_t bytes) { return bytes / BYTES_PER_MEGABYTE; };

		DebugPageRow("GPU", "%s", adapter.Name.c_str());
		DebugPageRow("Installed VRAM", "%.0f MB", toMB(adapter.DedicatedVideoMemory));
		DebugPageRow("Dedicated system memory", "%.0f MB", toMB(adapter.DedicatedSystemMemory));
		DebugPageRow("Shared system memory", "%.0f MB", toMB(adapter.SharedSystemMemory));

		DebugPageSection("Tracked VRAM usage");
		DebugPageRow("Total", "%.2f MB", toMB(vram.GetTotal()));
		DebugPageRow("    Textures", "%.2f MB", toMB(vram.GetCategory(Graphics::VRAMCategory::Texture)));
		DebugPageRow("    Render targets", "%.2f MB", toMB(vram.GetCategory(Graphics::VRAMCategory::RenderTarget)));
		DebugPageRow("    Vertex buffers", "%.2f MB", toMB(vram.GetCategory(Graphics::VRAMCategory::VertexBuffer)));
		DebugPageRow("    Index buffers", "%.2f MB", toMB(vram.GetCategory(Graphics::VRAMCategory::IndexBuffer)));

		if (adapter.DedicatedVideoMemory > 0)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Usage of installed VRAM");
			ImGui::TableSetColumnIndex(1);
			ImGui::ProgressBar((float)vram.GetTotal() / (float)adapter.DedicatedVideoMemory, ImVec2(160.0f, 0.0f));
		}

		EndDebugPage();
	}

	void Renderer::DrawDimensionStatsPage()
	{
		if (!BeginDebugPage("Dimension Stats"))
			return;

		const auto& playerItem = *LaraItem;
		const auto& player = GetLaraInfo(playerItem);
		const auto& room = g_Level.Rooms[playerItem.RoomNumber];

		DebugPageRow("Position", "%d, %d, %d", playerItem.Pose.Position.x, playerItem.Pose.Position.y, playerItem.Pose.Position.z);
		DebugPageRow("Orientation", "%d, %d, %d", playerItem.Pose.Orientation.x, playerItem.Pose.Orientation.y, playerItem.Pose.Orientation.z);
		DebugPageRow("Scale", "%.3f, %.3f, %.3f", playerItem.Pose.Scale.x, playerItem.Pose.Scale.y, playerItem.Pose.Scale.z);
		DebugPageRow("Room number", "%d", playerItem.RoomNumber);
		DebugPageRow("Pathfinding box ID", "%d", playerItem.BoxNumber);

		if (player.Context.WaterSurfaceDist == -NO_HEIGHT)
			DebugPageRow("Water surface dist", "N/A");
		else
			DebugPageRow("Water surface dist", "%d", player.Context.WaterSurfaceDist);

		DebugPageSection("Room");
		DebugPageRow("Bounds", "%d, %d, %d, %d", room.Position.x, room.Position.z, room.Position.x + BLOCK(room.XSize), room.Position.z + BLOCK(room.ZSize));
		DebugPageRow("Y, min floor, max ceiling", "%d, %d, %d", room.Position.y, room.BottomHeight, room.TopHeight);

		DebugPageSection("Camera");
		DebugPageRow("Position", "%d, %d, %d", Camera.pos.x, Camera.pos.y, Camera.pos.z);
		DebugPageRow("Look at", "%d, %d, %d", Camera.target.x, Camera.target.y, Camera.target.z);
		DebugPageRow("Room number", "%d", Camera.pos.RoomNumber);

		EndDebugPage();
	}

	void Renderer::DrawPlayerStatsPage()
	{
		if (!BeginDebugPage("Player Stats"))
			return;

		const auto& playerItem = *LaraItem;
		const auto& player = GetLaraInfo(playerItem);

		DebugPageRow("Anim object ID", "%d", playerItem.Animation.AnimObjectID);
		DebugPageRow("Anim number", "%d", playerItem.Animation.AnimNumber);
		DebugPageRow("Frame number", "%d", playerItem.Animation.FrameNumber);
		DebugPageRow("Active state", "%d", playerItem.Animation.ActiveState);
		DebugPageRow("Target state", "%d", playerItem.Animation.TargetState);
		DebugPageRow("Velocity", "%.3f, %.3f, %.3f", playerItem.Animation.Velocity.z, playerItem.Animation.Velocity.y, playerItem.Animation.Velocity.x);
		DebugPageRow("Is airborne", "%d", playerItem.Animation.IsAirborne);
		DebugPageRow("Hand status", "%d", player.Control.HandStatus);
		DebugPageRow("Water status", "%d", player.Control.WaterStatus);
		DebugPageRow("Can climb ladder", "%d", player.Control.CanClimbLadder);
		DebugPageRow("Can monkey swing", "%d", player.Control.CanMonkeySwing);
		DebugPageRow("Target hit points", "%d", player.TargetEntity ? player.TargetEntity->HitPoints : 0);

		EndDebugPage();
	}

	void Renderer::DrawInputStatsPage()
	{
		if (!BeginDebugPage("Input Stats"))
			return;

		int	 size			 = (int)ACTION_ID_GROUPS[(int)USER_ACTION_GROUP_IDS.back()].back();
		auto clickedActions	 = BitField(size);
		auto heldActions	 = BitField(size);
		auto releasedActions = BitField(size);

		for (auto actionGroupID : USER_ACTION_GROUP_IDS)
		{
			for (auto actionID : ACTION_ID_GROUPS[(int)actionGroupID])
			{
				const auto& action = ActionMap.at(actionID);

				if (action.IsClicked())
					clickedActions.Set((int)action.GetID());

				if (action.IsHeld())
					heldActions.Set((int)action.GetID());

				if (action.IsReleased())
					releasedActions.Set((int)action.GetID());
			}
		}

		DebugPageRow("Clicked actions", "%s", clickedActions.ToString().c_str());
		DebugPageRow("Held actions", "%s", heldActions.ToString().c_str());
		DebugPageRow("Released actions", "%s", releasedActions.ToString().c_str());
		DebugPageRow("Move axes", "%.3f, %.3f", GetMoveAxis().x, GetMoveAxis().y);
		DebugPageRow("Camera axes", "%.3f, %.3f", GetCameraAxis().x, GetCameraAxis().y);
		DebugPageRow("Mouse axes", "%.3f, %.3f", GetMouseAxis().x, GetMouseAxis().y);
		DebugPageRow("Cursor pos", "%.3f, %.3f", GetMouse2DPosition().x, GetMouse2DPosition().y);

		EndDebugPage();
	}

	void Renderer::DrawCollisionStatsPage()
	{
		if (!BeginDebugPage("Collision Stats"))
			return;

		DebugPageRow("Collision type", "%d", LaraCollision.CollisionType);
		DebugPageRow("Bridge item ID", "%d", LaraCollision.Middle.Bridge);
		DebugPageRow("Front floor", "%d", LaraCollision.Front.Floor);
		DebugPageRow("Front left floor", "%d", LaraCollision.FrontLeft.Floor);
		DebugPageRow("Front right floor", "%d", LaraCollision.FrontRight.Floor);
		DebugPageRow("Front ceiling", "%d", LaraCollision.Front.Ceiling);
		DebugPageRow("Front left ceiling", "%d", LaraCollision.FrontLeft.Ceiling);
		DebugPageRow("Front right ceiling", "%d", LaraCollision.FrontRight.Ceiling);

		EndDebugPage();
	}

	void Renderer::DrawPathfindingStatsPage()
	{
		if (!BeginDebugPage("Pathfinding Stats"))
			return;

		const auto& playerItem = *LaraItem;
		int playerBoxID = (playerItem.BoxNumber == NO_VALUE) ? GetPointCollision(playerItem).GetBottomSector().PathfindingBoxID : playerItem.BoxNumber;

		DebugPageRow("Player box number", "%d", playerBoxID);
		DrawPathfindingEnemyRows(playerBoxID, GetActiveCreatures());

		EndDebugPage();
	}

	void Renderer::DrawPortalStatsPage()
	{
		if (!BeginDebugPage("Portal Stats"))
			return;

		DebugPageRow("Camera room number", "%d", Camera.pos.RoomNumber);
		DebugPageRow("Room collector time", "%d", _timeRoomsCollector);
		DebugPageRow("Rooms", "%d", _debugRoomCount);
		DebugPageRow("    CheckPortal() calls", "%d", _numCheckPortalCalls);
		DebugPageRow("    GetVisibleRooms() calls", "%d", _numGetVisibleRoomsCalls);
		DebugPageRow("    Dot products", "%d", _numDotProducts);

		EndDebugPage();
	}
}
