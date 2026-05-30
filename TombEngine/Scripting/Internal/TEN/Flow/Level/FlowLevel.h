#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Scripting/Internal/TEN/Flow/DynamicSky/DynamicSky.h"
#include "Scripting/Internal/TEN/Flow/Horizon/Horizon.h"
#include "Scripting/Internal/TEN/Flow/LensFlare/LensFlare.h"
#include "Scripting/Internal/TEN/Flow/SkyLayer/SkyLayer.h"
#include "Scripting/Internal/TEN/Flow/Starfield/Starfield.h"
#include "Scripting/Internal/TEN/Flow/Fog/Fog.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/InventoryItem/InventoryItem.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"

using namespace TEN::Scripting;

struct Level : public ScriptInterfaceLevel
{
	Fog			Fog			 = {};
	int			LevelFarView = 0;
	std::string AmbientTrack = {};

	SkyLayer Layer1 = {};
	SkyLayer Layer2 = {};

	// Dynamic sky container: atmospheric sky dome, aurora and volumetric clouds.
	TEN::Scripting::DynamicSky DynamicSky = {};
	TEN::Scripting::Horizon Horizon1 = {};
	TEN::Scripting::Horizon Horizon2 = {};
	TEN::Scripting::LensFlare LensFlare = {};
	TEN::Scripting::MoonLens  MoonLens  = {};
	TEN::Scripting::Starfield Starfield = {};

	// Per-level dust storm config (level.dustStorm).
	TEN::Scripting::LevelDustStorm DustStorm = {};

	// Per-level underwater sky config (level.underwaterSky).
	TEN::Scripting::LevelUnderwaterSky UnderwaterSky = {};

	WeatherType Weather				= WeatherType::None;
	float		WeatherStrength		= 1.0f;
	bool		WeatherClustering	= true;
	bool		Storm				= false;
	bool		Rumble				= false;

	// Steady wind for this level. WindSpeed <= 0 means no override (keeps
	// any wind set globally from Settings.lua via Flow.SetCloudWind).
	// WindSpeed range 0..8 maps to cloud drift and particle / hair movement.
	float		WindSpeed			= -1.0f;
	Vec2		WindDirection		= Vec2(1.0f, 0.0f);

	LaraType Type = LaraType::Normal;
	int LevelSecrets = 0;
	std::vector<InventoryItem> InventoryObjects = {};

	bool ResetHub = false;

	// Dynamic snow surface height offset. Positive values raise the surface
	// (thicker snow); negative values lower it. Saved per savegame slot.
	// Writable from Lua via SetDynamicSnowLevel / GetDynamicSnowLevel.
	float SnowSurfaceOffset = 0.0f;

	// Per-level snow overlay depth override, in world units.
	// 0 means "use the global Settings.Snow.maxDepth value".
	int SnowMaxDepth = 0;

	// TODO: Clean up this mess.

	RGBAColor8Byte GetFogColor() const override;
	float GetWeatherStrength() const override;
	bool GetSkyLayerEnabled(int index) const override;
	bool GetStormEnabled() const override;
	bool GetRumbleEnabled() const override;
	short GetSkyLayerSpeed(int index) const override;
	RGBAColor8Byte GetSkyLayerColor(int index) const override;
	LaraType GetLaraType() const override;
	void SetWeatherStrength(float val);
	void SetDynamicSnowLevel(float offset);
	float GetDynamicSnowLevel() const;
	float GetSnowSurfaceOffset() const override { return SnowSurfaceOffset; }
	int   GetSnowMaxDepth() const override { return SnowMaxDepth; }
	static void Register(sol::table& parent);
	WeatherType GetWeatherType() const override;
	bool GetWeatherClustering() const override;
	float GetFogMinDistance() const override;
	float GetFogMaxDistance() const override;
	float GetFarView() const override;
	void SetSecrets(int secrets);
	int GetSecrets() const override;
	std::string GetAmbientTrack() const override;
	bool GetResetHubEnabled() const override;

	// Horizon getters
	bool GetHorizonEnabled(int index) const override;
	GAME_OBJECT_ID GetHorizonObjectID(int index) const override;
	float GetHorizonTransparency(int index) const override;
	Vector3 GetHorizonPosition(int index) const override;
	EulerAngles GetHorizonOrientation(int index) const override;
	Vector3 GetHorizonPrevPosition(int index) const override;
	EulerAngles GetHorizonPrevOrientation(int index) const override;

	// Compatibility
	bool GetHorizon1Enabled() const;
	void SetHorizon1Enabled(bool enabled);

	// Lens flare getters
	bool  GetLensFlareEnabled() const override;
	bool  GetLensFlareEffects() const override;
	int	  GetLensFlareSunSpriteID() const override;
	short GetLensFlarePitch() const override;
	short GetLensFlareYaw() const override;
	Color GetLensFlareColor() const override;
	Color GetLensFlareEvaluatedColor() const override;
	int   GetLensFlareColorMode() const override;

	// Lens flare mutable access (for debug UI).
	TEN::Scripting::LensFlare& GetMutableLensFlare();

	// Starfield getters
	int	  GetStarfieldStarCount() const override;
	int	  GetStarfieldMeteorCount() const override;
	int	  GetStarfieldMeteorSpawnDensity() const override;
	float GetStarfieldMeteorVelocity() const override;

	// Utility
	const SkyLayer& GetSkyLayer(int index) const;
	const TEN::Scripting::Horizon& GetHorizon(int index) const;
};
