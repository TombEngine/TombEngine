#include "framework.h"
#include "Scripting/Internal/TEN/Flow/Settings/Settings.h"

#include "Game/effects/Hair.h"
#include "Scripting/Internal/TEN/Objects/Lara/WeaponTypes.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptUtil.h"

using namespace TEN::Effects::Hair;
using namespace TEN::Scripting::Types;

namespace TEN::Scripting
{

	/// Global engine settings which don't fall into particular category or can't be assigned to a specific object.
	// Can be accessed using @{Flow.SetSettings} and @{Flow.GetSettings} functions.
	// @tenclass Flow.Settings
	// @pragma nostrip

	Settings::Settings()
	{
		Hair[(int)PlayerHairType::Normal]     = { LM_HEAD, Vec3(-4.0f,  -4.0f, -48.0f),  { 37, 39, 40, 38 } };
		Hair[(int)PlayerHairType::YoungLeft]  = { LM_HEAD, Vec3(-48.0f, -48.0f, -50.0f), { 79, 78, 76, 77 } };
		Hair[(int)PlayerHairType::YoungRight] = { LM_HEAD, Vec3(48.0f,  -48.0f, -50.0f), { 68, 69, 70, 71 } };
	
		// NOTE: Since Weapons array is bound to Lua directly and Lua accesses this array by native enum, where 0 is NONE, and 1 is PISTOLS,
		// 0 index is omitted due to Lua indexing arrays starting from 1. 1 must be subtracted from initializer index.
		Weapons[(int)LaraWeaponType::Pistol          - 1] = { 8.0f,  BLOCK(8),  9,  (int)BLOCK(0.65f), 1,  1,  30, ScriptColor(192, 128, 0), 9,  3, true,  true,  true,  false, false, Vec3(  0, 120, 30)  };
		Weapons[(int)LaraWeaponType::Revolver        - 1] = { 4.0f,  BLOCK(8),  16, (int)BLOCK(0.65f), 21, 21, 6,  ScriptColor(192, 128, 0), 9,  3, true,  false, true,  false, false, Vec3(-10, 130, 65)  };
		Weapons[(int)LaraWeaponType::Uzi             - 1] = { 8.0f,  BLOCK(8),  3,  (int)BLOCK(0.65f), 1,  1,  30, ScriptColor(192, 128, 0), 9,  2, true,  true,  true,  false, false, Vec3(  0, 110, 40)  };
		Weapons[(int)LaraWeaponType::Shotgun         - 1] = { 10.0f, BLOCK(8),  0,  (int)BLOCK(0.50f), 3,  3,  6,  ScriptColor(192, 128, 0), 12, 3, true,  true,  false, false, false, Vec3(  0, 210, 40)  };
		Weapons[(int)LaraWeaponType::HK              - 1] = { 4.0f,  BLOCK(12), 0,  (int)BLOCK(0.50f), 4,  4,  30, ScriptColor(192, 128, 0), 12, 2, true,  true,  true,  false, false, Vec3(  0, 220, 102) };
		Weapons[(int)LaraWeaponType::Crossbow        - 1] = { 8.0f,  BLOCK(8),  0,  (int)BLOCK(0.50f), 5,  20, 10, ScriptColor(192, 128, 0), 0,  0, false, false, false, false, false, Vec3(  0, 240, 50)  };
		Weapons[(int)LaraWeaponType::GrenadeLauncher - 1] = { 8.0f,  BLOCK(8),  0,  (int)BLOCK(0.50f), 30, 30, 10, ScriptColor(192, 128, 0), 0,  0, true,  false, false, false, false, Vec3(  0, 190, 50)  };
		Weapons[(int)LaraWeaponType::RocketLauncher  - 1] = { 8.0f,  BLOCK(8),  0,  (int)BLOCK(0.50f), 30, 30, 1,  ScriptColor(192, 128, 0), 0,  0, true,  false, false, false, false, Vec3(  0, 90,  90)  };
		Weapons[(int)LaraWeaponType::HarpoonGun      - 1] = { 8.0f,  BLOCK(8),  0,  (int)BLOCK(0.50f), 6,  6,  10, ScriptColor(192, 128, 0), 0,  0, false, false, false, false, false, Vec3(-15, 240, 50)  };
	}

