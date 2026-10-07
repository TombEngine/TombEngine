#pragma once

class EulerAngles;

namespace TEN::Entities::Effects
{
	constexpr auto LENSFLARE_ITEMFLAG_BRIGHTNESS_SCALE = 100.0f;

	struct LensFlare
	{
		int SpriteID = 0;

		Vector3 Position   = Vector3::Zero;
		int		RoomNumber = 0;	
		Color	Color	   = {};

		bool IsGlobal	   = false;
		bool EffectsEnabled = true;
	};

	extern std::vector<LensFlare> LensFlares;

	void SetupLensFlare(const Vector3& pos, int roomNumber, const Color& color, float* intensity, int spriteID, bool effectsEnabled = true);
	void ControlLensFlare(int itemNumber);
	void ClearLensFlares();
	void UpdateGlobalLensFlare();
}
