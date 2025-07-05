#include "framework.h"
#include "Game/Hud/TargetHighlighter.h"

#include "Game/camera.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/items.h"
#include "Game/lara/lara_fire.h"
#include "Game/lara/lara_helpers.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Specific/configuration.h"
#include "Specific/trutils.h"

using namespace TEN::Effects::DisplaySprite;
using namespace TEN::Math;
using namespace TEN::Utils;
using TEN::Renderer::g_Renderer;

namespace TEN::Hud
{
	float CrosshairData::GetScale(float cameraDist) const
	{
		constexpr auto RANGE			   = BLOCK(10);
		constexpr auto CROSSHAIR_SCALE_MAX = 0.12f;
		constexpr auto CROSSHAIR_SCALE_MIN = CROSSHAIR_SCALE_MAX / 3;

		auto alpha = cameraDist / RANGE;
		return Lerp(CROSSHAIR_SCALE_MAX, CROSSHAIR_SCALE_MIN, alpha);
	}

	float CrosshairData::GetRadius() const
	{
		// Get screen aspect ratio.
		auto screenRes = g_Renderer.GetScreenResolution().ToVector2();
		float screenResAspect = screenRes.x / screenRes.y;

		return ((((DISPLAY_SPACE_RES.x * Scale) / 2) * (RadiusScale * PulseScale)) * screenResAspect);
	}

	Vector2 CrosshairData::GetPositionOffset(short orientOffset) const
	{
		constexpr auto ANGLE_OFFSET = ANGLE(-360.0f / (SEGMENT_COUNT * 2));

		float offsetDist = GetRadius();
		auto relPosOffset = Vector2(offsetDist, 0.0f);
		auto rotMatrix = Matrix::CreateRotationZ(TO_RAD((Orientation + orientOffset) + ANGLE_OFFSET));

		auto posOffset = Vector2::Transform(relPosOffset, rotMatrix);
		return GetAspectCorrect2DPosition(posOffset);
	}

	void CrosshairData::SetPrimary()
	{
		IsPrimary = true;
		ColorTarget = COLOR_RED;
	}

	void CrosshairData::SetPeripheral()
	{
		IsPrimary = false;
		ColorTarget = COLOR_GRAY;
	}

	void CrosshairData::Update(const Vector3& targetPos, bool isActive, bool doPulse)
	{
		constexpr auto ROT					   = ANGLE(2.0f);
		constexpr auto ALIGN_ANGLE_STEP		   = ANGLE(360.0f / SEGMENT_COUNT);
		constexpr auto SCALE_PRIMARY		   = 1.0f;
		constexpr auto SCALE_PERIPHERAL		   = 0.8f;
		constexpr auto RADIUS_SCALE_PRIMARY	   = 0.5f;
		constexpr auto RADIUS_SCALE_PERIPHERAL = 0.25f;
		constexpr auto PULSE_SCALE_MAX		   = 1.3f;
		constexpr auto MORPH_LERP_ALPHA		   = 0.3f;
		constexpr auto ORIENT_LERP_ALPHA	   = 0.1f;
		constexpr auto RADIUS_LERP_ALPHA	   = 0.2f;

		if (Position.has_value())
			StoreInterpolationData();

		// Update active status.
		IsActive = isActive;

		// Update position.
		Position = g_Renderer.Get2DPosition(targetPos);

		// Update orientation.
		if (IsPrimary)
		{
			Orientation += IsActive ? ROT : (ROT * 2);
		}
		else
		{
			short closestAlignAngle = round(Orientation / ALIGN_ANGLE_STEP) * ALIGN_ANGLE_STEP;
			Orientation = (short)round(Lerp(Orientation, closestAlignAngle, ORIENT_LERP_ALPHA));
		}

		// Update scale.
		if (IsActive)
		{
			float cameraDist = Vector3::Distance(Camera.pos.ToVector3(), targetPos);
			float scaleTarget = GetScale(cameraDist) * (IsPrimary ? SCALE_PRIMARY : SCALE_PERIPHERAL);

			Scale = Lerp(Scale, scaleTarget, MORPH_LERP_ALPHA);
		}
		else
		{
			Scale = Lerp(Scale, 0.0f, MORPH_LERP_ALPHA / 4);
		}

		// Update color.
		if (!IsActive)
		{
			ColorTarget = CrosshairData::COLOR_GRAY;
			ColorTarget.w = 0.0f;
		}
		Color = Vector4::Lerp(Color, ColorTarget, MORPH_LERP_ALPHA);

		float radiusScaleTarget = IsPrimary ? RADIUS_SCALE_PRIMARY : RADIUS_SCALE_PERIPHERAL;
		if (!IsActive)
			radiusScaleTarget = RADIUS_SCALE_PERIPHERAL;

		// Update radius scale.
		RadiusScale = Lerp(RadiusScale, radiusScaleTarget, RADIUS_LERP_ALPHA);

		// Update pulse scale.
		PulseScale = doPulse ? PULSE_SCALE_MAX : Lerp(PulseScale, 1.0f, MORPH_LERP_ALPHA);

		// Update segments.
		for (auto& segment : Segments)
			segment.PosOffset = GetPositionOffset(segment.OrientOffset);
	}

