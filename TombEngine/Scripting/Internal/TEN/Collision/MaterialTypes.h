#pragma once

#include "Game/collision/floordata.h"

namespace TEN::Scripting::Collision
{
    /// Constants for material types.
    // Corresponds to Tomb Editor texture sound material types.
    // To be used with @{Collision.Probe.GetFloorMaterialType} and @{Collision.Probe.GetCeilingMaterialType}.
    // @enum Collision.MaterialType
    // @pragma nostrip

    static const auto MATERIAL_TYPES = std::unordered_map<std::string, MaterialType>
    {
        /// Mud material type.
        // @mem MUD
        { "MUD", MaterialType::Mud },

        /// Snow material type.
        // @mem SNOW
        { "SNOW", MaterialType::Snow },

        /// Sand material type.
        // @mem SAND
        { "SAND", MaterialType::Sand },

        /// Gravel material type.
        // @mem GRAVEL
        { "GRAVEL", MaterialType::Gravel },

        /// Ice material type.
        // @mem ICE
        { "ICE", MaterialType::Ice },

        /// Water material type.
        // @mem WATER
        { "WATER", MaterialType::Water },

        /// Stone material type.
        // @mem STONE
        { "STONE", MaterialType::Stone },

        /// Wood material type.
        // @mem WOOD
        { "WOOD", MaterialType::Wood },

        /// Metal material type.
        // @mem METAL
        { "METAL", MaterialType::Metal },

        /// Marble material type.
        // @mem MARBLE
        { "MARBLE", MaterialType::Marble },

        /// Grass material type.
        // @mem GRASS
        { "GRASS", MaterialType::Grass },

        /// Concrete material type.
        // @mem CONCRETE
        { "CONCRETE", MaterialType::Concrete },

        /// Old wood material type.
        // @mem OLD_WOOD
        { "OLD_WOOD", MaterialType::OldWood },

        /// Old metal material type.
        // @mem OLD_METAL
        { "OLD_METAL", MaterialType::OldMetal },

        /// Custom material type 1.
        // @mem CUSTOM_1
        { "CUSTOM_1", MaterialType::Custom1 },

        /// Custom material type 2.
        // @mem CUSTOM_2
        { "CUSTOM_2", MaterialType::Custom2 },

        /// Custom material type 3.
        // @mem CUSTOM_3
        { "CUSTOM_3", MaterialType::Custom3 },

        /// Custom material type 4.
        // @mem CUSTOM_4
        { "CUSTOM_4", MaterialType::Custom4 },

        /// Custom material type 5.
        // @mem CUSTOM_5
        { "CUSTOM_5", MaterialType::Custom5 },

        /// Custom material type 6.
        // @mem CUSTOM_6
        { "CUSTOM_6", MaterialType::Custom6 },

        /// Custom material type 7.
        // @mem CUSTOM_7
        { "CUSTOM_7", MaterialType::Custom7 },

        /// Custom material type 8.
        // @mem CUSTOM_8
        { "CUSTOM_8", MaterialType::Custom8 }
    };
}
