#pragma once

#include "Renderer/VolumetricCloud/VolumetricCloud.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"

namespace sol { class state; }

namespace TEN::Scripting
{
	using namespace TEN::Renderer::VolumetricCloud;
	using namespace TEN::Scripting::Types;

	/// Describes a volumetric cloud layer. To be used with Flow.Level.layer1 or Flow.Level.layer2.
	/// When assigned, replaces the legacy bitmap sky layer with full volumetric clouds.
	class VolumetricCloudLayer
	{
	public:
		static void Register(sol::table& parent);

		// Fields exposed to Lua.
		CloudRenderSettings Settings = {};

		// Constructors.
		VolumetricCloudLayer() = default;
		VolumetricCloudLayer(sol::table settingsTable);

		// Getters.
		bool        GetEnabled() const;
		float       GetCoverage() const;
		float       GetDensity() const;
		float       GetWindSpeed() const;
		float       GetBottomHeight() const;
		float       GetThickness() const;
		float       GetEvolutionSpeed() const;
		float       GetShapeScale() const;
		float       GetDetailScale() const;
		float       GetDetailStrength() const;
		float       GetAbsorption() const;
		float       GetAmbient() const;
		float       GetSilverlining() const;
		std::string GetQuality() const;

		// Setters.
		void SetCoverage(float val);
		void SetDensity(float val);
		void SetWindSpeed(float val);
		void SetBottomHeight(float val);
		void SetThickness(float val);
		void SetEvolutionSpeed(float val);
		void SetShapeScale(float val);
		void SetDetailScale(float val);
		void SetDetailStrength(float val);
		void SetAbsorption(float val);
		void SetAmbient(float val);
		void SetSilverlining(float val);
		void SetQuality(const std::string& preset);

	private:
		void ParseSettingsTable(sol::table table);
		static CloudQualityPreset ParseQualityString(const std::string& str);
	};
}
