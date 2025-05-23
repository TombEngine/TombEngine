#pragma once
#include "Scripting/Internal/TEN/Flow/Horizon/Horizon.h"
#include "Scripting/Internal/TEN/Flow/LensFlare/LensFlare.h"
#include "Scripting/Internal/TEN/Flow/SkyLayer/SkyLayer.h"
#include "Scripting/Internal/TEN/Flow/Starfield/Starfield.h"
#include "Scripting/Internal/TEN/Flow/Fog/Fog.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/InventoryItem/InventoryItem.h"

using namespace TEN::Scripting;

static const std::unordered_map<std::string, WeatherType> WEATHER_TYPES
{
	{ "None", WeatherType::None },
	{ "Rain", WeatherType::Rain },
	{ "Snow", WeatherType::Snow }
};

static const std::unordered_map<std::string, LaraType> PLAYER_TYPES
{
	{ "Normal", LaraType::Normal },
	{ "Young", LaraType::Young },
	{ "Bunhead", LaraType::Bunhead },
	{ "Catsuit", LaraType::Catsuit },
	{ "Divesuit", LaraType::Divesuit },
	{ "Invisible", LaraType::Invisible }
};

struct Level : public ScriptInterfaceLevel
{
	Fog			Fog			 = {};
	int			LevelFarView = 0;
	std::string AmbientTrack = {};

	SkyLayer Layer1 = {};
	SkyLayer Layer2 = {};
	TEN::Scripting::Horizon Horizon1 = {};
	TEN::Scripting::Horizon Horizon2 = {};
	TEN::Scripting::LensFlare LensFlare = {};
	TEN::Scripting::Starfield Starfield = {};

	WeatherType Weather			= WeatherType::None;
	float		WeatherStrength = 1.0f;
	bool		Storm			= false;
	bool		Rumble			= false;

	LaraType Type = LaraType::Normal;
	int LevelSecrets = 0;
	std::vector<InventoryItem> InventoryObjects = {};

	bool ResetHub = false;

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
	static void Register(sol::table& parent);
	WeatherType GetWeatherType() const override;
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
	int	  GetLensFlareSunSpriteID() const override;
	short GetLensFlarePitch() const override;
	short GetLensFlareYaw() const override;
	Color GetLensFlareColor() const override;

	// Starfield getters
	int	  GetStarfieldStarCount() const override;
	int	  GetStarfieldMeteorCount() const override;
	int	  GetStarfieldMeteorSpawnDensity() const override;
	float GetStarfieldMeteorVelocity() const override;

	// Utility
	const SkyLayer& GetSkyLayer(int index) const;
	const TEN::Scripting::Horizon& GetHorizon(int index) const;
};