	void CrosshairData::Draw() const
	{
		constexpr auto SPRITE_SEQUENCE_OBJECT_ID = ID_CROSSHAIR_GRAPHICS;
		constexpr auto STATIC_ELEMENT_SPRITE_ID	 = 0;
		constexpr auto SEGMENT_ELEMENT_SPRITE_ID = 1;
		constexpr auto PRIORITY					 = 0; // TODO: Check later. May interfere with Lua display sprites. -- Sezz 2023.10.06

		if (!Position.has_value())
			return;

		auto pos0 = Vector2::Lerp(PrevPosition, *Position, g_Renderer.GetInterpolationFactor());
		short orient0 = PrevOrientation + Geometry::GetShortestAngle(PrevOrientation, Orientation) * g_Renderer.GetInterpolationFactor();
		float scale = Lerp(PrevScale, Scale, g_Renderer.GetInterpolationFactor());
		auto color = Color::Lerp(PrevColor, Color, g_Renderer.GetInterpolationFactor());

		// Draw main static element.
		AddDisplaySprite(
			SPRITE_SEQUENCE_OBJECT_ID, STATIC_ELEMENT_SPRITE_ID,
			pos0, orient0, Vector2(scale), color,
			PRIORITY, DisplaySpriteAlignMode::Center, DisplaySpriteScaleMode::Fill,
			BlendMode::Additive, DisplaySpritePhase::Draw);

		// Draw animated outer segment elements.
		for (int i = 0; i < Segments.size(); i++)
		{
			const auto& segment = Segments[i];
			const auto& prevSegment = PrevSegments[i];

			auto pos1 = pos0 + Vector2::Lerp(prevSegment.PosOffset, segment.PosOffset, g_Renderer.GetInterpolationFactor());
			short orient1 = orient0 + (prevSegment.OrientOffset + (Geometry::GetShortestAngle(prevSegment.OrientOffset, segment.OrientOffset) * g_Renderer.GetInterpolationFactor()));

			AddDisplaySprite(
				SPRITE_SEQUENCE_OBJECT_ID, SEGMENT_ELEMENT_SPRITE_ID,
				pos1, orient1, Vector2(scale / 2), color,
				PRIORITY, DisplaySpriteAlignMode::Center, DisplaySpriteScaleMode::Fill,
				BlendMode::Additive, DisplaySpritePhase::Draw);
		}
	}

	void TargetHighlighterController::Update(const ItemInfo& playerItem)
	{
		const auto& player = GetLaraInfo(playerItem);

		// Check if target highlighter is enabled or lasersight is active.
		if (!g_Configuration.EnableTargetHighlighter || player.Control.Look.IsUsingBinoculars)
		{
			if (!_crosshairs.empty())
				_crosshairs.clear();

			return;
		}

		// Loop over player targets.
		auto itemNumbers = std::vector<int>{};
		for (const auto* item : player.TargetList)
		{
			if (item == nullptr)
				continue;

			// Collect item number.
			if (item->HitPoints != NOT_TARGETABLE)
				itemNumbers.push_back(item->Index);

			// Find crosshair at item number key.
			auto it = _crosshairs.find(item->Index);
			if (it == _crosshairs.end())
				continue;

			// Set crosshair as primary or peripheral.
			auto& crosshair = it->second;
			if (player.TargetEntity != nullptr && item->Index == player.TargetEntity->Index)
			{
				crosshair.SetPrimary();
			}
			else
			{
				crosshair.SetPeripheral();
			}
		}

		// Update crosshairs.
		Update(itemNumbers);
	}

	void TargetHighlighterController::Draw() const
	{
		//DrawDebug();

		if (_crosshairs.empty())
			return;

		for (const auto& [itemNumber, crosshair] : _crosshairs)
			crosshair.Draw();
	}

	void TargetHighlighterController::Clear()
	{
		*this = {};
	}

