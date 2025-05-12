#include "framework.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"

#include "Scripting/Internal/ScriptAssert.h"

using namespace TEN::Scripting;

/***
Stores level metadata.
These are things things which aren't present in the compiled level file itself.

@tenclass Flow.Level
@pragma nostrip
*/

/// Make a new Level object.
//@function Level
//@treturn Level a Level object.
void Level::Register(sol::table& parent)
{
	// Register type.
	parent.new_usertype<Level>(
		"Level",
		sol::constructors<Level()>(),
		sol::call_constructor, sol::constructors<Level()>(),

/// (string) String key for the level's name. Corresponds to an entry in strings.lua.
//@mem nameKey
		"nameKey", &Level::NameStringKey,

/// (string) Level-specific Lua script file.
// Path of the Lua file holding the level's logic script, relative to the location of the Tomb Engine executable.
//@mem scriptFile
		"scriptFile", &Level::ScriptFileName,

/// (string) Compiled file path.
// This path is relative to the location of the TombEngine executable.
//@mem levelFile
		"levelFile", &Level::FileName,

/// (string) Load screen image.
// Path of the level's load screen file (.png or .jpg), relative to the location of the Tomb Engine executable.
//@mem loadScreenFile
		"loadScreenFile", &Level::LoadScreenFileName,
		
/// (string) Initial ambient sound track to play.
// This is the filename of the track __without__ the .wav extension.
//@mem ambientTrack
		"ambientTrack", &Level::AmbientTrack,

/// (@{Flow.SkyLayer}) Primary sky cloud layer.
//@mem layer1
		"layer1", &Level::Layer1,

/// (@{Flow.SkyLayer}) Secondary sky cloud layer.
//@mem layer2
		"layer2", &Level::Layer2,

///  (@{Flow.Horizon}) Primary horizon object.
//@mem horizon1
		"horizon1", &Level::Horizon1,
		"horizon", sol::property(&Level::GetHorizon1Enabled, &Level::SetHorizon1Enabled), // Compatibility.

///  (@{Flow.Horizon}) Secondary horizon object.
//@mem horizon2
		"horizon2", &Level::Horizon2,

/// (@{Flow.Starfield}) Starfield in the sky.
// @mem starfield
		"starfield", &Level::Starfield,

/// (@{Flow.LensFlare}) Global lens flare.
// @mem lensFlare
		"lensFlare", &Level::LensFlare,

/// (@{Flow.Fog}) Global distance fog, with specified RGB color and distance.
// If not provided, distance fog will not be visible.
//@mem fog
		"fog", &Level::Fog,

/// (bool) Enable flickering lightning in the sky.
// Equivalent to classic TRLE's lightning setting, as in the TRC Ireland levels or TR4 Cairo levels.
//@mem storm
		"storm", &Level::Storm,

/// (WeatherType) Choose weather effect.
// Must be one of the values `WeatherType.None`, `WeatherType.Rain`, or `WeatherType.Snow`.
//@mem weather
		"weather", &Level::Weather,

/// (float) Choose weather strength.
// Must be value between `0.1` and `1.0`.
//@mem weatherStrength
		"weatherStrength", &Level::WeatherStrength,

/*** (LaraType) Appearance of Lara. Must be either `LaraType.Normal` or `LaraType.Young`.
E.g. `myLevel.laraType = LaraType.Young` will make Lara appear as young (with two ponytails rendered).
This setting does not affect ability to use weapons or flares.

 @mem laraType*/
		"laraType", &Level::Type,

/// (bool) Enable occasional screen shake effect.
// As seen in TRC's Sinking Submarine.
//@mem rumble
		"rumble", &Level::Rumble,

/// (int) The maximum draw distance for level.
// Given in sectors (blocks). Must be at least 4.
//@mem farView
		"farView", &Level::LevelFarView,

/// (bool) Reset hub data.
// Resets the state for all previous levels, including items, flipmaps and statistics.
//@mem resetHub
		"resetHub", &Level::ResetHub,

/// (table of @{Flow.InventoryItem}s) A table of inventory object layout overrides.
//@mem objects
		"objects", &Level::InventoryObjects,

/// (short) Set total secret count for current level.
//@mem secrets
		"secrets", &Level::LevelSecrets
	);
}

