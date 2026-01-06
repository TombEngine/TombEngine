#include "framework.h"
#include "Game/Hud/StatusBars.h"

#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Objects/game_object_ids.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/clock.h"

using namespace TEN::Renderer;

extern RendererHudBar* g_AirBar;
extern RendererHudBar* g_ExposureBar;
extern RendererHudBar* g_HealthBar;
extern RendererHudBar* g_StaminaBar;

namespace TEN::Hud
{
	void StatusBar::Initialize(float value)
	{
		Value =
		TargetValue = std::clamp(value, 0.0f, 1.0f);
	}

	void StatusBar::Update(float value)
	{
		constexpr auto LIFE_START_FADING = 0.2f;
		constexpr auto LERP_ALPHA		 = 0.3f;

		// Update life.
		if (Life > 0.0f)
			Life -= 1.0f;

		// Update opacity.
		float alpha = std::clamp(Life, 0.0f, LIFE_START_FADING) / LIFE_START_FADING;
		Opacity = Lerp(0.0f, 1.0f, alpha);

		// Update target value.
		TargetValue = std::clamp(value, 0.0f, 1.0f);

		// Update value.
		Value = Lerp(Value, TargetValue, LERP_ALPHA);
		if (abs(Value - TargetValue) <= EPSILON)
			Value = TargetValue;
	}

	void StatusBarsController::Initialize(const ItemInfo& item)
	{
		const auto& player = GetLaraInfo(item);

		// Initialize bar values.
		_airBar.Initialize(player.Status.Air / LARA_AIR_MAX);
		_exposureBar.Initialize(player.Status.Exposure / LARA_EXPOSURE_MAX);
		_healthBar.Initialize(item.HitPoints / LARA_HEALTH_MAX);
		_staminaBar.Initialize(player.Status.Stamina / LARA_STAMINA_MAX);
	}

	void StatusBarsController::Update(const ItemInfo& item)
	{
		constexpr auto FLASH_INTERVAL = 0.2f;

		// Update flash.
		if ((GlobalCounter % (int)round(FLASH_INTERVAL * FPS)) == 0)
			_doFlash = !_doFlash;

		// Update bars.
		UpdateAirBar(item);
		UpdateExposureBar(item);
		UpdateHealthBar(item);
		UpdateStaminaBar(item);
	}

	void StatusBarsController::Clamp(const ItemInfo& item)
	{
		const auto& player = GetLaraInfo(item);

		_airBar.Value = _airBar.TargetValue = player.Status.Air / LARA_AIR_MAX;
		_exposureBar.Value = _exposureBar.TargetValue = player.Status.Exposure / LARA_EXPOSURE_MAX;
		_healthBar.Value = _healthBar.TargetValue = item.HitPoints / LARA_HEALTH_MAX;
		_staminaBar.Value = _staminaBar.TargetValue = player.Status.Stamina / LARA_STAMINA_MAX;
	}

	void StatusBarsController::Draw(const ItemInfo& item) const
	{
		// Avoid drawing if HUD is disabled.
		if (!g_GameFlow->GetSettings()->Hud.StatusBars)
			return;

		const auto& player = GetLaraInfo(item);
		bool isPoisoned = (player.Status.Poison != 0);

		// Draw bars.
		DrawAirBar();
		DrawExposureBar();
		DrawHealthBar(isPoisoned);
		DrawStaminaBar();
	}

	void StatusBarsController::Clear()
	{
		*this = {};
	}

	void StatusBarsController::UpdateAirBar(const ItemInfo& item)
	{
		const auto& player = GetLaraInfo(item);

		// Update generic data.
		_airBar.Update(player.Status.Air / LARA_AIR_MAX);
		
		// Update life.
		if (_airBar.Value != _airBar.TargetValue ||
			player.Control.WaterStatus == WaterStatus::Underwater)
		{
			_airBar.Life = round(StatusBar::LIFE_MAX * FPS);
		}

		// HACK: Special case for UPV as it sets player.Control.WaterStatus to WaterStatus::Dry.
		if (player.Context.Vehicle != NO_VALUE)
		{
			const auto& vehicleItem = g_Level.Items[player.Context.Vehicle];
			if (vehicleItem.ObjectNumber == ID_UPV)
				_airBar.Life = round(StatusBar::LIFE_MAX * FPS);
		}
	}

