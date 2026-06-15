-- ldignore
-- Accessory presets for the Photo Mode accessory picker.
-- Each entry describes a mesh swap to apply to the pm_Sunglasses moveable.
--   name        : Display name shown in the menu.
--   objID       : Source object to copy meshes from (nil = no swap / restore default).
--   meshIndices : Which mesh slots on pm_Sunglasses are swapped from objID.
--
-- Add or remove entries freely.  The first entry should always be a "None"
-- sentinel (objID = nil, meshIndices = {}) so the player can clear accessories.

local Accessories =
{
    { name = "None",        objID = nil,                               meshIndices = {}   },
    { name = "Sunglasses",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, meshIndices = {14} },
    { name = "Rose",        objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, meshIndices = {10} },
}

return Accessories
