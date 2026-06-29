-- ldignore
-- Translatable strings for the PhotoMode module.
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
    pm_accessory      = {"Accessory"},
    pm_gunflash       = {"Gun Flash"},

    -- Effects menu items (Lens)
    pm_fov            = {"FOV"},
    pm_roll           = {"Roll"},

    -- Effects menu items (Filters)
    pm_preset         = {"Filter Type"},
    pm_strength       = {"Filter Strength"},
    pm_tint           = {"Filter Tint"},
    pm_tint_intensity = {"Tint Intensity"},

    -- Effects menu items (Frames)
    pm_frame_overlay  = {"Frame"},

    -- Effects menu items (Depth of Field)
    pm_dof_mode       = {"Depth of Field"},
    pm_dof_focus      = {"Focus Distance"},
    pm_dof_range      = {"Focus Range"},
    pm_dof_strength   = {"Blur Strength"},

    -- Light menu items
    pm_enabled        = {"Enabled"},
    pm_source         = {"Position"},
    pm_radius         = {"Radius"},
    pm_color          = {"Color"},
    pm_intensity      = {"Intensity"},
    pm_place_light    = {"Place Light At"},

    -- UI menu items
    pm_hide_ui        = {"Hide UI"},
    pm_hide_character = {"Hide Character"},
    pm_screenshot     = {"Take Photo"},
    pm_exit           = {"Exit Photo Mode"},

    -- Common
    pm_reset          = {"Reset"},
    pm_press          = {"Accept"},

    -- Display strings
    pm_mode_prefix       = {"Mode: "},
    pm_screenshot_saved  = {"Saved to "},
    pm_screenshot_failed = {"Failed to save "},

    -- Per-mode control hints (line 1 of the help bar)
    pm_help_camera            = {"WASD: Move  Mouse: Look  RMB+Mouse: Raise/Lower"},
    pm_help_character         = {"WASD or LMB+Mouse: Move  Mouse: Rotate  RMB+Mouse: Raise/Lower"},
    pm_help_light             = {"WASD or LMB+Mouse: Move  RMB+Mouse: Raise/Lower"},
    pm_help_camera_gamepad    = {"LStick: Move  RStick: Look  RT+RStick: Raise/Lower"},
    pm_help_character_gamepad = {"LStick: Move  RStick: Rotate  RT+RStick: Raise/Lower"},
    pm_help_light_gamepad     = {"LStick: Move RT+RStick: Raise/Lower"},
}

strings.pm_help_nav           = {"Q/E: Switch Tab  Look: " .. strings.pm_hide_ui[1] .. "  Draw/F12: " .. strings.pm_screenshot[1] .. "  Inventory: Exit"}
strings.pm_help_nav_gamepad   = {"LS/RS: Switch Tab  Y: " .. strings.pm_hide_ui[1] .. "  LT: " .. strings.pm_screenshot[1] .. "  Inventory: Exit"}

TEN.Flow.SetStrings(strings)
