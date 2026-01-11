#include "framework.h"
#include "Game/Hud/PickupSummary.h"

#include "Game/pickup/pickup.h"
#include "Game/pickup/pickup_ammo.h"
#include "Game/pickup/pickup_consumable.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/clock.h"

using namespace TEN::Math;
using TEN::Renderer::g_Renderer;

namespace TEN::Hud
{
	bool DisplayPickup::IsOffscreen() const
	{
		constexpr auto SCREEN_THRESHOLD_COEFF = 0.1f;
		constexpr auto SCREEN_THRESHOLD		  = Vector2(DISPLAY_SPACE_RES.x * SCREEN_THRESHOLD_COEFF, DISPLAY_SPACE_RES.y * SCREEN_THRESHOLD_COEFF);

		if (Position.x <= -SCREEN_THRESHOLD.x ||
			Position.y <= -SCREEN_THRESHOLD.y ||
			Position.x >= (DISPLAY_SPACE_RES.x + SCREEN_THRESHOLD.x) ||
			Position.y >= (DISPLAY_SPACE_RES.y + SCREEN_THRESHOLD.y))
		{
			return true;
		}

		return false;
	}

	void DisplayPickup::Update(bool isHead)
	{
		constexpr auto LIFE_BUFFER		   = 0.2f;
		constexpr auto SCALE_MAX		   = 0.4f;
		constexpr auto SCALE_MIN		   = 0.25f;
		constexpr auto HIDE_VEL_MAX		   = DISPLAY_SPACE_RES.x * 0.03f;
		constexpr auto HIDE_VEL_ACCEL	   = HIDE_VEL_MAX / 4;
		constexpr auto POS_LERP_ALPHA	   = 0.15f;
		constexpr auto STRING_SCALAR_ALPHA = 0.25f;
		constexpr auto ROT_RATE			   = ANGLE(360.0f / (LIFE_MAX * FPS));
		constexpr auto ROT				   = EulerAngles(0, ROT_RATE, 0);

		StoreInterpolationData();

		// Move offscreen.
		if (Life <= 0.0f && isHead)
		{
			HideVelocity = std::clamp(HideVelocity + HIDE_VEL_ACCEL, 0.0f, HIDE_VEL_MAX);
			Position.x += HideVelocity;
		}
		// Update position, scale, and opacity.
		else if (Life > 0.0f)
		{
			float totalDist = Vector2::Distance(Origin, Target);
			float coveredDist = Vector2::Distance(Origin, Position);

			// Handle edge case when stack shifts.
			if (coveredDist > totalDist)
			{
				Origin = Position;
				totalDist = Vector2::Distance(Origin, Target);
				coveredDist = Vector2::Distance(Origin, Position);
			}

			float alpha = coveredDist / totalDist;

			Position = Vector2::Lerp(Position, Target, POS_LERP_ALPHA);
			Scale = std::max(Lerp(SCALE_MIN, SCALE_MAX, alpha), Scale);
			Opacity = std::max(Lerp(0.0f, 1.0f, alpha), Opacity);
		}

		// Update orientation.
		Orientation += ROT;

		// Update string scale.
		float alpha = Scale / SCALE_MAX;
		StringScale = Lerp(0.0f, 1.0f, alpha) * (1.0f + StringScalar);
		StringScalar = Lerp(StringScalar, 0.0f, STRING_SCALAR_ALPHA);

		// Update life.
		Life -= 1.0f;
		if (!isHead)
			Life = std::max(Life, round(LIFE_BUFFER * FPS));
	}

	void PickupSummaryController::AddDisplayPickup(GAME_OBJECT_ID objectID, const Vector2& origin, unsigned int count)
	{
		constexpr auto STRING_SCALAR_MAX = 0.6f;

		// No count; return early.
		if (count == 0)
			return;

		float life = round(DisplayPickup::LIFE_MAX * FPS);

		// Increment count of existing display pickup if it exists.
		for (auto& pickup : _displayPickups)
		{
			// Ignore already disappearing display pickups.
			if (pickup.Life <= 0.0f)
				continue;

			if (pickup.ObjectID == objectID)
			{
				pickup.Count += count;
				pickup.Life = life;
				pickup.StringScalar = STRING_SCALAR_MAX;
				return;
			}
		}

		// Create new display pickup.
		auto& pickup = GetNewDisplayPickup();

		pickup.ObjectID = objectID;
		pickup.Count = count;
		pickup.Position =
		pickup.Origin = origin;
		pickup.Target = Vector2::Zero;
		pickup.Life = life;
		pickup.Scale = 0.0f;
		pickup.Opacity = 0.0f;
		pickup.HideVelocity = 0.0f;
		pickup.StringScale = 0.0f;
		pickup.StringScalar = 0.0f;
	}

