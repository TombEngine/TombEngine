#pragma once

/// Constants for the type of intersection checks.
// To be used with @{Collision.Ray} class.
// @enum Collision.IntersectionType
// @pragma nostrip

enum class ScriptIntersectionType
{
	None,
	Box,
	BoxAndSphere
};

static const std::unordered_map<std::string, ScriptIntersectionType> INTERSECTION_TYPE
{
	/// No intersection checks.
	// @mem NONE
	{ "NONE", ScriptIntersectionType::None },

	/// Bounding box intersection checks.
	// @mem BOX
	{ "BOX", ScriptIntersectionType::Box },

	/// Bounding box and sphere intersection checks. A check will succeed only when an overlap of a bounding box and a mesh sphere is detected.
	// This type of intersection check is useful for objects that use mesh sphere masking for collision detection.
	// @mem BOX_AND_SPHERE
	{ "BOX_AND_SPHERE", ScriptIntersectionType::BoxAndSphere }
};
