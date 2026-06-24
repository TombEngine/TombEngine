local Weapons =
{
    {
        name = "Default",
        objID = TEN.Objects.ObjID.LARA_SKIN,
        meshIndices = {},
        weaponType = TEN.Objects.WeaponType.NONE,
        type = "none",
        gunFlash = false
    },
    {
        name = TEN.Flow.GetString("pistols"),
        objID = TEN.Objects.ObjID.PISTOLS_ANIM,
        meshIndices = { 10, 13 },
        weaponType = TEN.Objects.WeaponType.PISTOLS,
        pickupObjID = TEN.Objects.ObjID.PISTOLS_ITEM,
        type = "holsters",
        gunFlash = TEN.Objects.WeaponFlashMode.AUTO
    },
    {
        name = TEN.Flow.GetString("pistols") .. "(Right)",
        objID = TEN.Objects.ObjID.PISTOLS_ANIM,
        meshIndices = { 10 },
        weaponType = TEN.Objects.WeaponType.PISTOLS,
        pickupObjID = TEN.Objects.ObjID.PISTOLS_ITEM,
        type = "right",
        gunFlash = TEN.Objects.WeaponFlashMode.RIGHT
    },
    {
        name = TEN.Flow.GetString("pistols") .. "(Left)",
        objID = TEN.Objects.ObjID.PISTOLS_ANIM,
        meshIndices = { 13 },
        weaponType = TEN.Objects.WeaponType.PISTOLS,
        pickupObjID = TEN.Objects.ObjID.PISTOLS_ITEM,
        type = "left",
        gunFlash = TEN.Objects.WeaponFlashMode.LEFT
    },
    {
        name = TEN.Flow.GetString("shotgun"),
        objID = TEN.Objects.ObjID.SHOTGUN_ANIM,
        meshIndices = { 10 },
        weaponType = TEN.Objects.WeaponType.SHOTGUN,
        pickupObjID = TEN.Objects.ObjID.SHOTGUN_ITEM,
        type = "back",
        gunFlash = false
    },
    {
        name = TEN.Flow.GetString("uzis"),
        objID = TEN.Objects.ObjID.UZI_ANIM,
        meshIndices = { 10, 13 },
        weaponType = TEN.Objects.WeaponType.UZIS,
        pickupObjID = TEN.Objects.ObjID.UZI_ITEM,
        type = "holsters",
        gunFlash = TEN.Objects.WeaponFlashMode.AUTO
    },
    {
        name = TEN.Flow.GetString("revolver"),
        objID = TEN.Objects.ObjID.REVOLVER_ANIM,
        meshIndices = { 10 },
        weaponType = TEN.Objects.WeaponType.REVOLVER,
        pickupObjID = TEN.Objects.ObjID.REVOLVER_ITEM,
        type = "right",
        gunFlash = TEN.Objects.WeaponFlashMode.AUTO
    },
    {
        name = TEN.Flow.GetString("hk"),
        objID = TEN.Objects.ObjID.HK_ANIM,
        meshIndices = { 10 },
        weaponType = TEN.Objects.WeaponType.HK,
        pickupObjID = TEN.Objects.ObjID.HK_ITEM,
        type = "back",
        gunFlash = TEN.Objects.WeaponFlashMode.AUTO
    },
    {
        name = TEN.Flow.GetString("crossbow"),
        objID = TEN.Objects.ObjID.CROSSBOW_ANIM,
        meshIndices = { 10 },
        weaponType = TEN.Objects.WeaponType.CROSSBOW,
        pickupObjID = TEN.Objects.ObjID.CROSSBOW_ITEM,
        type = "back",
        gunFlash = false
    },
    {
        name = TEN.Flow.GetString("harpoon_gun"),
        objID = TEN.Objects.ObjID.HARPOON_ANIM,
        meshIndices = { 10 },
        weaponType = TEN.Objects.WeaponType.HARPOON_GUN,
        pickupObjID = TEN.Objects.ObjID.HARPOON_ITEM,
        type = "back",
        gunFlash = false
    },
    {
        name = TEN.Flow.GetString("grenade_launcher"),
        objID = TEN.Objects.ObjID.GRENADE_ANIM,
        meshIndices = { 10 },
        weaponType = TEN.Objects.WeaponType.GRENADE_LAUNCHER,
        pickupObjID = TEN.Objects.ObjID.GRENADE_GUN_ITEM,
        type = "back",
        gunFlash = false
    },
    {
        name = TEN.Flow.GetString("rocket_launcher"),
        objID = TEN.Objects.ObjID.ROCKET_ANIM,
        meshIndices = { 10 },
        weaponType = TEN.Objects.WeaponType.ROCKET_LAUNCHER,
        pickupObjID = TEN.Objects.ObjID.ROCKET_LAUNCHER_ITEM,
        type = "back",
        gunFlash = false
    },
    {
        name = TEN.Flow.GetString("flares"),
        objID = TEN.Objects.ObjID.FLARE_ANIM,
        meshIndices = { 13 },
        weaponType = TEN.Objects.WeaponType.FLARE,
        pickupObjID = TEN.Objects.ObjID.FLARE_INV_ITEM,
        type = "none",
        gunFlash = false
    },
    {
        name = TEN.Flow.GetString("crowbar"),
        objID = TEN.Objects.ObjID.LARA_CROWBAR_ANIM,
        meshIndices = { 10 },
        weaponType = TEN.Objects.WeaponType.NONE,
        pickupObjID = TEN.Objects.ObjID.CROWBAR_ITEM,
        type = "none",
        gunFlash = false
    }
}

return Weapons