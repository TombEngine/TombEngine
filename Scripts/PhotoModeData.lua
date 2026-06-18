local PhotoModeData = {}

PhotoModeData.Accessories =
{
--   Each entry describes an Accessory.
--   name        : Display name shown in the menu.
--   objID       : Source object to copy meshes from.
--   meshIndices : Which mesh slots are swapped from objID.
--
-- { name = "Rose",        objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, meshIndices = {10} }
}

PhotoModeData.Expressions =
{
--   Each entry describes an expression.
--   name        : Display name shown in the menu.
--   objID       : Source object to copy meshes from.
--   meshIndices : Which mesh slots are swapped from objID.
-- { name = "Talk 1", objID = TEN.Objects.ObjID.LARA_SPEECH_HEAD1, meshIndices = {14} },
}

PhotoModeData.Frames =
{
--  Each entry describes a frame.
--   name        : Display name shown in the menu.
--   spriteID    : Which sprite index in PHOTOMODE_FRAMES to use. 
-- { name = "Recording", spriteID = 3 },
}

PhotoModeData.Outfits =
{
--   Each entry describes an outfit.
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
--   Each entry describes a pose.
--   name        : Display name shown in the menu.
--   objID       : Source object to use for the pose.
--   animNumber  : Which animation id to use for the pose.
--   frameNumber : Which frame to use from the animation.
--   { name = "Pose Name",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 42, frameNumber = 0 },
}

return PhotoModeData