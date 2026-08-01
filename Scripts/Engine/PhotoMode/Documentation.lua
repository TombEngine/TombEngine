---
-- An in-game photo mode for Tomb Engine.
--
-- The player freezes the game, moves a free camera, adjusts Lara's pose and
-- appearance, sets up a light, applies post-process filters, and can overlay
-- frame sprites before taking an in-game screenshot.
--
-- To enable photo mode in a level, `require` this module once in your level script or in Autoexec.lua:
--	local PhotoMode = require("Engine.PhotoMode.PhotoMode")
-- The module self-registers via callbacks, so no further scripting is needed.
--
-- Make sure the following objects exist in your wad:
--
-- - `PHOTOMODE_SPRITES` — Sprite sheet for menus.
-- - `PHOTOMODE_FRAMES`  — Frame overlay sprite sheet.
-- - `PHOTOMODE_ANIMS`   — Object that holds all custom photo mode poses.
-- - `CAMERA_TARGET`     — Used internally to drive the object camera.
-- 
-- It is possible to change settings on a per-level basis via @{PhotoMode.GetSettings} and @{PhotoMode.SetSettings} functions, but keep in mind that
-- _Settings.lua is reread every time the level is reloaded_. Therefore, you need to implement custom settings management in your level script
-- if you want to override global settings.
--
-- <h3><b>Controls</b></h3>
-- The active header tab (cycle with Q / E or shoulder buttons) determines which
-- subject (camera, character or light) the movement controls apply to. The controls remain functional even when the UI is hidden.<br><br>
--
-- <table class="function_list">
-- <tr>
--   <th>Action</th>
--   <th>Keyboard</th>
--   <th>Controller</th>
-- </tr>
-- <tr>
--   <td>Enter Photo Mode</td>
--   <td>F3</td>
--   <td>Press Left Stick + Right Stick</td>
-- </tr>
-- <tr>
--   <td>Move forward / back</td>
--   <td>W / S</td>
--   <td>Left stick Y</td>
-- </tr>
-- <tr>
--   <td>Strafe left / right</td>
--   <td>A / D</td>
--   <td>Left stick X</td>
-- </tr>
-- <tr>
--   <td>Rotate camera view</td>
--   <td>Mouse (move freely)</td>
--   <td>Right stick</td>
-- </tr>
-- <tr>
--   <td>Rotate character Y</td>
--   <td>Mouse X</td>
--   <td>Right stick X</td>
-- </tr>
-- <tr>
--   <td>Move subject horizontally</td>
--   <td>Hold LMB + Mouse (move freely)</td>
--   <td>Hold RT + Right stick Y</td>
-- </tr>
-- <tr>
--   <td>Move subject vertically</td>
--   <td>Hold RMB + Mouse Y</td>
--   <td>Hold RT + Right stick Y</td>
-- </tr>
-- <tr>
--   <td>Navigate menu items</td>
--   <td>Up / Down</td>
--   <td>D-pad </td>
-- </tr>
-- <tr>
--   <td>Change option value</td>
--   <td>Left / Right</td>
--   <td>D-pad Left / Right</td>
-- </tr>
-- <tr>
--   <td>Confirm (Accept)</td>
--   <td>Enter / Action</td>
--   <td>Cross/A</td>
-- </tr>
-- <tr>
--   <td>Switch tabs</td>
--   <td>Q / E</td>
--   <td>LS / RS</td>
-- </tr>
-- <tr>
--   <td>Hide UI</td>
--   <td>Look (NumPad 0) / F12</td>
--   <td>Triangle / Y</td>
-- </tr>
-- <tr>
--   <td>Take Photo</td>
--   <td>Draw</td>
--   <td>LT</td>
-- </tr>
-- <tr>
--   <td>Exit Photo Mode</td>
--   <td>Inventory (Escape)</td>
--   <td>Circle / B</td>
-- </tr>
-- </table>
--
-- <h3><b>Custom Setup</b></h3>
-- To override default photo mode content, such as poses, outfits, expressions, accessories and frames, custom setup
-- may be defined in setup file: `Scripts/PhotoModeSetup.lua`. Below is a description of each section that is available in this file.
--
-- @luautil PhotoMode

