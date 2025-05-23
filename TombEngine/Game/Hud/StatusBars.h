#pragma once
#include "Renderer/Structures/RendererHudBar.h"

enum GAME_OBJECT_ID : short;
struct ItemInfo;

namespace TEN::Hud
{
	using namespace TEN::Renderer;
	using namespace TEN::Renderer::Structures;

	struct StatusBar
	{
		static constexpr auto LIFE_MAX = 0.75f;

		float Value		  = 0.0f;
		float TargetValue = 0.0f;
		float Life		  = 0.0f;
		float Opacity	  = 0.0f; // TODO: Opacity in renderer.

		void Initialize(float value);
		void Update(float value);
	};

	class StatusBarsController
	{
	private:
		// Fields

		StatusBar _airBar	   = {};
		StatusBar _exposureBar = {};
		StatusBar _healthBar   = {};
		StatusBar _staminaBar  = {};

		bool _doFlash = false;

	public:
		// Utilities

		void Initialize(const ItemInfo& item);
		void Update(const ItemInfo& item);
		void Draw(const ItemInfo& item) const;
		void Clear();

	private:
		// Update helpers

		void UpdateAirBar(const ItemInfo& item);
		void UpdateExposureBar(const ItemInfo& item);
		void UpdateHealthBar(const ItemInfo& item);
		void UpdateStaminaBar(const ItemInfo& item);

		// Draw helpers

		void DrawStatusBar(float value, float criticalValue, const RendererHudBar& rHudBar, GAME_OBJECT_ID textureID, int frame, bool isPoisoned) const;
		void DrawAirBar() const;
		void DrawExposureBar() const;
		void DrawHealthBar(bool isPoisoned) const;
		void DrawStaminaBar() const;
	};
}
