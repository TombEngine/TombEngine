#include "framework.h"
#include "Game/Hud/Hud.h"

#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/Hud/PickupSummary.h"
#include "Game/Hud/StatusBars.h"
#include "Game/items.h"

using namespace TEN::Math;

namespace TEN::Hud
{
	HudController g_Hud = {};

	bool HudController::CanDrawHud() const
	{
		// Avoid drawing in title level and during cutscenes.
		return (CurrentLevel != 0 && CinematicBarsHeight <= EPSILON);
	}

	void HudController::Update(const ItemInfo& playerItem)
	{
		// Delay pickup summary update until HUD is available again to avoid
		// lack of consequential indication, e.g. after flyby triggered by a pickup.
		if (CanDrawHud())
			PickupSummary.Update();

		InteractionHighlighter.Update();
		TargetHighlighter.Update(playerItem);
		Speedometer.Update();
		StatusBars.Update(playerItem);
	}

	void HudController::Draw2D(const ItemInfo& playerItem) const
	{
		if (!CanDrawHud())
			return;

		InteractionHighlighter.Draw();
		TargetHighlighter.Draw();
		Speedometer.Draw();
		StatusBars.Draw(playerItem);
	}

	void HudController::Draw3D()
	{
		if (!CanDrawHud())
			return;

		PickupSummary.Draw();
	}

	void HudController::Clear()
	{
		InteractionHighlighter.Clear();
		TargetHighlighter.Clear();
		Speedometer.Clear();
		PickupSummary.Clear();
		StatusBars.Clear();
	}
}