--- Adding Accessories
-- @section AddingAccessories
-- Accessories are drawn on top of Lara. They are defined in <i>Scripts/PhotoModeData.lua</i> file in Accessories table. To add new accessories add a row to the accessory presets table in that file. The first entry should always be a "None" sentinel (objID = nil, meshIndices = {}) so the player can clear accessories.
-- To hide the Accessory option entirely set Settings.Character.accessoriesEnabled = false via @{PhotoMode.SetSettings}.
-- @usage
-- PhotoModeSetup.Accessories =
-- {
--    {
--      name = "Sunglasses",
--      objID       = TEN.Objects.ObjID.ACTOR1_SPEECH_HEAD1,
--      meshIndices = { 14 }, 
--    },
--    { 
--      name = "Beret",
--      objID       = TEN.Objects.ObjID.ANIMATING5,
--      meshIndices = { 14 },
--    }
--}

--- Display name shown in the selector.
-- @tfield string name Display name for this acccessory.

--- The object to source the accessory meshes from.
-- @tfield Objects.ObjID objID Object that provides the accessory meshes.

--- Mesh slot indices on the accessory moveable to make visible.
-- @tfield table meshIndices Array of mesh slot indices to display on the accessory moveable.
-- @usage
-- meshIndices = { 0, 4, 9 } — only the listed slot indices are shown

--- Optional function called after the accessory is applied.
-- @tfield[opt=nil] function onEnter Hook function executed after applying this accessory.
-- @usage
-- onEnter = function() -- Function to call when the accessory is selected.
--      TEN.Sound.PlaySound(42)
-- end

--- Whether the accessory is visible in the selector menu.
-- @tfield[opt=true] bool unlocked true or nil makes the accessory visible; false hides it in selection until @{PhotoMode.UnlockAccessory} is called.

--- Adding Expressions
-- @section AddingExpressions
-- Expressions swap one or more of Lara's classic mesh slots with meshes sourced from another object. They are defined in <i>Scripts/PhotoModeData.lua</i> file in Expressions table. Use multiple indices to swap more than one mesh at once.
-- @usage
-- PhotoModeSetup.Expressions =
-- {
--    {
--      name = "Scream",
--      objID       = TEN.Objects.ObjID.LARA_SCREAM,
--      meshIndices = { 14 },   -- head mesh
--    }
-- }

--- Display name shown in the selector.
-- @tfield string name Display name for this expression.

