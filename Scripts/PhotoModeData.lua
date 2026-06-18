local PhotoModeData = {}

PhotoModeData.Accessories =
{
-- Accessory presets for the Photo Mode accessory picker.
-- Each entry describes a mesh swap to apply to the pm_Sunglasses moveable.
--   name        : Display name shown in the menu.
--   objID       : Source object to copy meshes from (nil = no swap / restore default).
--   meshIndices : Which mesh slots on pm_Sunglasses are swapped from objID.
--
-- Add or remove entries freely.  The first entry should always be a "None"
-- sentinel (objID = nil, meshIndices = {}) so the player can clear accessories.
-- { name = "Rose",        objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, meshIndices = {10} }

}

PhotoModeData.Expressions =
{

}

PhotoModeData.Frames =
{

}

PhotoModeData.Outfits =
{
-- skin:              Array of up to 5 ObjIDs → Lara:SetSkin(skin, skinJoints, skinScream, hair1, hair2).
--                    Nil entries leave that slot unchanged.
-- skinnedMesh:       ObjID → Lara:SwapSkinnedMesh(objID [, skinnedMeshIndex]).
--                    "clear" → Lara:ClearSkinnedMesh() (disables GPU skin entirely).
-- skinnedMeshIndex:  Optional sub-index for SwapSkinnedMesh.
-- meshVisible:       Controls classic mesh visibility.
--                    "none" or nil → hide all classic meshes.
--                    "all"         → keep all classic meshes visible.
--                    { i, ... }    → keep only listed indices visible, hide the rest.
-- onEnter:           Optional function() called after the outfit is applied.
-- unlocked:          true/nil = outfit is visible in the menu.
--                    false    = hidden until PhotoMode.UnlockOutfit(name) is called.

--   {
--     name = "Classic TR4",
--     skin = {
--             TEN.Objects.ObjID.ANIMATING1, TEN.Objects.ObjID.ANIMATING2,
--             TEN.Objects.ObjID.ANIMATING3, TEN.Objects.ObjID.ANIMATING4
--            },
--     meshVisible = "all",
--   },
--   {
--     name = "TEN Lara",
--     skinnedMesh = TEN.Objects.ObjID.LARA_EXTRA_MESH1,
--     meshVisible = {10, 13},
--     onEnter = 
--     function()
--     local settings = TEN.Flow.GetSettings()
--     settings.Hair[1].offset = Vec3(-4, 3, -28)
--     TEN.Flow.SetSettings(settings)
--     end,
--     unlocked = false
--   },

}

PhotoModeData.Poses =
{

}

return PhotoModeData