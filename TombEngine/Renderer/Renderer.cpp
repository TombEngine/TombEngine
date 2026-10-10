#include "framework.h"
#include "Renderer/Renderer.h"
#include "Renderer/ImGuiIntegration.h"
#include "Renderer/Next/NextRenderer.h"
#include "Game/camera.h"
#include "Game/effects/tomb4fx.h"
#include "Math/Math.h"
#include "Renderer/Structures/RendererRectangle.h"
#include "Renderer/RenderView.h"
#include "Renderer/RendererUtils.h"
#include "Renderer/Structures/RendererHudBar.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/clock.h"
#include "Specific/trutils.h"

namespace TEN::Renderer
{
	using namespace TEN::Renderer::Structures;
	using namespace TEN::Utils;

	Renderer g_Renderer;

	Renderer::Renderer() :
		_gameCamera({0, 0, 0}, {0, 0, 1}, {0, 1, 0}, 1, 1, 0, 1, 10, 90),
		_oldGameCamera({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, 1, 1, 0, 1, 10, 90),
		_currentGameCamera({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, 1, 1, 0, 1, 10, 90)
	{
	}

	Renderer::~Renderer()
	{
		ImGuiShutdown();
		FreeRendererData();
	}

	void Renderer::FreeRendererData()
	{
		_items.resize(0);
		_moveableObjects.resize(0);
		_staticObjects.clear();
		_sprites.resize(0);
		_rooms.resize(0);
		_animatedTextureSets.resize(0);

		_shadowLight = nullptr;

		_dynamicLightList = 0;
		for (auto& dynamicLightList : _dynamicLights)
			dynamicLightList.resize(0);

		for (auto& mesh : _meshes)
			delete mesh;
		_meshes.resize(0);

		_videoSprite = {};
	}

	void Renderer::Lock()
	{
		_isLocked = true;
	}

	// Video frames fill the whole sprite; the next renderer uploads the frame of the video handler.
	void Renderer::UpdateVideoSprite(int width, int height)
	{
		_videoSprite.X = _videoSprite.Y = 0;
		_videoSprite.Width = width;
		_videoSprite.Height = height;
		_videoSprite.UV[0] = Vector2(0.0f, 0.0f);
		_videoSprite.UV[1] = Vector2(1.0f, 0.0f);
		_videoSprite.UV[2] = Vector2(1.0f, 1.0f);
		_videoSprite.UV[3] = Vector2(0.0f, 1.0f);
	}

	void Renderer::ClearVideoSprite()
	{
		_videoSprite = {};
	}

	bool Renderer::IsVideoSpriteActive() const
	{
		return (_videoSprite.Width > 0);
	}

	void Renderer::ReloadShaders()
	{
		Next::g_NextRenderer.ReloadShaders();
	}

	int Renderer::Synchronize()
	{
		// Sync the renderer
		int nf = TimeSync();
		if (nf < 2)
		{
			int i = 2 - nf;
			nf = 2;
			do
			{
				while (!TimeSync());
				i--;
			}
			while (i);
		}

		return nf;
	}

	void Renderer::UpdateProgress(float value)
	{
		RenderLoadingScreen(value);
	}

	float Renderer::CalculateFrameRate()
	{
		static int last_time = clock();
		static int count = 0;
		static float fps = 0.0f;

		count++;
		if (count == 10)
		{
			double t;
			time_t this_time;
			this_time = clock();
			t = (this_time - last_time) / (double)CLOCKS_PER_SEC;
			last_time = this_time;
			fps = float(count / t);
			count = 0;
		}

		_fps = fps;

		return fps;
	}

	void Renderer::SetGraphicsSettingsChanged()
	{
		_graphicsSettingsChanged = true;
	}

	RendererDebugPage Renderer::GetDebugPage() const
	{
		return _debugPage;
	}
}