--- The object to source swapped meshes from (nil restores Lara's default expression).
-- @tfield Objects.ObjID objID Object to source the swapped meshes from.

--- Mesh slot indices on Lara to swap.
-- @tfield table meshIndices table of mesh slot indices to swap. Use multiple indices to swap more than one mesh at once.
-- @usage
-- meshIndices = { 0, 4, 9 } — only the listed slot indices are swapped

--- Optional function called after the expression is applied.
-- @tfield[opt=nil] function onEnter Hook function executed after applying this expression.
-- @usage
-- onEnter = function() -- Function to call when the expression is selected.
--      TEN.Sound.PlaySound(42)
-- end

--- Whether the expression is visible in the selector menu.
-- @tfield[opt=true] bool unlocked true or nil makes the expression visible; false hides it in selection until @{PhotoMode.UnlockExpression} is called.

--- Adding Frames
-- @section AddingFrames
-- Frames are full-screen sprites drawn from the PHOTOMODE_FRAMES object. A spriteID of -1 means "no frame". The first entry should always be "None". They are defined in <i>Scripts/PhotoModeData.lua</i> file in Frames table.
-- @usage
-- PhotoModeSetup.Frames =
-- {
--    { name = "My Custom Frame", spriteID = 6, scaleMode = TEN.View.ScaleMode.FIT },
-- }

--- Display name shown in the Frames selector.
-- @tfield string name Display name for this frame overlay preset.

--- Sprite index in the PHOTOMODE_FRAMES object (-1 for no frame).
-- @tfield int spriteID Sprite index from PHOTOMODE_FRAMES. Use -1 for no overlay.

--- Scale mode to set for the frame overlay.
-- @tfield[opt=TEN.View.ScaleMode.STRETCH] View.ScaleMode scaleMode Scale mode for the frame overlay.

--- Optional function called after the frame is selected.
-- @tfield[opt=nil] function onEnter Hook function executed when this frame overlay is selected.
-- @usage
-- onEnter = function() -- Function to call when the frame is selected.
--      TEN.Sound.PlaySound(42)
-- end

--- Whether the frame is visible in the selector menu.
-- @tfield[opt=true] bool unlocked true or nil makes the frame visible; false hides it in selection until @{PhotoMode.UnlockFrame} is called.

--- Adding Outfits
-- @section AddingOutfits
-- Outfits can change Lara's Outfit. Both the classic skins and skinned mesh can be used.  They are defined in <i>Scripts/PhotoModeData.lua</i> file in Outfits table. The first entry is always "Default". Set unlocked = false to hide an outfit until the player earns it, then call @{PhotoMode.UnlockOutfit} to reveal it. Unlocks are saved in GlobalVars.Engine.PhotoMode.Outfits.
-- @usage
-- PhotoModeSetup.Outfits =
-- {
-- -- Classic skin swap (uses Lara:SetSkin):
--
--    { 
--      name = "Classic TR4",
--      skin = 
--          {
--             TEN.Objects.ObjID.ANIMATING1,   -- skin
--             TEN.Objects.ObjID.ANIMATING2,   -- skinJoints
--             TEN.Objects.ObjID.ANIMATING3,   -- skinScream
--             TEN.Objects.ObjID.ANIMATING4,   -- hair1
--             -- hair2 omitted - unchanged
--          },
--      meshVisible = "all",
--    },
--
-- -- Skinned mesh swap (uses Lara:SwapSkinnedMesh):
--
--    {
--       name             = "Remastered",
--       skinnedMesh      = TEN.Objects.ObjID.ANIMATING14,
--       skinnedMeshIndex = 0,      -- Optional sub-index.
--       meshVisible      = "none", -- Hide classic meshes
--       unlocked         = false,
--       onEnter = function() -- Function to call when outfit is selected.
--         local s = TEN.Flow.GetSettings()
--         s.Hair[1].offset = Vec3(-4, 3, -28)
--         TEN.Flow.SetSettings(s)
--       end
--    }
-- }

--- Display name shown in the selector.
-- @tfield string name Display name for this outfit.

--- Array of up to 5 ObjIDs for classic skin swap via Lara:SetSkin().
-- @tfield[opt=nil] table skin Array of up to 5 ObjIDs: skin, skinJoints, skinScream, hair1, hair2. Nil entries leave that slot unchanged.
-- @usage
--      skin = 
--          {
--             TEN.Objects.ObjID.ANIMATING1,   -- skin
--             TEN.Objects.ObjID.ANIMATING2,   -- skinJoints
--             TEN.Objects.ObjID.ANIMATING3,   -- skinScream
--             TEN.Objects.ObjID.ANIMATING4,   -- hair1
--          },

--- ObjID for skinned mesh swap, or the string "clear" to disable GPU skinning.
-- @tfield[opt=nil] Objects.ObjID skinnedMesh ObjID passed to Lara:SwapSkinnedMesh(), or "clear" to call Lara:ClearSkinnedMesh().
-- @usage
-- skinnedMesh = TEN.Objects.ObjID.ANIMATING14,

--- Optional sub-index passed to Lara:SwapSkinnedMesh().
-- @tfield[opt=nil] int skinnedMeshIndex Optional sub-index for SwapSkinnedMesh.
-- @usage
-- skinnedMeshIndex = 0, -- Optional sub-index.

--- Controls classic mesh visibility: "all", "none", or a table of visible slot indices.
-- @tfield[opt=nil] string|table meshVisible "all" keeps all meshes visible, "none" hides all, or a table of indices keeps only those slots visible.
--
-- @usage
-- - "all"       -- All classic mesh slots remain visible.
-- - "none"      -- All classic mesh slots are hidden.
-- - { 0, 4, 9 } -- Only the listed slot indices stay visible; rest are hidden.

--- Optional function called after the outfit is applied.
-- @tfield[opt=nil] function onEnter Hook function executed after applying this outfit. Can be used to change hair offsets.
-- @usage
-- onEnter = function() -- Function to call when outfit is selected.
--      local s = TEN.Flow.GetSettings()
--      s.Hair[1].offset = Vec3(-4, 3, -28)
--      TEN.Flow.SetSettings(s)
-- end
--

--- Whether the outfit is visible in the selector menu.
-- @tfield[opt=true] bool unlocked true or nil makes the outfit visible; false hides it in selection until @{PhotoMode.UnlockOutfit} is called.

--- Adding Poses
-- @section AddingPoses
-- Poses are defined in the <i>Scripts/PhotoModeData.lua</i> file in Poses table. Each pose applies an animation from the PHOTOMODE_ANIMS object or any other object in the level.
-- @usage
-- PhotoModeSetup.Poses =
-- {
--    { 
--      name = "Victory",
--      objID       = TEN.Objects.ObjID.PHOTOMODE_ANIMS,
--      animNumber  = 42, -- Animation slot inside PHOTOMODE_ANIMS.
--      frameNumber = 0,  -- Starting frame (0 = first frame).
--    },
--
-- -- You can also reference any other object's animations.
--
--    { 
--      name = "Running",
--      objID       = TEN.Objects.ObjID.LARA,
--      animNumber  = 17,
--      frameNumber = 0,
--    },
-- }

--- Display name shown in the selector.
-- @tfield string name Display name for this pose.

--- The object that contains the animation to apply for this pose.
-- @tfield Objects.ObjID objID The object that contains the animation to apply for this pose.

--- The animation slot number inside that object (0-based).
-- @tfield int animNumber The animation slot number inside that object (0-based).

--- The starting frame number for that animation (0-based).
-- @tfield int frameNumber The frame number for that animation to set as pose (0-based).

--- Optional function called after the pose is applied.
-- @tfield[opt=nil] function onEnter Hook function executed after applying this pose.
-- @usage
-- onEnter = function() -- Function to call when the pose is selected.
--      TEN.Sound.PlaySound(42)
-- end

--- Whether the pose is visible in the selector menu.
-- @tfield[opt=true] bool unlocked true or nil makes the pose visible; false hides it in selection until @{PhotoMode.UnlockPose} is called.

--- Unlocking Content
-- @section UnlockingContent
-- All PhotoMode data types (Accessories, Expressions, Frames, Poses and Outfits) support per-entry unlocking.
-- Set <code>unlocked = false</code> on an entry to hide it until the matching <code>Unlock*</code> function is called.
-- Unlocks persist per type in GlobalVars.Engine.PhotoMode (e.g. <code>GlobalVars.Engine.PhotoMode.Outfits</code>) and remain unlocked in all levels.
-- To remove all unlocks at once use @{PhotoMode.ClearData}.

--- Clear all unlocked PhotoMode content so it no longer appears in the selectors.
-- @function PhotoMode.ClearData
-- @usage
-- PhotoMode.ClearData()

--- Unlock a named accessory so it appears in the PhotoMode accessory selector. The accessory remains unlocked in all levels.
-- @function PhotoMode.UnlockAccessory
-- @tparam string name The accessory name string as defined in Scripts/PhotoModeSetup.lua file in Accessories table.
-- @usage
-- PhotoMode.UnlockAccessory("Sunglasses")

--- Unlock a named expression so it appears in the PhotoMode expression selector. The expression remains unlocked in all levels.
-- @function PhotoMode.UnlockExpression
-- @tparam string name The expression name string as defined in Scripts/PhotoModeSetup.lua file in Expressions table.
-- @usage
-- PhotoMode.UnlockExpression("Scream")

--- Unlock a named frame so it appears in the PhotoMode frame overlay selector. The frame remains unlocked in all levels.
-- @function PhotoMode.UnlockFrame
-- @tparam string name The frame name string as defined in Scripts/PhotoModeSetup.lua file in Frames table.
-- @usage
-- PhotoMode.UnlockFrame("Recording")

--- Unlock a named outfit so it appears in the PhotoMode outfit selector. The outfit remains unlocked in all levels.
-- @function PhotoMode.UnlockOutfit
-- @tparam string name The outfit name string as defined in Scripts/PhotoModeData.lua file in Outfits table.
-- @usage
-- PhotoMode.UnlockOutfit("Secret Wetsuit")

--- Unlock a named pose so it appears in the PhotoMode pose selector. The pose remains unlocked in all levels.
-- @function PhotoMode.UnlockPose
-- @tparam string name The pose name string as defined in Scripts/PhotoModeSetup.lua file in Poses table.
-- @usage
-- PhotoMode.UnlockPose("Victory")

--- Settings
-- @section Settings
-- Settings can be managed by the two functions @{PhotoMode.GetSettings} and @{PhotoMode.SetSettings}. See those functions for details.

--- Set settings tables for PhotoMode.
-- @function PhotoMode.SetSettings
-- @tparam Settings newSettings Required settings table
-- @usage
-- -- In the level's lua file
-- local settings = PhotoMode.GetSettings()
-- settings.Character.accessoriesEnabled = false
-- PhotoMode.SetSettings(settings)

--- Get settings tables for PhotoMode.
-- @function PhotoMode.GetSettings
-- @treturn Settings Current settings table

--- Settings.Camera
-- @section Settings.Camera
-- Camera-related settings.
-- @usage
-- -- In the level's lua file
-- local settings = PhotoMode.GetSettings()
-- settings.Camera.limitCameraDistance = false
-- PhotoMode.SetSettings(settings)

--- Whether to limit camera distance from Lara to prevent clipping through level geometry. Enabling this will cause the camera to stop moving further away once it reaches the configured distance, but it will not push the camera back if the player moves closer after exceeding the limit.
-- @tfield[opt=true] bool limitCameraDistance true to enable camera distance limiting, false to allow unlimited camera distance.

--- Maximum camera distance from Lara when Settings.Camera.limitCameraDistance is enabled. The camera will stop moving further away once it reaches this distance.
-- @tfield[opt=4096] int distance Maximum camera distance from Lara when Settings.Camera.limitCameraDistance is enabled, measured in game units.

--- All four DOF items (Mode, Focus Distance, Focus Range, Blur Strength) can be hidden at once by setting depthOfFieldEnabled = false. This is useful for projects that do not use the DOF post-process effect and want a cleaner Camera menu.
-- @tfield[opt=true] bool depthOfFieldEnabled true to enable depth of field, false to disable it.

--- Settings.Character
-- @section Settings.Character
-- Settings related to character customization.
-- @usage
-- -- In the level's lua file
-- local settings = PhotoMode.GetSettings()
-- settings.Character.weaponsUnlocked = false
-- PhotoMode.SetSettings(settings)

--- Whether the Accessories menu is enabled. Accessories are mesh swaps parented to a hidden moveable that mirrors Lara's animation, allowing them to move naturally with her skeleton. Set this to false to hide the Accessories menu and disable the accessory system entirely.
-- @tfield[opt=true] bool accessoriesEnabled true to enable the Accessories menu, false to hide it and disable the accessory option in Character menu.

--- Whether the Outfits menu is enabled.
-- @tfield[opt=true] bool outfitsEnabled true to enable the Outfits menu, false to hide it and disable the outfit option in Character menu.

--- Whether to show all weapon options in the Character menu regardless of inventory.
-- @tfield[opt=true] bool weaponsUnlocked set this to false to hide unavailable weapons.

--- Settings.ColorMap
-- @section Settings.ColorMap
-- These settings define the colors used throughout the PhotoMode UI.
-- Colors are of type @{Color}.
-- @usage
-- -- Example of changing the text color
-- -- In the level's lua file
-- local settings = PhotoMode.GetSettings()
-- settings.ColorMap.plainTextColor = TEN.Color(200, 180, 60, 255)
-- PhotoMode.SetSettings(settings)

--- Color used for standard body text in the PhotoMode UI.
-- @tfield[opt=Flow.GetSettings().UI.plainTextColor] Color plainText Applied to descriptive text.

--- Color used for section headers and titles.
-- @tfield[opt=Flow.GetSettings().UI.headerTextColor] Color headerText Applied to PhotoMode category headings and titles.

--- Color used for selectable option text.
-- @tfield[opt=Flow.GetSettings().UI.optionTextColor] Color optionText Applied to text entries.

--- Color used to render the neutral sprites.
-- @tfield[opt=Color(255&#44; 255&#44; 255&#44; 255)] Color neutral Tint applied to the sprites.

--- Color used to render the dimmed sprites.
-- @tfield[opt=Color(120&#44; 120&#44; 120&#44; 255)] Color dimmed Tint applied to the sprites.


--- Settings.SoundMap
-- @section Settings.SoundMap
-- These settings map PhotoMode UI events to sound effect IDs.
-- Sound IDs correspond to entries in the game's sound catalogue.
-- @usage
-- -- Example of overriding the PhotoMode open sound
-- -- In the level's lua file
-- local settings = PhotoMode.GetSettings()
-- settings.SoundMap.menuOpen = 42
-- PhotoMode.SetSettings(settings)

--- Sound played when changing the PhotoMode tabs.
-- @tfield[opt=108] int menuRotate Sound effect ID triggered while scrolling through PhotoMode.

--- Sound played when hovering over or highlighting a menu option.
-- @tfield[opt=109] int menuSelect Sound effect ID triggered on item selection highlight.

--- Sound played when confirming a menu choice.
-- @tfield[opt=111] int menuChoose Sound effect ID triggered when the player confirms a selected action.

--- Sound played when the PhotoMode is opened.
-- @tfield[opt=109] int menuOpen Sound effect ID triggered when the PhotoMode is opened.

--- Sound played when the PhotoMode is closed.
-- @tfield[opt=109] int menuClose Sound effect ID triggered when the PhotoMode is closed.

--- Sound played when a photo is taken.
-- @tfield[opt=111] int takePhoto Sound effect ID triggered when a photo is taken.
