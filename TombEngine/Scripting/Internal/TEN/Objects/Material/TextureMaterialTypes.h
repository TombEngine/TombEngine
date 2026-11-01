#pragma once

#include "Renderer/RendererEnums.h"

/// Constants for texture material types.
// Corresponds to Tomb Editor material shader types.
// To be used with @{Objects.Material.GetType}.
// @enum Objects.TextureMaterialType
// @pragma nostrip

static const auto TEXTURE_MATERIAL_TYPES = std::unordered_map<std::string, TextureMaterialType>
{
	/// Default material.
	// @mem DEFAULT
	{ "DEFAULT", TextureMaterialType::Default },

	/// Reflective material using legacy environment reflections, similar to savegame crystal or Midas golden effect in TR1.
	// @mem REFLECTIVE
	{ "REFLECTIVE", TextureMaterialType::Reflective },

	/// Reflective material using skybox environment reflections.
	// @mem SKYBOX_REFLECTIVE
	{ "SKYBOX_REFLECTIVE", TextureMaterialType::SkyboxReflective }
};