	void Settings::Register(sol::table& parent)
	{
		AnimSettings::Register(parent);
		CameraSettings::Register(parent);
		FlareSettings::Register(parent);
		GraphicsSettings::Register(parent);
		HairSettings::Register(parent);
		HudSettings::Register(parent);
		PhysicsSettings::Register(parent);
		SystemSettings::Register(parent);
		WeaponSettings::Register(parent);

		parent.new_usertype<Settings>(
			ScriptReserved_Settings,
			sol::constructors<Settings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(Settings, ScriptReserved_Settings),
			ScriptReserved_AnimSettings, &Settings::Animations,
			ScriptReserved_CameraSettings, &Settings::Camera,
			ScriptReserved_FlareSettings, &Settings::Flare,
			ScriptReserved_GraphicsSettings, &Settings::Graphics,
			ScriptReserved_HairSettings, &Settings::Hair,
			ScriptReserved_HudSettings, &Settings::Hud,
			ScriptReserved_PhysicsSettings, &Settings::Physics,
			ScriptReserved_SystemSettings, &Settings::System,
			ScriptReserved_WeaponSettings, &Settings::Weapons);
	}

	/// Animations
	// @section Animations
	// These settings determine whether a specific moveset is available in-game.

	void AnimSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<AnimSettings>(
			ScriptReserved_AnimSettings, sol::constructors<AnimSettings()>(),
			sol::call_constructor, sol::constructors<AnimSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(AnimSettings, ScriptReserved_AnimSettings),

		/// Extended crawl moveset.
		// @tfield bool crawlExtended When enabled, player will be able to traverse across one-click steps in crawlspaces.
		"crawlExtended", &AnimSettings::CrawlExtended,

		/// Crouch roll.
		// @tfield bool crouchRoll When enabled, player can perform crawlspace roll by pressing sprint key.
		"crouchRoll", &AnimSettings::CrouchRoll,

		/// Crawlspace dive.
		// @tfield bool crawlspaceSwandive When enabled, player will be able to swandive into crawlspaces.
		"crawlspaceSwandive", &AnimSettings::CrawlspaceDive,

		// Overhang climbing.
		// @tfield bool overhangClimb Enables overhang climbing feature. Currently does not work.
		"overhangClimb", &AnimSettings::OverhangClimb,

		// Extended slide mechanics.
		// @tfield bool slideExtended If enabled, player will be able to change slide direction with controls. Currently does not work.
		"slideExtended", &AnimSettings::SlideExtended,

		/// Sprint jump.
		// @tfield bool sprintJump If enabled, player will be able to perform extremely long jump when sprinting.
		"sprintJump", &AnimSettings::SprintJump,

		/// Ledge jumps.
		// @tfield bool ledgeJumps If this setting is enabled, player will be able to jump upwards while hanging on the ledge.
		"ledgeJumps", &AnimSettings::LedgeJumps,

		/// Pose timeout.
		// @tfield int poseTimeout If this setting is larger than 0, idle standing pose animation will be performed after given timeout (in seconds).
		"poseTimeout", &AnimSettings::PoseTimeout);
	}

	/// Camera
	// @section Camera
	// Parameters to customize camera and everything related to it.

	void CameraSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<CameraSettings>(ScriptReserved_CameraSettings, sol::constructors<CameraSettings()>(),
			sol::call_constructor, sol::constructors<CameraSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(CameraSettings, ScriptReserved_CameraSettings),

		/// Determines highlight color in binocular mode.
		// @tfield Color binocularLightColor Color of highlight, when player presses action. Zero color means there will be no highlight.
		"binocularLightColor", &CameraSettings::BinocularLightColor,
	
		/// Determines highlight color in lasersight mode.
		// @tfield Color lasersightLightColor Lasersight highlight color. Zero color means there will be no highlight.
		"lasersightLightColor", &CameraSettings::LasersightLightColor,
	
		/// Specify whether camera can collide with objects.
		// @tfield bool objectCollision When enabled, camera will collide with moveables and statics. Disable for TR4-like camera behaviour.
		"objectCollision", &CameraSettings::ObjectCollision);
	}

	/// Flare
	// @section Flare
	// These settings change appearance and behaviour of a flare.

	void FlareSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<FlareSettings>(ScriptReserved_FlareSettings, sol::constructors<FlareSettings()>(),
			sol::call_constructor, sol::constructors<FlareSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(FlareSettings, ScriptReserved_FlareSettings),

		/// Flare color.
		// @tfield Color color Flare color. Used for sparks and lensflare coloring as well.
		"color", &FlareSettings::Color,

		/// Muzzle offset.
		// @tfield Vec3 offset A relative muzzle offset where light and particle effects originate from.
		"offset", &FlareSettings::Offset,

		/// Light range.
		// @tfield int range Flare light radius or range. Represented in "clicks" equal to 256 world units.
		"range", &FlareSettings::Range,

		/// Burn timeout.
		// @tfield int timeout Flare burn timeout. Flare will stop working after given timeout (specified in seconds).
		"timeout", &FlareSettings::Timeout,

		/// Default flare pickup count.
		// @tfield int pickupCount Specifies amount of flares that you get when you pick up a box of flares.
		"pickupCount", &FlareSettings::PickupCount,

		/// Lens flare brightness.
		// @tfield float lensflareBrightness Brightness multiplier. Specifies how bright lens flare is in relation to light (on a range from 0 to 1).
		"lensflareBrightness", &FlareSettings::LensflareBrightness,

		/// Toggle spark effect.
		// @tfield bool sparks Spark effect. Determines whether flare generates sparks when burning.
		"sparks", &FlareSettings::Sparks,

		/// Toggle smoke effect.
		// @tfield bool smoke Smoke effect. Determines whether flare generates smoke when burning.
		"smoke", &FlareSettings::Smoke,

		/// Toggle flicker effect.
		// @tfield bool flicker Light and lensflare flickering. When turned off, flare light will be constant.
		"flicker", &FlareSettings::Flicker);
	}

	/// Graphics
	// @section Graphics
	// These settings are used to enable or disable certain graphics features.

	void GraphicsSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<GraphicsSettings>(ScriptReserved_GraphicsSettings, sol::constructors<GraphicsSettings()>(),
			sol::call_constructor, sol::constructors<GraphicsSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(GraphicsSettings, ScriptReserved_GraphicsSettings),

			/// Enable skinning.
			// @tfield bool skinning If enabled, skinning will be used for animated objects with skin. Disable to force classic TR workflow.
			"skinning", &GraphicsSettings::Skinning);
	}

	/// Hair
	// @section Hair
	// This is a table of braid object settings. <br>
	// Table consists of three entries, with first one representing classic Lara braid, and 2 and 3 representing left and right young Lara braids respectively.
	// Therefore, if you want to access classic Lara braid settings, use `settings.Hair[1]`, and so on.

	void HairSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<HairSettings>(ScriptReserved_HairSettings, sol::constructors<HairSettings()>(),
			sol::call_constructor, sol::constructors<HairSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(HairSettings, ScriptReserved_HairSettings),

		/// Root mesh to which hair object will attach to.
		// @tfield int mesh Index of a root mesh to which hair will attach. Root mesh may be different for each hair object.
		"rootMesh", &HairSettings::RootMesh,

		/// Relative braid offset to a headmesh. Not used with skinned hair mesh.
		// @tfield Vec3 offset Specifies how braid is positioned in relation to a headmesh.
		"offset", &HairSettings::Offset,
	
		/// Braid connection indices. Not used with skinned hair mesh.
		// @tfield table indices A list of headmesh's vertex connection indices. Each index corresponds to nearest braid rootmesh vertex. Amount of indices is unlimited.
		"indices", &HairSettings::Indices);
	}

	/// Hud
	// @section Hud
	// These settings determine visibility of particular in-game HUD elements.

	void HudSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<HudSettings>(ScriptReserved_HudSettings, sol::constructors<HudSettings()>(),
			sol::call_constructor, sol::constructors<HudSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(HudSettings, ScriptReserved_HudSettings),

		/// Toggle in-game status bars visibility.
		// @tfield bool statusBars If disabled, all status bars (health, air, stamina) will be hidden.
		"statusBars", &HudSettings::StatusBars,

		/// Toggle loading bar visibility.
		// @tfield bool loadingBar If disabled, loading bar will be invisible in game.
		"loadingBar", &HudSettings::LoadingBar,

		/// Toggle speedometer visibility.
		// @tfield bool speedometer If disabled, speedometer will be invisible in game.
		"speedometer", &HudSettings::Speedometer,

		/// Toggle pickup notifier visibility.
		// @tfield bool pickupNotifier If disabled, pickup notifier will be invisible in game.
		"pickupNotifier", &HudSettings::PickupNotifier);
	}

	/// Physics
	// @section Physics
	// Here you will find various settings for game world physics.

	void PhysicsSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<PhysicsSettings>(ScriptReserved_PhysicsSettings, sol::constructors<PhysicsSettings()>(),
			sol::call_constructor, sol::constructors<PhysicsSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(PhysicsSettings, ScriptReserved_PhysicsSettings),

		/// Global world gravity.
		// @tfield float gravity Specifies global gravity. Mostly affects Lara and several other objects.
		"gravity", &PhysicsSettings::Gravity,

		/// Swim velocity.
		// @tfield float swimVelocity Specifies swim velocity for Lara. Affects both surface and underwater.
		"swimVelocity", &PhysicsSettings::SwimVelocity);
	}

	/// Weapons
	// @section Weapons
	// This is a table of weapon settings, with several parameters available for every weapon.
	// Access particular weapon's settings by using @{Objects.WeaponType} as an index for this table, e.g. `settings.Weapons[Flow.WeaponType.PISTOLS]`.

	void WeaponSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<WeaponSettings>(ScriptReserved_WeaponSettings, sol::constructors<WeaponSettings()>(),
			sol::call_constructor, sol::constructors<WeaponSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(WeaponSettings, ScriptReserved_WeaponSettings),

		/// Shooting accuracy.
		// @tfield float accuracy Determines accuracy range in angles (smaller angles mean higher accuracy). Applicable only for firearms.
		"accuracy", &WeaponSettings::Accuracy,

		/// Targeting distance.
		// @tfield float targetingDistance Specifies maximum targeting distance in world units (1 block = 1024 world units) for a given weapon.
		"targetingDistance", &WeaponSettings::Distance,

		/// Shooting interval.
		// @tfield float interval Specifies an interval (in frames), after which Lara is able to shoot again. Not applicable for backholster weapons.
		"interval", &WeaponSettings::Interval,

		/// Damage.
		// @tfield int damage Amount of hit points taken for every hit.
		"damage", &WeaponSettings::Damage,

		/// Alternate damage.
		// @tfield int alternateDamage For Revolver and HK, specifies damage in lasersight mode. For crossbow, specifies damage for explosive ammo.
		"alternateDamage", &WeaponSettings::AlternateDamage,

		/// Water level.
		// @tfield int waterLevel Specifies water depth, at which Lara will put weapons back into holsters, indicating it's not possible to use it in water.
		"waterLevel", &WeaponSettings::WaterLevel,

		/// Default ammo pickup count.
		// @tfield int pickupCount Amount of ammo which is given with every ammo pickup for this weapon.
		"pickupCount", &WeaponSettings::PickupCount,

		/// Gunflash color.
		// @tfield Color flashColor specifies the color of the gunflash.
		"flashColor", &WeaponSettings::FlashColor,

		/// Gunflash range.
		// @tfield Color flashRange specifies the range of the gunflash.
		"flashRange", &WeaponSettings::FlashRange,

		/// Gunflash duration.
		// @tfield int flashDuration specifies the duration of a gunflash effect.
		"flashDuration", &WeaponSettings::FlashDuration,

		/// Gun smoke.
		// @tfield bool smoke if set to true, indicates that weapon emits gun smoke.
		"smoke", &WeaponSettings::Smoke,

		/// Gun shell.
		// @tfield bool shell If set to true, indicates that weapon emits gun shell. Applicable only for firearms.
		"shell", &WeaponSettings::Shell,

		/// Display muzzle flash.
		// @tfield bool muzzleFlash specifies whether muzzle flash should be displayed or not.
		"muzzleFlash", &WeaponSettings::MuzzleFlash,

		/// Display muzzle glow.
		// @tfield bool muzzleGlow specifies whether muzzle glow should be displayed or not.
		"muzzleGlow", &WeaponSettings::MuzzleGlow,

		/// Colorize muzzle flash.
		// @tfield bool colorizeMuzzleFlash specifies whether muzzle flash should be tinted with the same color as gunflash color.
		"colorizeMuzzleFlash", &WeaponSettings::ColorizeMuzzleFlash,

		/// Muzzle offset.
		// @tfield Vec3 muzzleOffset specifies offset for spawning muzzle gunflash effects.
		"muzzleOffset", &WeaponSettings::MuzzleOffset);
	}

	/// System
	// @section System
	// Global system settings that is not directly related to gameplay.

	void SystemSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<SystemSettings>(ScriptReserved_SystemSettings, sol::constructors<SystemSettings()>(),
			sol::call_constructor, sol::constructors<SystemSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(SystemSettings, ScriptReserved_SystemSettings),

		/// How should the application respond to script errors?
		// @tfield Flow.ErrorMode errorMode Error mode to use.
		"errorMode", &SystemSettings::ErrorMode,

		/// Use multithreading in certain calculations. <br>
		// When set to `true`, some performance-critical calculations will be performed in parallel, which can give
		// a significant performance boost. Don't disable unless you have problems with launching or using TombEngine.
		// @tfield bool multithreaded Determines whether to use multithreading or not.
		"multithreaded", &SystemSettings::Multithreaded,

		/// Can the game utilize the fast reload feature? <br>
		// When set to `true`, the game will attempt to perform fast savegame reloading if current level is the same as
		// the level loaded from the savegame. It will not work if the level timestamp or checksum has changed
		// (i.e. level was updated). If set to `false`, this functionality is turned off.
		// @tfield bool fastReload Toggles fast reload on or off.
		"fastReload", &SystemSettings::FastReload);
	}
}
