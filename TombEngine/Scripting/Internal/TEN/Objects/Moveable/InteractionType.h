#pragma once

#include "Game/Hud/InteractionHighlighter.h"
using namespace TEN::Hud;

/// Constants for interaction type.
// To be used with @{Objects.Moveable.ShowInteractionHighlight} and @{Objects.LaraObject.Interact} functions.
// @enum Objects.InteractionType
// @pragma nostrip

static const auto INTERACTION_TYPE = std::unordered_map<std::string, InteractionType>
{
	/// Show PICKUP interaction icon.
	// @mem PICKUP
	{ "PICKUP", InteractionType::Pickup },

	/// Show USE interaction icon.
	// @mem USE
	{ "USE", InteractionType::Use },

	/// Show TALK interaction icon.
	// @mem TALK
	{ "TALK", InteractionType::Talk}
};
