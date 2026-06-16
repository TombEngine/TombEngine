-- ldignore
local Outfits =
{
    -- Index 1: Default — restores whatever state was active on photo mode entry.
  {
    name = "Default"
  },

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

  {
    name = "Classic TR4",
    skin = {
            TEN.Objects.ObjID.ANIMATING1, TEN.Objects.ObjID.ANIMATING2,
            TEN.Objects.ObjID.ANIMATING3, TEN.Objects.ObjID.ANIMATING4
           },
    meshVisible = "all",
  },
  {
    name = "Classic TR2",
    skin = {
            TEN.Objects.ObjID.ANIMATING18, TEN.Objects.ObjID.ANIMATING19,
            TEN.Objects.ObjID.ANIMATING20, TEN.Objects.ObjID.ANIMATING21
           },
    meshVisible = "all",
  },
  {
    name = "Remastered",
    skin = {
            TEN.Objects.ObjID.ANIMATING14, TEN.Objects.ObjID.ANIMATING15,
            TEN.Objects.ObjID.ANIMATING16, TEN.Objects.ObjID.ANIMATING17
           },
    meshVisible = "all",
    onEnter = 
      function()
          local settings = TEN.Flow.GetSettings()
          settings.Hair[1].offset = Vec3(-4, 3, -28)
          TEN.Flow.SetSettings(settings)
      end,
    unlocked = false
  },
  {
    name = "Dark Raider",
    skin = {
            TEN.Objects.ObjID.ANIMATING10, TEN.Objects.ObjID.ANIMATING11,
            TEN.Objects.ObjID.ANIMATING12, TEN.Objects.ObjID.ANIMATING13
           },
    meshVisible = "all",
  },
  {
    name = "Underworld Casual",
    skin = {
            TEN.Objects.ObjID.ANIMATING6, TEN.Objects.ObjID.ANIMATING7,
            TEN.Objects.ObjID.ANIMATING8, TEN.Objects.ObjID.ANIMATING9
           },
    meshVisible = "all",
  },
  {
    name = "TEN Lara",
    skinnedMesh = TEN.Objects.ObjID.LARA_EXTRA_MESH1,
    meshVisible = {10, 13},
  },
  { 
    name = "Jeans",
    skinnedMesh = TEN.Objects.ObjID.LARA_EXTRA_MESH2,
    meshVisible = "none",
  }
}

return Outfits