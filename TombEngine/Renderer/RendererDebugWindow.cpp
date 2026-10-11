#include "framework.h"
#include "Renderer/Renderer.h"

#include <imgui.h>

#include "Game/camera.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Sky/SkyCloudDebug.h"
#include "Game/Sky/SkyCloudSystem.h"
#include "Renderer/ImGuiIntegration.h"
#include "Renderer/Next/NextRenderer.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"
#include "Specific/Input/InputAction.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Input;
using namespace TEN::Renderer::Next;

namespace TEN::Renderer
{
	static void AddStatRow(const char* name, const char* format, ...)
	{
		char buffer[128];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		va_end(args);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(name);
		ImGui::TableSetColumnIndex(1);
		ImGui::TextUnformatted(buffer);
	}

	static bool BeginStatTable(const char* id)
	{
		if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
			return false;

		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.6f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.4f);
		return true;
	}

	static float ToMegabytes(unsigned long long bytes)
	{
		return bytes / (1024.0f * 1024.0f);
	}

	static const char* YesNo(bool value)
	{
		return value ? "yes" : "no";
	}

	// Frame time history of the last seconds, with average and worst frame.
	void Renderer::DrawGraphicsStatsOverview()
	{
		float frameTime = ImGui::GetIO().DeltaTime * 1000.0f;

		float averageTime = 0.0f;
		float worstTime = 0.0f;
		float averageNextTime = 0.0f;
		for (int i = 0; i < GRAPHICS_STATS_HISTORY; i++)
		{
			averageTime += _statsFrameTimes[i];
			worstTime = std::max(worstTime, _statsFrameTimes[i]);
			averageNextTime += _statsNextCpuTimes[i];
		}

		averageTime /= GRAPHICS_STATS_HISTORY;
		averageNextTime /= GRAPHICS_STATS_HISTORY;

		// Graphs scale with the average, so that loading hitches do not flatten them.
		float frameScale = std::max(averageTime * 2.0f, 1.0f);
		float nextScale = std::max(averageNextTime * 2.0f, 0.1f);

		auto overlay = fmt::format("{:.2f} ms ({:.0f} FPS)", frameTime, (frameTime > 0.0f) ? 1000.0f / frameTime : 0.0f);
		ImGui::PlotLines("Frame time", _statsFrameTimes.data(), GRAPHICS_STATS_HISTORY, _statsHistoryIndex, overlay.c_str(), 0.0f, frameScale, ImVec2(0.0f, 60.0f));
		if (g_NextRenderer.IsActive())
		{
			auto nextOverlay = fmt::format("{:.3f} ms", g_NextRenderer.GetFrameStats().CpuTime);
			ImGui::PlotLines("Next CPU", _statsNextCpuTimes.data(), GRAPHICS_STATS_HISTORY, _statsHistoryIndex, nextOverlay.c_str(), 0.0f, nextScale, ImVec2(0.0f, 40.0f));
		}

		if (!BeginStatTable("##Overview"))
			return;

		AddStatRow("Configured backend", (g_Configuration.GraphicsBackend == GraphicsBackend::DX11) ? "DirectX 11" : "Vulkan");
		AddStatRow("FPS", "%.1f", _fps);
		AddStatRow("Average frame time", "%.2f ms", averageTime);
		AddStatRow("Worst frame time", "%.2f ms", worstTime);
		AddStatRow("Average next renderer CPU time", "%.3f ms", averageNextTime);
		AddStatRow("Game update time", "%d ms", _timeUpdate);
		AddStatRow("Control phase time", "%d", ControlPhaseTime);
		AddStatRow("Frontend frame time", "%d ms", _timeFrame);
		AddStatRow("Screen resolution", "%d x %d", _screenWidth, _screenHeight);
		AddStatRow("Windowed", YesNo(g_Configuration.EnableWindowedMode));
		constexpr const char* ANTIALIASING_NAMES[] = { "none", "low (FXAA)", "medium (SMAA)", "high (SMAA)" };
		constexpr const char* SHADOW_NAMES[] = { "none", "player", "all" };
		constexpr const char* QUALITY_NAMES[] = { "low", "medium", "high" };
		AddStatRow("Antialiasing", "%s", ANTIALIASING_NAMES[std::clamp((int)g_Configuration.AntialiasingMode, 0, 3)]);
		AddStatRow("Shadows", "%s", SHADOW_NAMES[std::clamp((int)g_Configuration.ShadowType, 0, 2)]);
		AddStatRow("Shadow map size", "%d", g_Configuration.ShadowMapSize);
		AddStatRow("Ambient occlusion", YesNo(g_Configuration.EnableAmbientOcclusion));
		AddStatRow("Caustics", YesNo(g_Configuration.EnableCaustics));
		AddStatRow("Decals", YesNo(g_Configuration.EnableDecals));
		AddStatRow("High framerate", YesNo(g_Configuration.EnableHighFramerate));
		AddStatRow("Atmospheric sky quality", "%s", QUALITY_NAMES[std::clamp((int)g_Configuration.AtmosphericSkyQuality, 0, 2)]);
		AddStatRow("Gamma", "%.2f", g_Configuration.Gamma);
		ImGui::EndTable();
	}

	// Times, passes and per frame data of the next renderer.
	void Renderer::DrawGraphicsStatsNext()
	{
		if (!g_NextRenderer.IsActive())
		{
			ImGui::TextUnformatted("The next renderer is not running.");
			return;
		}

		const auto& stats = g_NextRenderer.GetFrameStats();
		auto info = g_NextRenderer.GetInfo();

		if (ImGui::CollapsingHeader("Device", ImGuiTreeNodeFlags_DefaultOpen) && BeginStatTable("##NextDevice"))
		{
			AddStatRow("Backend", "%s", info.Backend.c_str());
			AddStatRow("Device", "%s", info.DeviceName.c_str());
			AddStatRow("Target size", "%d x %d", info.Width, info.Height);
			ImGui::EndTable();
		}

		if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen) && BeginStatTable("##NextFrame"))
		{
			AddStatRow("CPU time", "%.3f ms", stats.CpuTime);
			AddStatRow("    Collect", "%.3f ms", stats.CollectTime);
			AddStatRow("    Upload", "%.3f ms", stats.UploadTime);
			AddStatRow("Draw calls", "%d", stats.DrawCalls);
			AddStatRow("Uploaded data", "%.1f KB", stats.UploadBytes / 1024.0f);
			ImGui::EndTable();
		}

		if (ImGui::CollapsingHeader("Passes", ImGuiTreeNodeFlags_DefaultOpen) &&
			ImGui::BeginTable("##NextPasses", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Pass");
			ImGui::TableSetupColumn("CPU ms");
			ImGui::TableSetupColumn("Draws");
			ImGui::TableHeadersRow();
			for (const auto& pass : stats.Passes)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(pass.Name);
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.3f", pass.CpuTime);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%d", pass.DrawCalls);
			}

			ImGui::EndTable();
		}

		if (ImGui::CollapsingHeader("GPU time per pass"))
		{
			const auto& timings = g_NextRenderer.SampleGpuTimings();
			if (timings.empty())
				ImGui::TextUnformatted("Not reported by this backend.");

			if (!timings.empty() && BeginStatTable("##NextGpu"))
			{
				for (const auto& timing : timings)
					AddStatRow(timing.Name.c_str(), "%.3f ms", timing.Milliseconds);

				ImGui::EndTable();
			}
		}

		if (ImGui::CollapsingHeader("Draw calls", ImGuiTreeNodeFlags_DefaultOpen) &&
			ImGui::BeginTable("##NextCategories", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Content");
			ImGui::TableSetupColumn("Draws");
			ImGui::TableSetupColumn("Triangles");
			ImGui::TableHeadersRow();
			for (int category = 0; category < (int)DrawCategory::Count; category++)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(GetDrawCategoryName((DrawCategory)category));
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%d", stats.CategoryDrawCalls[category]);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%d", stats.CategoryTriangles[category]);
			}

			ImGui::EndTable();
			if (BeginStatTable("##NextInstancing"))
			{
				AddStatRow("Instanced object draws", "%d", stats.InstancedDrawCalls);
				AddStatRow("Drawn object instances", "%d", stats.DrawnInstances);
				ImGui::EndTable();
			}
		}

		if (ImGui::CollapsingHeader("API calls", ImGuiTreeNodeFlags_DefaultOpen) && BeginStatTable("##NextApi"))
		{
			const auto& api = stats.Api;
			AddStatRow("Render passes", "%d", api.RenderPasses);
			AddStatRow("Copy passes", "%d", api.CopyPasses);
			AddStatRow("Compute passes", "%d", api.ComputePasses);
			AddStatRow("Pipeline binds", "%d", api.PipelineBinds);
			AddStatRow("Vertex buffer binds", "%d", api.VertexBufferBinds);
			AddStatRow("Index buffer binds", "%d", api.IndexBufferBinds);
			AddStatRow("Texture binds", "%d", api.TextureBinds);
			AddStatRow("Storage buffer binds", "%d", api.StorageBufferBinds);
			AddStatRow("Uniform pushes", "%d", api.UniformPushes);
			AddStatRow("Uniform data", "%.1f KB", api.UniformBytes / 1024.0f);
			AddStatRow("Draws", "%d", api.Draws);
			AddStatRow("    Indexed", "%d", api.IndexedDraws);
			AddStatRow("Instances", "%lld", api.Instances);
			AddStatRow("Triangles", "%lld", api.Triangles);
			AddStatRow("Lines", "%lld", api.Lines);
			AddStatRow("Dispatches", "%d", api.Dispatches);
			AddStatRow("Blits", "%d", api.Blits);
			AddStatRow("Uploads", "%d", api.Uploads);
			AddStatRow("Uploaded buffer data", "%.1f KB", api.UploadBytes / 1024.0f);
			ImGui::EndTable();
		}

		if (ImGui::CollapsingHeader("Sorted faces", ImGuiTreeNodeFlags_DefaultOpen) && BeginStatTable("##NextSorted"))
		{
			AddStatRow("Sorted draws", "%d", stats.SortedDraws);
			AddStatRow("Sorted indices", "%d", stats.SortedIndices);
			AddStatRow("Collect and sort", "%.3f ms", stats.SortedCollectTime);
			AddStatRow("Record", "%.3f ms", stats.SortedRecordTime);
			AddStatRow("Material uploads", "%d", stats.MaterialUploads);
			ImGui::EndTable();
		}

		if (ImGui::CollapsingHeader("Scene data", ImGuiTreeNodeFlags_DefaultOpen) && BeginStatTable("##NextScene"))
		{
			AddStatRow("Rooms drawn", "%d", stats.RoomsDrawn);
			AddStatRow("Room draws", "%d", stats.RoomDraws);
			AddStatRow("Object draws", "%d", stats.ObjectDraws);
			AddStatRow("Shadow draws", "%d", stats.ShadowDraws);
			AddStatRow("Instances", "%d", stats.Instances);
			AddStatRow("Bones", "%d", stats.Bones);
			AddStatRow("Lights", "%d", stats.Lights);
			AddStatRow("Decals", "%d", stats.Decals);
			AddStatRow("Materials", "%d", stats.Materials);
			AddStatRow("Sprites", "%d", stats.Sprites);
			AddStatRow("Sprite runs", "%d", stats.SpriteRuns);
			AddStatRow("Sky sprites", "%d", stats.SkySprites);
			AddStatRow("Sorted draws", "%d", stats.SortedDraws);
			AddStatRow("Sorted indices", "%d", stats.SortedIndices);
			AddStatRow("Debris triangles", "%d", stats.DebrisTriangles);
			AddStatRow("Mirrors", "%d", stats.Mirrors);
			AddStatRow("Gun flashes", "%d", stats.GunFlashes);
			AddStatRow("Fog bulbs", "%d", stats.FogBulbs);
			AddStatRow("Cloud layer draws", "%d", stats.CloudDraws);
			AddStatRow("Debug vertices", "%d", stats.DebugVertices);
			ImGui::EndTable();
		}

		if (ImGui::CollapsingHeader("Resources", ImGuiTreeNodeFlags_DefaultOpen) && BeginStatTable("##NextResources"))
		{
			AddStatRow("Room vertices", "%d", info.RoomVertices);
			AddStatRow("Object vertices", "%d", info.ObjectVertices);
			AddStatRow("Texture array groups", "%d", info.TextureArrayGroups);
			AddStatRow("Texture pages", "%d", info.TexturePages);
			AddStatRow("Scene pipelines", "%d", info.ScenePipelines);
			AddStatRow("Pooled targets", "%d", info.PooledTargets);
			AddStatRow("Upload ring", "%.1f KB", info.UploadRingSize / 1024.0f);
			ImGui::EndTable();
		}
	}

	// Sky passes of the frame, as recorded by the legacy renderer for both renderers.
	void Renderer::DrawGraphicsStatsSky()
	{
		if (!BeginStatTable("##Sky"))
			return;

		int cloudRaymarches = 0;
		for (const auto& draw : _nextCloudDraws)
			cloudRaymarches += (draw.IsRaymarched && !draw.IsCleared) ? 1 : 0;

		AddStatRow("Weather preset", "%s", TEN::Sky::SkyCloudSystem::PresetTypeToString(TEN::Sky::g_SkyCloudSystem.GetCurrentPreset()));
		AddStatRow("Atmospheric dome", YesNo(_nextSkyEffects.IsDomeDrawn));
		AddStatRow("Sun and moon discs", YesNo(_nextSkyEffects.IsSunMoonDiscDrawn));
		AddStatRow("Aurora", _nextSkyEffects.IsAuroraDrawn ? (_nextSkyEffects.IsAuroraHalfResolution ? "half resolution" : "full resolution") : "no");
		AddStatRow("Cloud layer A", YesNo(TEN::Sky::g_SkyCloudSystem.IsCloudAActive()));
		AddStatRow("Cloud layer B", YesNo(TEN::Sky::g_SkyCloudSystem.IsCloudBActive()));
		AddStatRow("Cloud draws", "%d", (int)_nextCloudDraws.size());
		AddStatRow("Cloud raymarches", "%d", cloudRaymarches);
		AddStatRow("God rays", YesNo(_nextGodRays.IsDrawn));
		AddStatRow("Dust storm", YesNo(_nextDustStorm.IsDrawn));
		for (int layer = 0; layer < (int)_nextHorizonLayers.size(); layer++)
		{
			const auto& horizon = _nextHorizonLayers[layer];
			AddStatRow(layer == 0 ? "Horizon 1" : "Horizon 2", horizon.IsDrawn ? "alpha %.2f, fade %.2f, rise %.2f" : "no", horizon.Alpha, horizon.GradientFade, horizon.GradientRise);
		}

		AddStatRow("Stars and meteors", "%d", (int)_nextSkySprites.size());
		ImGui::EndTable();
	}

	// Draw calls, triangles and memory of the legacy renderer, which still runs behind the next renderer.
	// Scene collection of the renderer frontend: visible rooms, lights and sorted faces.
	void Renderer::DrawGraphicsStatsFrontend()
	{
		if (!BeginStatTable("##Frontend"))
			return;

		AddStatRow("Rooms drawn", "%d", _debugViewCounts.Rooms);
		AddStatRow("Sprites", "%d", _debugViewCounts.Sprites);
		AddStatRow("Lights", "%d", _debugViewCounts.Lights);
		AddStatRow("Fog bulbs", "%d", _debugViewCounts.FogBulbs);
		AddStatRow("Sorted faces", "%d", _debugViewCounts.SortedFaces);
		AddStatRow("Interleaved sorted groups", "%d", _numInterleavedSortedGroups);
		AddStatRow("Sorted collect", "%.3f ms", _timeSortedCollect);
		AddStatRow("Sorted sort", "%.3f ms", _timeSortedSort);
		AddStatRow("Portal checks", "%d", _numCheckPortalCalls);
		AddStatRow("Visible room searches", "%d", _numGetVisibleRoomsCalls);
		AddStatRow("Dot products", "%d", _numDotProducts);
		ImGui::EndTable();
	}

	void Renderer::DrawDebugRendererPage(const RenderView& view)
	{
		if (!ImGui::BeginTabBar("##RendererTabs"))
			return;

		if (ImGui::BeginTabItem("Overview"))
		{
			DrawGraphicsStatsOverview();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Next renderer"))
		{
			DrawGraphicsStatsNext();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Sky effects"))
		{
			DrawGraphicsStatsSky();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Frontend"))
		{
			DrawGraphicsStatsFrontend();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	void Renderer::DrawDebugDimensionPage(const RenderView& view)
	{
		const auto& playerItem = *LaraItem;
		const auto& player = GetLaraInfo(playerItem);
		const auto& room = g_Level.Rooms[playerItem.RoomNumber];
		if (!BeginStatTable("##Dimensions"))
			return;

		AddStatRow("Position", "%d, %d, %d", playerItem.Pose.Position.x, playerItem.Pose.Position.y, playerItem.Pose.Position.z);
		AddStatRow("Orientation", "%d, %d, %d", playerItem.Pose.Orientation.x, playerItem.Pose.Orientation.y, playerItem.Pose.Orientation.z);
		AddStatRow("Scale", "%.3f, %.3f, %.3f", playerItem.Pose.Scale.x, playerItem.Pose.Scale.y, playerItem.Pose.Scale.z);
		AddStatRow("Room number", "%d", playerItem.RoomNumber);
		AddStatRow("Pathfinding box", "%d", playerItem.BoxNumber);
		if (player.Context.WaterSurfaceDist == -NO_HEIGHT)
			AddStatRow("Water surface distance", "N/A");
		else
			AddStatRow("Water surface distance", "%d", player.Context.WaterSurfaceDist);

		AddStatRow("Room bounds", "%d, %d, %d, %d", room.Position.x, room.Position.z, room.Position.x + BLOCK(room.XSize), room.Position.z + BLOCK(room.ZSize));
		AddStatRow("Room Y, bottom, top", "%d, %d, %d", room.Position.y, room.BottomHeight, room.TopHeight);
		AddStatRow("Camera position", "%d, %d, %d", Camera.pos.x, Camera.pos.y, Camera.pos.z);
		AddStatRow("Camera target", "%d, %d, %d", Camera.target.x, Camera.target.y, Camera.target.z);
		AddStatRow("Camera room number", "%d", Camera.pos.RoomNumber);
		ImGui::EndTable();
	}

	void Renderer::DrawDebugPlayerPage(const RenderView& view)
	{
		const auto& playerItem = *LaraItem;
		const auto& player = GetLaraInfo(playerItem);
		if (!BeginStatTable("##Player"))
			return;

		AddStatRow("Animation object", "%d", playerItem.Animation.AnimObjectID);
		AddStatRow("Animation number", "%d", playerItem.Animation.AnimNumber);
		AddStatRow("Frame number", "%d", playerItem.Animation.FrameNumber);
		AddStatRow("Active state", "%d", playerItem.Animation.ActiveState);
		AddStatRow("Target state", "%d", playerItem.Animation.TargetState);
		AddStatRow("Velocity", "%.3f, %.3f, %.3f", playerItem.Animation.Velocity.z, playerItem.Animation.Velocity.y, playerItem.Animation.Velocity.x);
		AddStatRow("Airborne", "%s", YesNo(playerItem.Animation.IsAirborne));
		AddStatRow("Hand status", "%d", (int)player.Control.HandStatus);
		AddStatRow("Water status", "%d", (int)player.Control.WaterStatus);
		AddStatRow("Can climb ladder", "%s", YesNo(player.Control.CanClimbLadder));
		AddStatRow("Can monkey swing", "%s", YesNo(player.Control.CanMonkeySwing));
		AddStatRow("Hit points", "%d", playerItem.HitPoints);
		AddStatRow("Target hit points", "%d", (player.TargetEntity != nullptr) ? player.TargetEntity->HitPoints : 0);
		ImGui::EndTable();
	}

	void Renderer::DrawDebugInputPage(const RenderView& view)
	{
		int size = (int)ACTION_ID_GROUPS[(int)USER_ACTION_GROUP_IDS.back()].back();
		auto clickedActions = BitField(size);
		auto heldActions = BitField(size);
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

		if (!BeginStatTable("##Input"))
			return;

		AddStatRow("Clicked actions", "%s", clickedActions.ToString().c_str());
		AddStatRow("Held actions", "%s", heldActions.ToString().c_str());
		AddStatRow("Released actions", "%s", releasedActions.ToString().c_str());
		AddStatRow("Move axes", "%.3f, %.3f", GetMoveAxis().x, GetMoveAxis().y);
		AddStatRow("Camera axes", "%.3f, %.3f", GetCameraAxis().x, GetCameraAxis().y);
		AddStatRow("Mouse axes", "%.3f, %.3f", GetMouseAxis().x, GetMouseAxis().y);
		AddStatRow("Cursor position", "%.3f, %.3f", GetMouse2DPosition().x, GetMouse2DPosition().y);
		ImGui::EndTable();
	}

	void Renderer::DrawDebugCollisionPage(const RenderView& view)
	{
		if (!BeginStatTable("##Collision"))
			return;

		AddStatRow("Collision type", "%d", (int)LaraCollision.CollisionType);
		AddStatRow("Bridge item", "%d", LaraCollision.Middle.Bridge);
		AddStatRow("Front floor", "%d", LaraCollision.Front.Floor);
		AddStatRow("Front left floor", "%d", LaraCollision.FrontLeft.Floor);
		AddStatRow("Front right floor", "%d", LaraCollision.FrontRight.Floor);
		AddStatRow("Front ceiling", "%d", LaraCollision.Front.Ceiling);
		AddStatRow("Front left ceiling", "%d", LaraCollision.FrontLeft.Ceiling);
		AddStatRow("Front right ceiling", "%d", LaraCollision.FrontRight.Ceiling);
		ImGui::EndTable();
	}

	void Renderer::DrawDebugCollisionMeshPage(const RenderView& view)
	{
		ImGui::TextWrapped("Room collision meshes are drawn in the scene.");
	}

	void Renderer::DrawDebugPortalPage(const RenderView& view)
	{
		if (!BeginStatTable("##Portals"))
			return;

		AddStatRow("Camera room number", "%d", Camera.pos.RoomNumber);
		AddStatRow("Room collector time", "%d", _timeRoomsCollector);
		AddStatRow("Rooms drawn", "%d", _debugViewCounts.Rooms);
		AddStatRow("Portal checks", "%d", _numCheckPortalCalls);
		AddStatRow("Visible room searches", "%d", _numGetVisibleRoomsCalls);
		AddStatRow("Dot products", "%d", _numDotProducts);
		ImGui::EndTable();
	}

	void Renderer::DrawDebugPathfindingPage(const RenderView& view)
	{
		const auto& playerItem = *LaraItem;
		int playerBoxID = (playerItem.BoxNumber == NO_VALUE) ? GetPointCollision(playerItem).GetBottomSector().PathfindingBoxID : playerItem.BoxNumber;
		auto creatures = GetActiveCreatures();
		if (!BeginStatTable("##Pathfinding"))
			return;

		AddStatRow("Player box", "%d", playerBoxID);
		AddStatRow("Active creatures", "%d", (int)creatures.size());
		if (PathfindingDisplayIndex >= 0 && PathfindingDisplayIndex < (int)creatures.size())
		{
			auto& enemy = g_Level.Items[creatures[PathfindingDisplayIndex]];
			const auto* creature = (CreatureInfo*)enemy.Data;
			auto zoneType = creature->LOT.Zone;
			const auto& zones = g_Level.Zones[(int)zoneType][(int)FlipStatus];

			auto mood = "Unknown";
			switch (creature->Mood)
			{
			case MoodType::Attack: mood = "Attack"; break;
			case MoodType::Stalk:  mood = "Stalk";  break;
			case MoodType::Escape: mood = "Escape"; break;
			case MoodType::Bored:  mood = "Bored";  break;
			}

			AddStatRow("Player zone", "%d", (playerBoxID == NO_VALUE) ? NO_VALUE : zones[playerBoxID]);
			AddStatRow("Enemy", "%s", enemy.Name.c_str());
			AddStatRow("Enemy box", "%d", enemy.BoxNumber);
			AddStatRow("Enemy zone type", "%d", (int)zoneType);
			AddStatRow("Enemy zone", "%d", (enemy.BoxNumber == NO_VALUE) ? NO_VALUE : zones[enemy.BoxNumber]);
			AddStatRow("Enemy mood", "%s", mood);
		}

		ImGui::EndTable();
		if (PathfindingDisplayIndex < 0 && !creatures.empty())
			ImGui::TextUnformatted("Push TAB to scroll through enemies.");
	}

	void Renderer::DrawDebugWireframePage(const RenderView& view)
	{
		ImGui::TextWrapped("The scene is drawn in wireframe.");
	}

	void Renderer::DrawDebugSkyPage(const RenderView& view)
	{
		TEN::Sky::DrawSkyDebugContents();
	}

	// Lines printed by the game code for the current page during the last game update.
	// Panels of the debug overlay sit along the screen edges with translucent backgrounds, so the scene stays visible.
	static bool BeginOverlayPanel(const char* name, const ImVec2& position, const ImVec2& pivot, const ImVec2& minSize, const ImVec2& maxSize, bool* isOpen = nullptr)
	{
		constexpr auto PANEL_BACKGROUND_ALPHA = 0.55f;

		ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
		ImGui::SetNextWindowSizeConstraints(minSize, maxSize);
		ImGui::SetNextWindowBgAlpha(PANEL_BACKGROUND_ALPHA);

		auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize;
		if (ImGui::Begin(name, isOpen, flags))
			return true;

		ImGui::End();
		return false;
	}

	// Summary of the frame: rate, history, CPU and GPU times and the most expensive GPU passes.
	void Renderer::DrawPerformancePanel()
	{
		constexpr auto GPU_PASSES_SHOWN = 8;

		float scale = ImGuiGetUiScale();
		float margin = 8.0f * scale;
		float width = 330.0f * scale;
		const auto& display = ImGui::GetIO().DisplaySize;
		if (!BeginOverlayPanel("Performance (F8)", ImVec2(margin, margin), ImVec2(0.0f, 0.0f), ImVec2(width, 0.0f), ImVec2(width, display.y * 0.6f)))
			return;

		float averageTime = 0.0f;
		float worstTime = 0.0f;
		for (float time : _statsFrameTimes)
		{
			averageTime += time;
			worstTime = std::max(worstTime, time);
		}

		averageTime /= GRAPHICS_STATS_HISTORY;

		ImGui::Text("%.0f FPS   %.2f ms", (averageTime > 0.0f) ? (1000.0f / averageTime) : 0.0f, averageTime);
		auto overlay = fmt::format("worst {:.1f} ms", worstTime);
		ImGui::PlotLines("##FrameTimes", _statsFrameTimes.data(), GRAPHICS_STATS_HISTORY, _statsHistoryIndex, overlay.c_str(), 0.0f,
			std::max(worstTime, 33.4f), ImVec2(-1.0f, 48.0f * scale));

		const auto& stats = g_NextRenderer.GetFrameStats();
		const auto& gpuTimings = g_NextRenderer.SampleGpuTimings();
		float gpuTime = 0.0f;
		for (const auto& timing : gpuTimings)
			gpuTime += timing.Milliseconds;

		int triangles = 0;
		for (int categoryTriangles : stats.CategoryTriangles)
			triangles += categoryTriangles;

		if (BeginStatTable("##Performance"))
		{
			AddStatRow("Frontend CPU", "%d ms", _timeFrame + _timeUpdate + _timeRoomsCollector);
			AddStatRow("Renderer CPU", "%.2f ms", stats.CpuTime);
			if (gpuTimings.empty())
				AddStatRow("GPU", "n/a");
			else
				AddStatRow("GPU", "%.2f ms", gpuTime);

			AddStatRow("Draw calls", "%d", stats.DrawCalls);
			AddStatRow("Triangles", "%d", triangles);
			AddStatRow("Rooms", "%d", stats.RoomsDrawn);
			AddStatRow("Resolution", "%d x %d", _screenWidth, _screenHeight);
			AddStatRow("Device", "%s", g_NextRenderer.GetInfo().DeviceName.c_str());
			ImGui::EndTable();
		}

		if (!gpuTimings.empty() && ImGui::CollapsingHeader("GPU passes", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto sortedTimings = gpuTimings;
			std::sort(sortedTimings.begin(), sortedTimings.end(), [](const auto& a, const auto& b) { return (a.Milliseconds > b.Milliseconds); });

			int count = std::min((int)sortedTimings.size(), GPU_PASSES_SHOWN);
			for (int i = 0; i < count; i++)
			{
				const auto& timing = sortedTimings[i];
				auto label = fmt::format("{}  {:.2f} ms", timing.Name, timing.Milliseconds);
				ImGui::ProgressBar((gpuTime > 0.0f) ? (timing.Milliseconds / gpuTime) : 0.0f, ImVec2(-1.0f, 0.0f), label.c_str());
			}
		}

		ImGui::End();
	}

	// Debug messages of the game code, along the bottom edge.
	void Renderer::DrawDebugMessagePanel()
	{
		if (_debugMessages.empty())
			return;

		float scale = ImGuiGetUiScale();
		float margin = 8.0f * scale;
		const auto& display = ImGui::GetIO().DisplaySize;
		if (!BeginOverlayPanel("Messages", ImVec2(margin, display.y - margin), ImVec2(0.0f, 1.0f), ImVec2(420.0f * scale, 0.0f), ImVec2(display.x * 0.5f, display.y * 0.35f)))
			return;

		for (const auto& message : _debugMessages)
			ImGui::TextUnformatted(message.c_str());

		ImGui::End();
	}

	// Selected page of a group: its tab becomes the debug page, so the scene debug drawing follows it.
	void Renderer::DrawDebugPageGroup(const char* group, const std::vector<DebugPageTab>& tabs, const RenderView& view, bool isSelectionPending)
	{
		int pageCount = 0;
		for (const auto& tab : tabs)
			pageCount += (strcmp(tab.Group, group) == 0) ? 1 : 0;

		bool hasTabBar = (pageCount > 1) && ImGui::BeginTabBar(group);
		for (const auto& tab : tabs)
		{
			if (strcmp(tab.Group, group) != 0)
				continue;

			if (hasTabBar)
			{
				auto flags = (isSelectionPending && tab.Page == _debugPage) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
				if (!ImGui::BeginTabItem(tab.Name, nullptr, flags))
					continue;
			}

			if (!isSelectionPending)
				_debugPage = tab.Page;

			(this->*tab.Draw)(view);
			if (hasTabBar)
				ImGui::EndTabItem();
		}

		if (hasTabBar)
			ImGui::EndTabBar();
	}

	// Debug pages in a column along the right edge, grouped by topic in tabs.
	void Renderer::DrawDebugPagePanel(const RenderView& view)
	{
		// Debug pages grouped by topic; the debug keys cycle through them in enum order.
		static const auto DEBUG_PAGE_TABS = std::vector<DebugPageTab>
		{
			{ RendererDebugPage::RendererStats, "Renderer", "Statistics", &Renderer::DrawDebugRendererPage },
			{ RendererDebugPage::PortalStats, "Renderer", "Portals", &Renderer::DrawDebugPortalPage },
			{ RendererDebugPage::WireframeMode, "Renderer", "Wireframe", &Renderer::DrawDebugWireframePage },
			{ RendererDebugPage::PlayerStats, "Player", "Player", &Renderer::DrawDebugPlayerPage },
			{ RendererDebugPage::DimensionStats, "Player", "Dimensions", &Renderer::DrawDebugDimensionPage },
			{ RendererDebugPage::InputStats, "Player", "Input", &Renderer::DrawDebugInputPage },
			{ RendererDebugPage::CollisionStats, "Collision", "Collision", &Renderer::DrawDebugCollisionPage },
			{ RendererDebugPage::CollisionMeshStats, "Collision", "Collision mesh", &Renderer::DrawDebugCollisionMeshPage },
			{ RendererDebugPage::PathfindingStats, "Collision", "Pathfinding", &Renderer::DrawDebugPathfindingPage },
			{ RendererDebugPage::SkyDebug, "Sky", "Sky", &Renderer::DrawDebugSkyPage }
		};

		constexpr const char* DEBUG_PAGE_GROUPS[] = { "Renderer", "Player", "Collision", "Sky" };

		float scale = ImGuiGetUiScale();
		float margin = 8.0f * scale;
		const auto& display = ImGui::GetIO().DisplaySize;
		float width = std::min(460.0f * scale, display.x * 0.4f);
		float height = display.y - margin * 2.0f;

		bool isOpen = true;
		if (!BeginOverlayPanel("Debug pages (F10, F11)", ImVec2(display.x - margin, margin), ImVec2(1.0f, 0.0f), ImVec2(width, 0.0f), ImVec2(width, height), &isOpen))
		{
			if (!isOpen)
				_debugPage = RendererDebugPage::None;

			return;
		}

		if (ImGui::BeginTabBar("##DebugGroups"))
		{
			// A page selected with the debug keys takes a frame to become the selected tab.
			bool isSelectionPending = (_debugPageSelectionFrames > 0);
			for (const char* group : DEBUG_PAGE_GROUPS)
			{
				bool hasSelectedPage = false;
				for (const auto& tab : DEBUG_PAGE_TABS)
					hasSelectedPage |= (strcmp(tab.Group, group) == 0 && tab.Page == _debugPage);

				auto flags = (isSelectionPending && hasSelectedPage) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
				if (!ImGui::BeginTabItem(group, nullptr, flags))
					continue;

				DrawDebugPageGroup(group, DEBUG_PAGE_TABS, view, isSelectionPending);
				ImGui::EndTabItem();
			}

			_debugPageSelectionFrames = std::max(_debugPageSelectionFrames - 1, 0);
			ImGui::EndTabBar();
		}

		ImGui::End();

		if (!isOpen)
			_debugPage = RendererDebugPage::None;
	}

	// Debug overlay: the performance panel, then the debug pages and messages while a page is active.
	void Renderer::DrawDebugOverlay(const RenderView& view)
	{
		// The frame history keeps running while other pages are shown.
		_statsFrameTimes[_statsHistoryIndex] = ImGui::GetIO().DeltaTime * 1000.0f;
		_statsNextCpuTimes[_statsHistoryIndex] = g_NextRenderer.IsActive() ? g_NextRenderer.GetFrameStats().CpuTime : 0.0f;
		_statsHistoryIndex = (_statsHistoryIndex + 1) % GRAPHICS_STATS_HISTORY;

		if (_isPerformanceOverlayVisible)
			DrawPerformancePanel();

		if (_debugPage == RendererDebugPage::None)
			return;

		DrawDebugPagePanel(view);
		DrawDebugMessagePanel();
	}

	void Renderer::TogglePerformanceOverlay()
	{
		_isPerformanceOverlayVisible = !_isPerformanceOverlayVisible;
	}

	bool Renderer::IsDebugOverlayVisible() const
	{
		// Benchmark screenshots show the scene alone.
		if (!DebugMode || TEN::Benchmark::IsBenchmarkActive())
			return false;

		return (_isPerformanceOverlayVisible || _debugPage != RendererDebugPage::None);
	}
}
