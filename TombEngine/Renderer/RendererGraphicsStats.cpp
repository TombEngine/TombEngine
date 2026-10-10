#include "framework.h"
#include "Renderer/Renderer.h"

#include <imgui.h>

#include "Game/Sky/SkyCloudSystem.h"
#include "Renderer/Graphics/VRAMTracker.h"
#include "Renderer/Next/NextRenderer.h"
#include "Specific/configuration.h"

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

		AddStatRow("Main window renderer", g_NextRenderer.IsActive() ? "next" : "legacy");
		AddStatRow("Configured renderer", (g_Configuration.GraphicsBackend == GraphicsBackend::Legacy) ? "legacy" :
			((g_Configuration.GraphicsBackend == GraphicsBackend::DX11) ? "next (DirectX 11)" : "next (Vulkan)"));
		AddStatRow("Legacy FPS", "%.1f", _fps);
		AddStatRow("Average frame time", "%.2f ms", averageTime);
		AddStatRow("Worst frame time", "%.2f ms", worstTime);
		AddStatRow("Average next renderer CPU time", "%.3f ms", averageNextTime);
		AddStatRow("Game update time", "%d ms", _timeUpdate);
		AddStatRow("Legacy frame time", "%d ms", _timeFrame);
		AddStatRow("Screen resolution", "%d x %d", _graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
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
	void Renderer::DrawGraphicsStatsLegacy(const RenderView& view)
	{
		if (ImGui::CollapsingHeader("Draw calls", ImGuiTreeNodeFlags_DefaultOpen) && BeginStatTable("##LegacyDraws"))
		{
			AddStatRow("Total", "%d", _numDrawCalls);
			AddStatRow("    Rooms", "%d", _numRoomsDrawCalls);
			AddStatRow("    Moveables", "%d", _numMoveablesDrawCalls);
			AddStatRow("    Statics", "%d", _numStaticsDrawCalls);
			AddStatRow("    Instanced statics", "%d", _numInstancedStaticsDrawCalls);
			AddStatRow("    Sprites", "%d", _numSpritesDrawCalls);
			AddStatRow("    Instanced sprites", "%d", _numInstancedSpritesDrawCalls);
			AddStatRow("    Lines", "%d", _numLinesDrawCalls);
			AddStatRow("    Debris", "%d", _numDebrisDrawCalls);
			AddStatRow("    Effects", "%d", _numEffectsDrawCalls);
			AddStatRow("    Shadow map", "%d", _numShadowMapDrawCalls);
			AddStatRow("Sorted", "%d", _numSortedRoomsDrawCalls + _numSortedMoveablesDrawCalls + _numSortedStaticsDrawCalls + _numSortedSpritesDrawCalls);
			AddStatRow("    Rooms", "%d", _numSortedRoomsDrawCalls);
			AddStatRow("    Moveables", "%d", _numSortedMoveablesDrawCalls);
			AddStatRow("    Statics", "%d", _numSortedStaticsDrawCalls);
			AddStatRow("    Sprites", "%d", _numSortedSpritesDrawCalls);
			ImGui::EndTable();
		}

		if (ImGui::CollapsingHeader("Geometry and state", ImGuiTreeNodeFlags_DefaultOpen) && BeginStatTable("##LegacyGeometry"))
		{
			AddStatRow("Triangles", "%d", _numTriangles);
			AddStatRow("Sorted triangles", "%d", _numSortedTriangles);
			AddStatRow("Rooms drawn", "%d", (int)view.RoomsToDraw.size());
			AddStatRow("Sprites", "%d", (int)view.SpritesToDraw.size());
			AddStatRow("Lights", "%d", (int)view.LightsToDraw.size());
			AddStatRow("Fog bulbs", "%d", (int)view.FogBulbsToDraw.size());
			AddStatRow("Sorted faces", "%d", (int)view.TransparentSortKeys.size());
			AddStatRow("Interleaved sorted groups", "%d", _numInterleavedSortedGroups);
			AddStatRow("Sorted collect", "%.3f ms", _timeSortedCollect);
			AddStatRow("Sorted sort", "%.3f ms", _timeSortedSort);
			AddStatRow("Sorted draw", "%.3f ms", _timeSortedDraw);
			AddStatRow("Constant buffer updates", "%d", _numConstantBufferUpdates);
			AddStatRow("    Per draw", "%d", _numPerDrawUpdates);
			AddStatRow("Constant buffer upload", "%.2f MB", ToMegabytes(_numConstantBufferBytes));
			AddStatRow("Material updates", "%d of %d", _numExecutedMaterialsUpdates, _numRequestedMaterialsUpdates);
			AddStatRow("Portal checks", "%d", _numCheckPortalCalls);
			AddStatRow("Visible room searches", "%d", _numGetVisibleRoomsCalls);
			AddStatRow("Dot products", "%d", _numDotProducts);
			ImGui::EndTable();
		}

		if (ImGui::CollapsingHeader("Video memory", ImGuiTreeNodeFlags_DefaultOpen) && BeginStatTable("##LegacyMemory"))
		{
			auto& vram = Graphics::VRAMTracker::Get();
			const auto& adapter = vram.GetAdapterInfo();
			AddStatRow("Adapter", "%s", adapter.Name.c_str());
			AddStatRow("Installed VRAM", "%.0f MB", ToMegabytes(adapter.DedicatedVideoMemory));
			AddStatRow("Dedicated system memory", "%.0f MB", ToMegabytes(adapter.DedicatedSystemMemory));
			AddStatRow("Shared system memory", "%.0f MB", ToMegabytes(adapter.SharedSystemMemory));
			AddStatRow("Tracked VRAM", "%.2f MB", ToMegabytes(vram.GetTotal()));
			AddStatRow("    Textures", "%.2f MB", ToMegabytes(vram.GetCategory(Graphics::VRAMCategory::Texture)));
			AddStatRow("    Render targets", "%.2f MB", ToMegabytes(vram.GetCategory(Graphics::VRAMCategory::RenderTarget)));
			AddStatRow("    Vertex buffers", "%.2f MB", ToMegabytes(vram.GetCategory(Graphics::VRAMCategory::VertexBuffer)));
			AddStatRow("    Index buffers", "%.2f MB", ToMegabytes(vram.GetCategory(Graphics::VRAMCategory::IndexBuffer)));
			ImGui::EndTable();
		}
	}

	// ImGui window of the graphics statistics debug page.
	void Renderer::DrawGraphicsStatsWindow(const RenderView& view)
	{
		// The history keeps running while other tabs are shown.
		_statsFrameTimes[_statsHistoryIndex] = ImGui::GetIO().DeltaTime * 1000.0f;
		_statsNextCpuTimes[_statsHistoryIndex] = g_NextRenderer.IsActive() ? g_NextRenderer.GetFrameStats().CpuTime : 0.0f;
		_statsHistoryIndex = (_statsHistoryIndex + 1) % GRAPHICS_STATS_HISTORY;

		ImGui::SetNextWindowSize(ImVec2(460.0f, 640.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Graphics statistics", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		if (ImGui::BeginTabBar("##GraphicsStatsTabs"))
		{
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

			if (ImGui::BeginTabItem("Sky"))
			{
				DrawGraphicsStatsSky();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Legacy renderer"))
			{
				DrawGraphicsStatsLegacy(view);
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}
}
