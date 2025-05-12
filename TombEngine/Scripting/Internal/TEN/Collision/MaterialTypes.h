#pragma once

#include "Game/collision/floordata.h"

/// Constants for material types.
// @enum Collision.MaterialType
// @pragma nostrip

/// Table of MaterialType constants.
// <br>
// Corresponds to Tomb Editor texture sound material types.
// To be used with @{Collision.Probe.GetFloorMaterialType} and @{Collision.Probe.GetCeilingMaterialType}.
//
// - `MUD`
// - `SNOW`
// - `SNOW`
// - `SAND`
// - `GRAVEL`
// - `ICE`
// - `WATER`
// - `STONE`
// - `WOOD`
// - `METAL`
// - `MARBLE`
// - `GRASS`
// - `CONCRETE`
// - `OLD_WOOD`
// - `OLD_METAL`
// - `CUSTOM_1`
// - `CUSTOM_2`
// - `CUSTOM_3`
// - `CUSTOM_4`
// - `CUSTOM_5`
// - `CUSTOM_6`
// - `CUSTOM_7`
// - `CUSTOM_8`
//
// @table Collision.MaterialType

namespace TEN::Scripting::Collision
{
	static const auto MATERIAL_TYPES = std::unordered_map<std::string, MaterialType>
	{
        { "MUD", MaterialType::Mud },
        { "SNOW", MaterialType::Snow },
        { "SAND", MaterialType::Sand },
        { "GRAVEL", MaterialType::Gravel },
        { "ICE", MaterialType::Ice },
        { "WATER", MaterialType::Water },
        { "STONE", MaterialType::Stone },
        { "WOOD", MaterialType::Wood },
        { "METAL", MaterialType::Metal },
        { "MARBLE", MaterialType::Marble },
        { "GRASS", MaterialType::Grass },
        { "CONCRETE", MaterialType::Concrete },
        { "OLD_WOOD", MaterialType::OldWood },
        { "OLD_METAL", MaterialType::OldMetal },
        { "CUSTOM_1", MaterialType::Custom1 },
        { "CUSTOM_2", MaterialType::Custom2 },
        { "CUSTOM_3", MaterialType::Custom3 },
        { "CUSTOM_4", MaterialType::Custom4 },
        { "CUSTOM_5", MaterialType::Custom5 },
        { "CUSTOM_6", MaterialType::Custom6 },
        { "CUSTOM_7", MaterialType::Custom7 },
        { "CUSTOM_8", MaterialType::Custom8 }
	};
}
