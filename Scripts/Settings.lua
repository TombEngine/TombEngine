-- TombEngine settings file
-- WARNING: Bad values could make your game unplayable; please follow reference guide attentively.

local Flow = TEN.Flow

local settings = Flow.Settings.new()

	settings.Animations.crouchRoll = true
	settings.Animations.crawlspaceSwandive = true
	settings.Animations.sprintJump = false
	settings.Animations.ledgeJumps = false
	settings.Animations.poseTimeout = 0
	
	settings.Camera.binocularLightColor = Color(192, 192, 96)
	settings.Camera.lasersightLightColor = Color(255, 0, 0)
	settings.Camera.objectCollision = true
	
	settings.Flare.color = Color(128, 64, 0)
	settings.Flare.offset = Vec3(0, 0, 41)
	settings.Flare.range = 9
	settings.Flare.timeout = 60
	settings.Flare.lensflareBrightness = 1.0
	settings.Flare.sparks = true
	settings.Flare.smoke = true
	settings.Flare.flicker = true
	settings.Flare.pickupCount = 12
	
	settings.Hud.statusBars = true
	settings.Hud.loadingBar = true
	settings.Hud.speedometer = true
	settings.Hud.pickupNotifier = true
	
	settings.Physics.gravity = 6
	settings.Physics.swimVelocity = 50
	
	settings.System.errorMode = Flow.ErrorMode.WARN
	settings.System.fastReload = true
	settings.System.multithreaded = true
	
	-- Hair[1] is normal player hair. Types [2] and [3] are for left and right young Lara hair.
	
	settings.Hair[1].rootMesh = 14
	settings.Hair[1].offset = Vec3(-4, -4, -48)
	settings.Hair[1].indices = { 37, 39, 40, 38 }
	
	settings.Hair[2].rootMesh = 14
	settings.Hair[2].offset = Vec3(-48, -48, -50)
	settings.Hair[2].indices = { 79, 78, 76, 77 }
	
	settings.Hair[3].rootMesh = 14
	settings.Hair[3].offset = Vec3(48, -48, -50)
	settings.Hair[3].indices = { 68, 69, 70, 71 }
	
	-- Not all weapon settings are applicable to every weapon. Those which are not applicable
	-- for a particular weapon type are omitted. See documentation for more details.
	
	settings.Weapons[WeaponType.PISTOLS].accuracy = 8
	settings.Weapons[WeaponType.PISTOLS].targetingDistance = 8192
	settings.Weapons[WeaponType.PISTOLS].interval = 9
	settings.Weapons[WeaponType.PISTOLS].waterLevel = 650
	settings.Weapons[WeaponType.PISTOLS].flashDuration = 3
	settings.Weapons[WeaponType.PISTOLS].flashRange = 9
	settings.Weapons[WeaponType.PISTOLS].flashColor = Color(192, 128, 0)
	settings.Weapons[WeaponType.PISTOLS].damage = 1
	settings.Weapons[WeaponType.PISTOLS].smoke = true
	settings.Weapons[WeaponType.PISTOLS].shell = true
	settings.Weapons[WeaponType.PISTOLS].muzzleFlash = true
	settings.Weapons[WeaponType.PISTOLS].colorizeMuzzleFlash = false
	settings.Weapons[WeaponType.PISTOLS].pickupCount = 30
	
	settings.Weapons[WeaponType.REVOLVER].accuracy = 8
	settings.Weapons[WeaponType.REVOLVER].targetingDistance = 8192
	settings.Weapons[WeaponType.REVOLVER].interval = 16
	settings.Weapons[WeaponType.REVOLVER].waterLevel = 650
	settings.Weapons[WeaponType.REVOLVER].flashDuration = 3
	settings.Weapons[WeaponType.REVOLVER].flashRange = 9
	settings.Weapons[WeaponType.REVOLVER].flashColor = Color(192, 128, 0)
	settings.Weapons[WeaponType.REVOLVER].damage = 21
	settings.Weapons[WeaponType.REVOLVER].alternateDamage = 21
	settings.Weapons[WeaponType.REVOLVER].smoke = true
	settings.Weapons[WeaponType.REVOLVER].shell = false
	settings.Weapons[WeaponType.REVOLVER].muzzleFlash = true
	settings.Weapons[WeaponType.REVOLVER].colorizeMuzzleFlash = false
	settings.Weapons[WeaponType.REVOLVER].pickupCount = 6
	
	settings.Weapons[WeaponType.UZIS].accuracy = 8
	settings.Weapons[WeaponType.UZIS].targetingDistance = 8192
	settings.Weapons[WeaponType.UZIS].interval = 3
	settings.Weapons[WeaponType.UZIS].waterLevel = 650
	settings.Weapons[WeaponType.UZIS].flashDuration = 2
	settings.Weapons[WeaponType.UZIS].flashRange = 9
	settings.Weapons[WeaponType.UZIS].flashColor = Color(192, 128, 0)
	settings.Weapons[WeaponType.UZIS].damage = 1
	settings.Weapons[WeaponType.UZIS].smoke = true
	settings.Weapons[WeaponType.UZIS].shell = true
	settings.Weapons[WeaponType.UZIS].muzzleFlash = true
	settings.Weapons[WeaponType.UZIS].colorizeMuzzleFlash = false
	settings.Weapons[WeaponType.UZIS].pickupCount = 30
	
	settings.Weapons[WeaponType.SHOTGUN].accuracy = 10
	settings.Weapons[WeaponType.SHOTGUN].targetingDistance = 8192
	settings.Weapons[WeaponType.SHOTGUN].waterLevel = 500
	settings.Weapons[WeaponType.SHOTGUN].flashDuration = 3
	settings.Weapons[WeaponType.SHOTGUN].flashRange = 12
	settings.Weapons[WeaponType.SHOTGUN].flashColor = Color(192, 128, 0)
	settings.Weapons[WeaponType.SHOTGUN].damage = 3
	settings.Weapons[WeaponType.SHOTGUN].smoke = true
	settings.Weapons[WeaponType.SHOTGUN].shell = true
	settings.Weapons[WeaponType.SHOTGUN].muzzleFlash = false
	settings.Weapons[WeaponType.SHOTGUN].colorizeMuzzleFlash = false
	settings.Weapons[WeaponType.SHOTGUN].pickupCount = 6
	
	settings.Weapons[WeaponType.HK].accuracy = 4
	settings.Weapons[WeaponType.HK].targetingDistance = 12288
	settings.Weapons[WeaponType.HK].waterLevel = 500
	settings.Weapons[WeaponType.HK].flashDuration = 2
	settings.Weapons[WeaponType.HK].flashRange = 12
	settings.Weapons[WeaponType.HK].flashColor = Color(192, 128, 0)
	settings.Weapons[WeaponType.HK].damage = 4
	settings.Weapons[WeaponType.HK].alternateDamage = 4
	settings.Weapons[WeaponType.HK].smoke = true
	settings.Weapons[WeaponType.HK].shell = true
	settings.Weapons[WeaponType.HK].muzzleFlash = true
	settings.Weapons[WeaponType.HK].colorizeMuzzleFlash = false
	settings.Weapons[WeaponType.HK].pickupCount = 30
	
	settings.Weapons[WeaponType.CROSSBOW].targetingDistance = 8192
	settings.Weapons[WeaponType.CROSSBOW].waterLevel = 500
	settings.Weapons[WeaponType.CROSSBOW].damage = 5
	settings.Weapons[WeaponType.CROSSBOW].alternateDamage = 20
	settings.Weapons[WeaponType.CROSSBOW].pickupCount = 10
	
	settings.Weapons[WeaponType.GRENADE_LAUNCHER].targetingDistance = 8192
	settings.Weapons[WeaponType.GRENADE_LAUNCHER].waterLevel = 500
	settings.Weapons[WeaponType.GRENADE_LAUNCHER].damage = 20
	settings.Weapons[WeaponType.GRENADE_LAUNCHER].smoke = true
	settings.Weapons[WeaponType.GRENADE_LAUNCHER].pickupCount = 10
	
	settings.Weapons[WeaponType.ROCKET_LAUNCHER].targetingDistance = 8192
	settings.Weapons[WeaponType.ROCKET_LAUNCHER].waterLevel = 500
	settings.Weapons[WeaponType.ROCKET_LAUNCHER].damage = 30
	settings.Weapons[WeaponType.ROCKET_LAUNCHER].smoke = true
	settings.Weapons[WeaponType.ROCKET_LAUNCHER].pickupCount = 1
	
	settings.Weapons[WeaponType.HARPOON_GUN].targetingDistance = 8192
	settings.Weapons[WeaponType.HARPOON_GUN].damage = 6
	settings.Weapons[WeaponType.HARPOON_GUN].pickupCount = 10
	
Flow.SetSettings(settings)
