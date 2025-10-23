#include "framework.h"
#include "Game/Hud/Speedometer.h"

#include "Game/effects/DisplaySprite.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/clock.h"

using namespace TEN::Effects::DisplaySprite;
using namespace TEN::Math;

using TEN::Renderer::g_Renderer;

namespace TEN::Hud
{
	void SpeedometerController::UpdateValue(float value)
	{
		_value = std::clamp(value, 0.0f, 1.0f);
		_hasValueUpdated = true;
	}

	void SpeedometerController::Update()
	{
		constexpr auto POINTER_ANGLE_MAX		= ANGLE(120.0f);
		constexpr auto POINTER_ANGLE_LERP_ALPHA = 0.25f;
		constexpr auto FADE_TIME				= 0.2f;

		if (!_hasValueUpdated && _life <= 0.0f &&
			_value <= 0.0f && _pointerAngle <= 0.0f)
		{
			return;
		}

		StoreInterpolationData();

		// Update life and updated value status.
		_life = std::clamp(_life + (_hasValueUpdated ? 1.0f : -1.0f), 0.0f, LIFE_MAX * FPS);
		_hasValueUpdated = false;

		// Ensure value resets to 0.
		if (_life <= 0.0f)
			_value = 0.0f;

		// Update appearance.
		_pointerAngle = Lerp(_pointerAngle, POINTER_ANGLE_MAX * _value, POINTER_ANGLE_LERP_ALPHA);
		_opacity = std::clamp(_life / std::round(FADE_TIME * FPS), 0.0f, 1.0f);
	}

	void SpeedometerController::Draw() const
	{
		constexpr auto POS						 = Vector2(DISPLAY_SPACE_RES.x - (DISPLAY_SPACE_RES.x / 6), DISPLAY_SPACE_RES.y - (DISPLAY_SPACE_RES.y / 10));
		constexpr auto POINTER_ANGLE_OFFSET		 = ANGLE(90.0f);
		constexpr auto SCALE					 = Vector2(0.35f);
		constexpr auto DIAL_ELEMENT_SPRITE_ID	 = 0;
		constexpr auto POINTER_ELEMENT_SPRITE_ID = 1;
		constexpr auto DIAL_PRIORITY			 = 0;
		constexpr auto POINTER_PRIORITY			 = 1;

		//DrawDebug();

		if (!g_GameFlow->GetSettings()->Hud.Speedometer)
			return;

		if (_life <= 0.0f)
			return;

		short pointerAngle = (short)Lerp(_prevPointerAngle, _pointerAngle, g_Renderer.GetInterpolationFactor());
		auto color = Color(1.0f, 1.0f, 1.0f, Lerp(_prevOpacity, _opacity, g_Renderer.GetInterpolationFactor()));

		// Draw dial.
		AddDisplaySprite(
			ID_SPEEDOMETER_GRAPHICS, DIAL_ELEMENT_SPRITE_ID,
			POS, 0, SCALE, color,
			DIAL_PRIORITY, DisplaySpriteAlignMode::Center, DisplaySpriteScaleMode::Fit, BlendMode::AlphaBlend,
			DisplaySpritePhase::Draw);

		// Draw pointer.
		AddDisplaySprite(
			ID_SPEEDOMETER_GRAPHICS, POINTER_ELEMENT_SPRITE_ID,
			POS, pointerAngle + POINTER_ANGLE_OFFSET, SCALE, color,
			POINTER_PRIORITY, DisplaySpriteAlignMode::Center, DisplaySpriteScaleMode::Fit, BlendMode::AlphaBlend,
			DisplaySpritePhase::Draw);
	}

	void SpeedometerController::Clear()
	{
		*this = {};
	}

	void SpeedometerController::DrawDebug() const
	{
		PrintDebugMessage("SPEEDOMETER DEBUG");
		PrintDebugMessage("Value: %.3f", _value);
		PrintDebugMessage("Pointer angle: %.3f", _pointerAngle);
		PrintDebugMessage("Opacity: %.3f", _opacity);
		PrintDebugMessage("Life: %.3f", _life / FPS);
	}
}