	void StatusBarsController::UpdateExposureBar(const ItemInfo& item)
	{
		const auto& player = GetLaraInfo(item);

		// Update generic data.
		_exposureBar.Update(player.Status.Exposure / LARA_EXPOSURE_MAX);

		// Update life.
		if (_exposureBar.Value != _exposureBar.TargetValue ||
			(TestEnvironment(ENV_FLAG_WATER, item.RoomNumber) && TestEnvironment(ENV_FLAG_COLD, item.RoomNumber)))
		{
			_exposureBar.Life = round(StatusBar::LIFE_MAX * FPS);
		}
	}

	void StatusBarsController::UpdateHealthBar(const ItemInfo& item)
	{
		const auto& player = GetLaraInfo(item);

		// Update generic data.
		_healthBar.Update(item.HitPoints / LARA_HEALTH_MAX);

		// Update life.
		if (_healthBar.Value != _healthBar.TargetValue ||
			item.HitPoints <= LARA_HEALTH_CRITICAL || player.Status.Poison != 0 ||
			(player.Control.HandStatus == HandStatus::WeaponDraw &&
				player.Control.Weapon.GunType != LaraWeaponType::Flare) || // HACK: Exclude flare.
			(player.Control.HandStatus == HandStatus::WeaponReady &&
				player.Control.Weapon.GunType != LaraWeaponType::Torch))   // HACK: Exclude torch.
		{
			_healthBar.Life = round(StatusBar::LIFE_MAX * FPS);
		}

		// Special case for weapon undraw.
		if (_healthBar.Value == _healthBar.TargetValue &&
			item.HitPoints > LARA_HEALTH_CRITICAL && player.Status.Poison == 0 &&
			player.Control.HandStatus == HandStatus::WeaponUndraw)
		{
			_healthBar.Life = 0.0f;
		}
	}

	void StatusBarsController::UpdateStaminaBar(const ItemInfo& item)
	{
		const auto& player = GetLaraInfo(item);

		// Update generic data.
		_staminaBar.Update(player.Status.Stamina / LARA_STAMINA_MAX);

		// Update life.
		if (_staminaBar.Value != _staminaBar.TargetValue ||
			_staminaBar.Value != 1.0f)
		{
			_staminaBar.Life = round(StatusBar::LIFE_MAX * FPS);
		}
	}

	void StatusBarsController::DrawStatusBar(float value, float criticalValue, const RendererHudBar& rHudBar, GAME_OBJECT_ID textureID, int frame, bool isPoisoned) const
	{
		if (value <= criticalValue)
			value = _doFlash ? value : 0.0f;

		g_Renderer.DrawBar(value, rHudBar, textureID, frame, isPoisoned);
	}

	void StatusBarsController::DrawAirBar() const
	{
		constexpr auto TEXTURE_ID	  = ID_AIR_BAR_TEXTURE;
		constexpr auto CRITICAL_VALUE = LARA_AIR_CRITICAL / LARA_AIR_MAX;

		if (_airBar.Life <= 0.0f)
			return;

		DrawStatusBar(_airBar.Value, CRITICAL_VALUE, *g_AirBar, TEXTURE_ID, 0, false);
	}
	
	void StatusBarsController::DrawExposureBar() const
	{
		constexpr auto TEXTURE_ID	  = ID_SFX_BAR_TEXTURE;
		constexpr auto CRITICAL_VALUE = LARA_EXPOSURE_CRITICAL / LARA_EXPOSURE_MAX;

		if (_exposureBar.Life <= 0.0f)
			return;

		DrawStatusBar(_exposureBar.Value, CRITICAL_VALUE, *g_ExposureBar, TEXTURE_ID, 0, false);
	}

	void StatusBarsController::DrawHealthBar(bool isPoisoned) const
	{
		constexpr auto TEXTURE_ID	  = ID_HEALTH_BAR_TEXTURE;
		constexpr auto CRITICAL_VALUE = LARA_HEALTH_CRITICAL / LARA_HEALTH_MAX;

		if (_healthBar.Life <= 0.0f)
			return;

		DrawStatusBar(_healthBar.Value, CRITICAL_VALUE, *g_HealthBar, TEXTURE_ID, GlobalCounter, isPoisoned);
	}

	void StatusBarsController::DrawStaminaBar() const
	{
		constexpr auto TEXTURE_ID	  = ID_DASH_BAR_TEXTURE;
		constexpr auto CRITICAL_VALUE = 0;

		if (_staminaBar.Life <= 0.0f)
			return;

		DrawStatusBar(_staminaBar.Value, CRITICAL_VALUE, *g_StaminaBar, TEXTURE_ID, 0, false);
	}
}