	void PickupSummaryController::AddDisplayPickup(const ItemInfo& item)
	{
		// NOTE: Ammo and consumables are a special case, as internal amount differs from pickup amount.
		int ammoCount = GetDefaultAmmoCount(item.ObjectNumber);
		int consumableCount = GetDefaultConsumableCount(item.ObjectNumber);

		int count = DISPLAY_PICKUP_COUNT_ARG_DEFAULT;
		if (ammoCount != NO_VALUE)
			count = ammoCount;
		if (consumableCount != NO_VALUE)
			count = consumableCount;

		AddDisplayPickup(item.ObjectNumber, item.Pose.Position.ToVector3(), (item.HitPoints > 0) ? item.HitPoints : count);
	}

	void PickupSummaryController::AddDisplayPickup(GAME_OBJECT_ID objectID, const Vector3& pos, unsigned int count)
	{
		// Project 3D position to 2D origin.
		auto origin = g_Renderer.Get2DPosition(pos);

		AddDisplayPickup(objectID, origin.value_or(Vector2::Zero), count);
	}

	void PickupSummaryController::Update()
	{
		if (_displayPickups.empty())
			return;

		// Get and apply stack positions as targets.
		auto stackPositions = GetStackPositions();
		for (int i = 0; i < stackPositions.size(); i++)
			_displayPickups[i].Target = std::move(stackPositions[i]);

		// Update display pickups.
		bool isHead = true;
		for (auto& pickup : _displayPickups)
		{
			pickup.Update(isHead);
			isHead = false;
		}

		ClearInactiveDisplayPickups();
	}

	void PickupSummaryController::Draw() const
	{
		//DrawDebug();

		if (!g_GameFlow->GetSettings()->Hud.PickupNotifier)
			return;

		if (_displayPickups.empty())
			return;

		// Draw display pickups.
		for (const auto& pickup : _displayPickups)
		{
			if (pickup.IsOffscreen())
				continue;

			g_Renderer.DrawDisplayPickup(pickup);
		}
	}

	void PickupSummaryController::Clear()
	{
		_displayPickups.clear();
	}

	std::vector<Vector2> PickupSummaryController::GetStackPositions() const
	{
		constexpr auto STACK_HEIGHT_MAX	   = 6;
		constexpr auto SCREEN_SCALE_COEFF  = 1 / 7.0f;
		constexpr auto SCREEN_OFFSET_COEFF = 1 / 7.0f;
		constexpr auto SCREEN_SCALE		   = Vector2(DISPLAY_SPACE_RES.x * SCREEN_SCALE_COEFF, DISPLAY_SPACE_RES.y * SCREEN_SCALE_COEFF);
		constexpr auto SCREEN_OFFSET	   = Vector2(DISPLAY_SPACE_RES.y * SCREEN_OFFSET_COEFF);

		// Calculate stack positions. 
		auto stackPositions = std::vector<Vector2>{};
		stackPositions.resize(_displayPickups.size());
		for (int i = 0; i < _displayPickups.size(); i++)
		{
			auto relPos = (i < STACK_HEIGHT_MAX) ? (Vector2(0.0f, i) * SCREEN_SCALE) : Vector2(0.0f, DISPLAY_SPACE_RES.y);
			auto pos = (DISPLAY_SPACE_RES - relPos) - SCREEN_OFFSET;
			stackPositions[i] = pos;
		}

		return stackPositions;
	}

	DisplayPickup& PickupSummaryController::GetNewDisplayPickup()
	{
		TENAssert(_displayPickups.size() <= DISPLAY_PICKUP_COUNT_MAX, "Display pickup overflow.");

		// Add and return new display pickup.
		if (_displayPickups.size() < DISPLAY_PICKUP_COUNT_MAX)
			return _displayPickups.emplace_back();

		// Clear and return most recent display pickup.
		auto& pickup = _displayPickups.back();
		pickup = {};
		return pickup;
	}

	void PickupSummaryController::ClearInactiveDisplayPickups()
	{
		_displayPickups.erase(
			std::remove_if(
				_displayPickups.begin(), _displayPickups.end(),
				[](const DisplayPickup& pickup)
				{
					return ((pickup.Life <= 0.0f) && pickup.IsOffscreen());
				}),
			_displayPickups.end());
	}

	void PickupSummaryController::DrawDebug() const
	{
		PrintDebugMessage("Display pickups in summary: %d", _displayPickups.size());
	}
}