	void TargetHighlighterController::Update(const std::vector<int>& itemNumbers)
	{
		constexpr auto TARGET_BONE_ID = 0;

		// No crosshairs to update; return early.
		if (_crosshairs.empty() && itemNumbers.empty())
			return;

		// Update active crosshairs.
		for (int itemNumber : itemNumbers)
		{
			const auto& item = g_Level.Items[itemNumber];
			auto targetPos = GetJointPosition(item, TARGET_BONE_ID).ToVector3();

			// Update existing active crosshair.
			auto it = _crosshairs.find(itemNumber);
			if (it != _crosshairs.end())
			{
				auto& crosshair = it->second;
				if (crosshair.IsActive)
				{
					crosshair.Update(targetPos, true, item.HitStatus);
					continue;
				}
			}

			// Add new active crosshair.
			AddCrosshair(itemNumber, targetPos);
		}

		// Update inactive crosshairs.
		for (auto& [itemNumber, crosshair] : _crosshairs)
		{
			// Find crosshairs at absent item number keys.
			if (Contains(itemNumbers, itemNumber))
				continue;

			const auto& item = g_Level.Items[itemNumber];
			auto targetPos = GetJointPosition(item, 0).ToVector3();

			// Update inactive crosshair.
			crosshair.Update(targetPos, false, item.HitStatus);
		}

		ClearInactiveCrosshairs();
	}

	CrosshairData& TargetHighlighterController::GetNewCrosshair(int itemNumber)
	{
		constexpr auto CROSSHAIR_COUNT_MAX = 16;

		// Map is full; clear smallest crosshair.
		if (_crosshairs.size() >= CROSSHAIR_COUNT_MAX)
		{
			int key = 0;
			float smallestScale = FLT_MAX;
			
			for (auto& [itemNumber, crosshair] : _crosshairs)
			{
				if (crosshair.Scale < smallestScale)
				{
					key = itemNumber;
					smallestScale = crosshair.Scale;
				}
			}

			_crosshairs.erase(key);
		}

		// Return new crosshair.
		_crosshairs.insert({ itemNumber, {} });
		auto& crosshair = _crosshairs.at(itemNumber);
		crosshair = {};
		return crosshair;
	}

	// TODO: If crosshair happens to be in view upon spawn, first frame is sometimes garbage.
	void TargetHighlighterController::AddCrosshair(int itemNumber, const Vector3& targetPos)
	{
		constexpr auto RADIUS_SCALE_START = 0.25f;
		constexpr auto ANGLE_STEP		  = ANGLE(360.0f / CrosshairData::SEGMENT_COUNT);

		auto pos = g_Renderer.Get2DPosition(targetPos);
		if (!pos.has_value())
			return;

		// Create new crosshair.
		auto& crosshair = GetNewCrosshair(itemNumber);

		crosshair.IsActive = true;
		crosshair.IsPrimary = false;
		crosshair.Position = *pos;
		crosshair.Orientation = 0;
		crosshair.Scale = 0.0f;
		crosshair.Color = CrosshairData::COLOR_GRAY;
		crosshair.Color.w = 0.0f;
		crosshair.ColorTarget = CrosshairData::COLOR_GRAY;
		crosshair.RadiusScale = RADIUS_SCALE_START;
		crosshair.PulseScale = 1.0f;

		// Initialize segments.
		short angleOffset = 0;
		for (auto& segment : crosshair.Segments)
		{
			segment.PosOffset = crosshair.GetPositionOffset(segment.OrientOffset);
			segment.OrientOffset = angleOffset;

			angleOffset += ANGLE_STEP;
		}
	}

	void TargetHighlighterController::ClearInactiveCrosshairs()
	{
		for (auto it = _crosshairs.begin(); it != _crosshairs.end();)
		{
			const auto& crosshair = it->second;
			(!crosshair.IsActive && crosshair.Scale <= EPSILON) ?
				(it = _crosshairs.erase(it)) : ++it;
		}
	}

	void TargetHighlighterController::DrawDebug() const
	{
		unsigned int primaryCount = 0;
		unsigned int peripheralCount = 0;

		for (const auto& [itemNumber, crosshair] : _crosshairs)
			crosshair.IsPrimary ? primaryCount++ : peripheralCount++;

		PrintDebugMessage("TARGET HIGHLIGHTER DEBUG");
		PrintDebugMessage(g_Configuration.EnableTargetHighlighter ? "Enabled" : "Disabled");
		PrintDebugMessage("Primary crosshairs: %d", primaryCount);
		PrintDebugMessage("Peripheral crosshairs: %d", peripheralCount);
	}
}