void Level::SetWeatherStrength(float val)
{
	bool cond = val <= 1.0f && val >= 0.0f;
	std::string msg{ "weatherStrength value must be in the range [0.1, 1.0]." };
	if (!ScriptAssert(cond, msg))
	{
		ScriptWarn("Setting weatherStrength view to 1.");
		WeatherStrength = 1.0f;
	}
	else
	{
		WeatherStrength = val;
	}
}

const SkyLayer& Level::GetSkyLayer(int index) const
{
	TENAssert(index == 0 || index == 1, "Sky layer index must be 0 or 1.");
	return (index == 0 ? Layer1 : Layer2);
}

RGBAColor8Byte Level::GetSkyLayerColor(int index) const
{
	return GetSkyLayer(index).GetColor();
}

bool Level::GetSkyLayerEnabled(int index) const
{
	return GetSkyLayer(index).Enabled;
}

short Level::GetSkyLayerSpeed(int index) const
{
	return GetSkyLayer(index).CloudSpeed;
}

LaraType Level::GetLaraType() const
{
	return Type;
}

bool Level::GetResetHubEnabled() const
{
	return ResetHub;
}

bool Level::GetStormEnabled() const
{
	return Storm;
}

bool Level::GetRumbleEnabled() const
{
	return Rumble;
}

float Level::GetWeatherStrength() const
{
	return WeatherStrength;	
}

WeatherType Level::GetWeatherType() const
{
	return Weather;
}

RGBAColor8Byte Level::GetFogColor() const
{
	return Fog.GetColor();
}

float Level::GetFogMinDistance() const
{
	return Fog.MinDistance;
}

float Level::GetFogMaxDistance() const
{
	return Fog.MaxDistance;
}

float Level::GetFarView() const
{
	return float(LevelFarView);
}

void Level::SetSecrets(int secrets)
{
	LevelSecrets = secrets;
}

int Level::GetSecrets() const
{
	return LevelSecrets;
}

std::string Level::GetAmbientTrack() const
{
	return AmbientTrack;
}

const TEN::Scripting::Horizon& Level::GetHorizon(int index) const
{
	TENAssert(index == 0 || index == 1, "Horizon index must be 0 or 1.");
	return (index == 0 ? Horizon1 : Horizon2);
}

bool Level::GetHorizon1Enabled() const
{
	return Horizon1.GetEnabled();
}

void Level::SetHorizon1Enabled(bool enabled)
{
	Horizon1.SetEnabled(enabled);
}

bool Level::GetHorizonEnabled(int index) const
{
	return GetHorizon(index).GetEnabled();
}

GAME_OBJECT_ID Level::GetHorizonObjectID(int index) const
{
	return GetHorizon(index).GetObjectID();
}

float Level::GetHorizonTransparency(int index) const
{
	return GetHorizon(index).GetTransparency();
}

Vector3 Level::GetHorizonPosition(int index) const
{
	return GetHorizon(index).GetPosition().ToVector3();
}

EulerAngles Level::GetHorizonOrientation(int index) const
{
	return GetHorizon(index).GetRotation().ToEulerAngles();
}

Vector3 Level::GetHorizonPrevPosition(int index) const
{
	return GetHorizon(index).GetPrevPosition().ToVector3();
}

EulerAngles Level::GetHorizonPrevOrientation(int index) const
{
	return GetHorizon(index).GetPrevRotation().ToEulerAngles();
}

bool Level::GetLensFlareEnabled() const
{
	return LensFlare.GetEnabled();
}

int Level::GetLensFlareSunSpriteID() const
{
	return LensFlare.GetSunSpriteID();
}

short Level::GetLensFlarePitch() const
{
	return ANGLE(LensFlare.GetPitch());
}

short Level::GetLensFlareYaw() const
{
	return ANGLE(LensFlare.GetYaw());
}

Color Level::GetLensFlareColor() const
{
	return LensFlare.GetColor();
}

int Level::GetStarfieldStarCount() const
{
	return Starfield.GetStarCount();
}

int Level::GetStarfieldMeteorCount() const
{
	return Starfield.GetMeteorCount();
}

int Level::GetStarfieldMeteorSpawnDensity() const
{
	return Starfield.GetMeteorSpawnDensity();
}

float Level::GetStarfieldMeteorVelocity() const
{
	return Starfield.GetMeteorVelocity();
}
