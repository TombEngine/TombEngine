--- Translatable strings for the PhotoMode module.
-- @module Engine.PhotoMode.Strings
-- @local

local strings =
{
    photo_mode = {"Photo Mode"},
    -- Header tabs
    pm_header_character = {"Character"},
    pm_header_effects   = {"Camera"},
    pm_header_filters   = {"Filters"},
    pm_header_light     = {"Light"},
    pm_header_ui        = {"UI"},

    -- Character menu items
    pm_animation      = {"Pose"},
    pm_outfit         = {"Outfit"},
    pm_weapons        = {"Weapons"},
    pm_expression     = {"Expression"},
    pm_sunglasses     = {"Sunglasses"},
    pm_gunflash       = {"Gun Flash"},

    -- Effects menu items (Lens)
    pm_fov            = {"FOV"},
    pm_roll           = {"Roll"},

    -- Effects menu items (Filters)
    pm_preset         = {"Filter Type"},
    pm_strength       = {"Filter Strength"},
    pm_tint           = {"Filter Tint"},

    -- Effects menu items (Frames)
    pm_frame_overlay  = {"Frame"},

    -- Effects menu items (Depth of Field)
    pm_dof_mode       = {"Depth of Field"},
    pm_dof_focus      = {"Focus Distance"},
    pm_dof_range      = {"Focus Range"},
    pm_dof_strength   = {"Blur Strength"},

    -- Light menu items
    pm_enabled        = {"Enabled"},
    pm_source         = {"Source"},
    pm_radius         = {"Radius"},
    pm_color          = {"Color"},
    pm_place_camera   = {"Place at Camera"},
    pm_place_lara     = {"Place at Lara"},

    -- UI menu items
    pm_hide_ui        = {"Hide UI"},
    pm_exit           = {"Exit Photo Mode"},

    -- Common
    pm_reset          = {"Reset"},
    pm_press          = {"Accept"},

    -- Display strings
    pm_mode_prefix    = {"Mode: "},
    pm_help           = {"StepL/R=Tab  Look=Hide UI  Inventory=Exit"},
}

TEN.Flow.SetStrings(strings)
