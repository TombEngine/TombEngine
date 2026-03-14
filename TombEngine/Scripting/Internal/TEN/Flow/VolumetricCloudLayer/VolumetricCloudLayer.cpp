#include "framework.h"
#include "Scripting/Internal/TEN/Flow/VolumetricCloudLayer/VolumetricCloudLayer.h"

#include <algorithm>
#include "Game/Sky/SkyCloudSystem.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Math/Math.h"

using namespace TEN::Renderer::VolumetricCloud;
using namespace TEN::Scripting::Types;
using namespace TEN::Math::Random;
using namespace TEN::Sky;

namespace TEN::Scripting
{
	/*** Describes a volumetric cloud layer, replacing the legacy flat bitmap sky layer.
	To be used with @{Flow.Level.layer1} and @{Flow.Level.layer2} properties.

	When assigned to a layer property, the engine switches from the old scrolling bitmap layer
	to a full volumetric cloud renderer for that layer.

	@tenprimitive Flow.VolumetricCloudLayer
	@pragma nostrip
	*/

	void VolumetricCloudLayer::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			VolumetricCloudLayer(),
			VolumetricCloudLayer(sol::table)>;

		parent.new_usertype<VolumetricCloudLayer>(
			"VolumetricCloudLayer",
			ctors(),
			sol::call_constructor, ctors(),

			/// (float) Cloud coverage in range [0.0, 1.0]. 0 = clear sky, 1 = full overcast with structure.
			// @mem coverage
			"coverage", sol::property(&VolumetricCloudLayer::GetCoverage, &VolumetricCloudLayer::SetCoverage),

			/// (float) Cloud density multiplier. Default: 0.8.
			// @mem density
			"density", sol::property(&VolumetricCloudLayer::GetDensity, &VolumetricCloudLayer::SetDensity),

			/// (float) Wind speed for bulk cloud advection. Default: 0.003.
			// @mem windSpeed
			"windSpeed", sol::property(&VolumetricCloudLayer::GetWindSpeed, &VolumetricCloudLayer::SetWindSpeed),

			/// (float) Height in world units of the cloud layer bottom above the camera horizon. Default: 8000.
			// @mem bottomHeight
			"bottomHeight", sol::property(&VolumetricCloudLayer::GetBottomHeight, &VolumetricCloudLayer::SetBottomHeight),

			/// (float) Vertical thickness of the cloud layer in world units. Default: 2500.
			// @mem thickness
			"thickness", sol::property(&VolumetricCloudLayer::GetThickness, &VolumetricCloudLayer::SetThickness),

			/// (float) Speed of local cloud evolution (billowing, erosion). Default: 0.15.
			// @mem evolutionSpeed
			"evolutionSpeed", sol::property(&VolumetricCloudLayer::GetEvolutionSpeed, &VolumetricCloudLayer::SetEvolutionSpeed),

			/// (float) Scale of low-frequency shape noise. Default: 0.00008.
			// @mem shapeScale
			"shapeScale", sol::property(&VolumetricCloudLayer::GetShapeScale, &VolumetricCloudLayer::SetShapeScale),

			/// (float) Scale of high-frequency detail erosion noise. Default: 0.0008.
			// @mem detailScale
			"detailScale", sol::property(&VolumetricCloudLayer::GetDetailScale, &VolumetricCloudLayer::SetDetailScale),

			/// (float) Strength of detail noise erosion [0.0, 1.0]. Default: 0.35.
			// @mem detailStrength
			"detailStrength", sol::property(&VolumetricCloudLayer::GetDetailStrength, &VolumetricCloudLayer::SetDetailStrength),

			/// (float) Light absorption factor (Beer-Lambert). Higher = darker/thicker clouds. Default: 1.1.
			// @mem absorption
			"absorption", sol::property(&VolumetricCloudLayer::GetAbsorption, &VolumetricCloudLayer::SetAbsorption),

			/// (float) Ambient sky light contribution [0.0, 1.0]. Default: 0.35.
			// @mem ambient
			"ambient", sol::property(&VolumetricCloudLayer::GetAmbient, &VolumetricCloudLayer::SetAmbient),

			/// (float) Silver lining / forward scattering strength [0.0, 1.0]. Default: 0.4.
			// @mem silverlining
			"silverlining", sol::property(&VolumetricCloudLayer::GetSilverlining, &VolumetricCloudLayer::SetSilverlining),

			/// (string) Quality preset: "Low", "Medium", or "High". Default: "Medium".
			// @mem quality
			"quality", sol::property(&VolumetricCloudLayer::GetQuality, &VolumetricCloudLayer::SetQuality),

			/*** Create a VolumetricCloudLayer pre-configured from a named weather preset.
			@function fromPreset
			@tparam string presetName Preset name: "ClearSky", "Cirrus", "Thunderstorm", etc.
			@tparam[opt] string layer Which cloud layer of the preset to copy: "cloudA" (default) or "cloudB".
			@treturn VolumetricCloudLayer A cloud layer object populated with the preset's values.
			@usage
			level.volumetricLayer1 = Flow.VolumetricCloudLayer.fromPreset("Cirrus")
			level.volumetricLayer1 = Flow.VolumetricCloudLayer.fromPreset("Thunderstorm", "cloudB")
			*/
			"fromPreset", sol::overload(
				[](const std::string& name) { return VolumetricCloudLayer::FromPreset(name); },
				[](const std::string& name, const std::string& lyr) { return VolumetricCloudLayer::FromPreset(name, lyr); }
			)
		);
	}

	/*** Create a VolumetricCloudLayer object.
	@function VolumetricCloudLayer
	@tparam[opt] table settings Optional settings table with fields: coverage, density, windSpeed,
	    bottomHeight, thickness, evolutionSpeed, shapeScale, detailScale, detailStrength,
	    absorption, ambient, silverlining, quality.
	@treturn VolumetricCloudLayer A volumetric cloud layer object.
	*/
	VolumetricCloudLayer::VolumetricCloudLayer(sol::table settingsTable)
	{
		Settings.Enabled = true;
		Settings.Mode = CloudLayerMode::Volumetric;
		ParseSettingsTable(settingsTable);
	}

	VolumetricCloudLayer VolumetricCloudLayer::FromPreset(const std::string& presetName, const std::string& layer)
	{
		auto type = SkyCloudSystem::StringToPresetType(presetName);
		const auto* def = g_SkyCloudSystem.GetPresetDefinition(type);
		if (!def)
			return {};

		const VolumetricCloudLayerSnapshot& snapshot =
			(layer == "cloudB") ? def->TargetState.CloudB : def->TargetState.CloudA;

		VolumetricCloudLayer result;
		result.Settings = snapshot.ToRenderSettings();
		return result;
	}

	// -----------------------------------------------------------------------
	// Table parsing
	// -----------------------------------------------------------------------

	void VolumetricCloudLayer::ParseSettingsTable(sol::table table)
	{
		if (auto val = table.get<sol::optional<float>>("coverage"))
			SetCoverage(*val);
		if (auto val = table.get<sol::optional<float>>("density"))
			SetDensity(*val);
		if (auto val = table.get<sol::optional<float>>("windSpeed"))
			SetWindSpeed(*val);
		if (auto val = table.get<sol::optional<float>>("bottomHeight"))
			SetBottomHeight(*val);
		if (auto val = table.get<sol::optional<float>>("thickness"))
			SetThickness(*val);
		if (auto val = table.get<sol::optional<float>>("evolutionSpeed"))
			SetEvolutionSpeed(*val);
		if (auto val = table.get<sol::optional<float>>("shapeScale"))
			SetShapeScale(*val);
		if (auto val = table.get<sol::optional<float>>("detailScale"))
			SetDetailScale(*val);
		if (auto val = table.get<sol::optional<float>>("detailStrength"))
			SetDetailStrength(*val);
		if (auto val = table.get<sol::optional<float>>("absorption"))
			SetAbsorption(*val);
		if (auto val = table.get<sol::optional<float>>("ambient"))
			SetAmbient(*val);
		if (auto val = table.get<sol::optional<float>>("silverlining"))
			SetSilverlining(*val);
		if (auto val = table.get<sol::optional<std::string>>("quality"))
			SetQuality(*val);

		// Wind direction: accept table {x, y} or Vec2.
		if (auto vecOpt = table.get<sol::optional<sol::table>>("windDirection"))
		{
			auto t = *vecOpt;
			float x = t.get_or(1, 1.0f);
			float y = t.get_or(2, 0.0f);
			float len = std::sqrt(x * x + y * y);
			if (len > 0.0001f)
			{
				Settings.WindDirection.x = x / len;
				Settings.WindDirection.y = y / len;
			}
		}
	}

	CloudQualityPreset VolumetricCloudLayer::ParseQualityString(const std::string& str)
	{
		if (str == "Low" || str == "low")
			return CloudQualityPreset::Low;
		if (str == "High" || str == "high")
			return CloudQualityPreset::High;
		return CloudQualityPreset::Medium;
	}

	// -----------------------------------------------------------------------
	// Getters
	// -----------------------------------------------------------------------

	bool  VolumetricCloudLayer::GetEnabled() const { return Settings.Enabled; }
	float VolumetricCloudLayer::GetCoverage() const { return Settings.Coverage; }
	float VolumetricCloudLayer::GetDensity() const { return Settings.Density; }
	float VolumetricCloudLayer::GetWindSpeed() const { return Settings.WindSpeed; }
	float VolumetricCloudLayer::GetBottomHeight() const { return Settings.CloudBottomHeight; }
	float VolumetricCloudLayer::GetThickness() const { return Settings.CloudThickness; }
	float VolumetricCloudLayer::GetEvolutionSpeed() const { return Settings.EvolutionSpeed; }
	float VolumetricCloudLayer::GetShapeScale() const { return Settings.Noise.ShapeScale; }
	float VolumetricCloudLayer::GetDetailScale() const { return Settings.Noise.DetailScale; }
	float VolumetricCloudLayer::GetDetailStrength() const { return Settings.Noise.DetailStrength; }
	float VolumetricCloudLayer::GetAbsorption() const { return Settings.Absorption; }
	float VolumetricCloudLayer::GetAmbient() const { return Settings.AmbientContrib; }
	float VolumetricCloudLayer::GetSilverlining() const { return Settings.SilverliningStr; }

	std::string VolumetricCloudLayer::GetQuality() const
	{
		switch (Settings.Quality)
		{
		case CloudQualityPreset::Low:  return "Low";
		case CloudQualityPreset::High: return "High";
		default:                       return "Medium";
		}
	}

	// -----------------------------------------------------------------------
	// Setters (with validation / clamping)
	// -----------------------------------------------------------------------

	void VolumetricCloudLayer::SetCoverage(float val)
	{
		Settings.Coverage = std::clamp(val, 0.0f, 1.0f);
	}

	void VolumetricCloudLayer::SetDensity(float val)
	{
		Settings.Density = std::clamp(val, 0.0f, 10.0f);
	}

	void VolumetricCloudLayer::SetWindSpeed(float val)
	{
		Settings.WindSpeed = std::clamp(val, 0.0f, 8.0f);
	}

	void VolumetricCloudLayer::SetBottomHeight(float val)
	{
		Settings.CloudBottomHeight = std::clamp(val, 500.0f, 200000.0f);
	}

	void VolumetricCloudLayer::SetThickness(float val)
	{
		float value = GenerateFloat(val, val + 100000.0f);
		Settings.CloudThickness = std::clamp(value, 100.0f, 200000.0f);
	}

	void VolumetricCloudLayer::SetEvolutionSpeed(float val)
	{
		Settings.EvolutionSpeed = std::clamp(val, 0.0f, 5.0f);
	}

	void VolumetricCloudLayer::SetShapeScale(float val)
	{
		Settings.Noise.ShapeScale = std::clamp(val, 0.0f, 1.0f);
	}

	void VolumetricCloudLayer::SetDetailScale(float val)
	{
		Settings.Noise.DetailScale = std::clamp(val, 0.0f, 1.0f);
	}

	void VolumetricCloudLayer::SetDetailStrength(float val)
	{
		Settings.Noise.DetailStrength = std::clamp(val, 0.0f, 1.0f);
	}

	void VolumetricCloudLayer::SetAbsorption(float val)
	{
		Settings.Absorption = std::clamp(val, 0.0f, 25.0f);
	}

	void VolumetricCloudLayer::SetAmbient(float val)
	{
		Settings.AmbientContrib = std::clamp(val, 0.0f, 1.0f);
	}

	void VolumetricCloudLayer::SetSilverlining(float val)
	{
		Settings.SilverliningStr = std::clamp(val, 0.0f, 1.0f);
	}

	void VolumetricCloudLayer::SetQuality(const std::string& preset)
	{
		Settings.Quality = ParseQualityString(preset);
	}
}
