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
	// Flow.Settings is composed of several sub-tables, and each section of the Flow.Settings documentation corresponds to one of these sub-tables.
	// These configuration groups are located in *settings.lua* script file.
	//
	// It is possible to change settings on a per-level basis via @{Flow.GetSettings} and @{Flow.SetSettings} functions, but keep in mind that
	// _settings.lua is reread every time the level is reloaded_. Therefore, you need to implement custom settings management in your level script
	// if you want to override global settings.
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
		GameplaySettings::Register(parent);
		GraphicsSettings::Register(parent);
		HairSettings::Register(parent);
		HudSettings::Register(parent);
		PathfindingSettings::Register(parent);
		PhysicsSettings::Register(parent);
		SystemSettings::Register(parent);
		UISettings::Register(parent);
		WeaponSettings::Register(parent);

		parent.new_usertype<Settings>(
			ScriptReserved_Settings,
			sol::constructors<Settings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(Settings, ScriptReserved_Settings),
			ScriptReserved_AnimSettings, &Settings::Animations,
			ScriptReserved_CameraSettings, &Settings::Camera,
			ScriptReserved_FlareSettings, &Settings::Flare,
			ScriptReserved_GameplaySettings, &Settings::Gameplay,
			ScriptReserved_GraphicsSettings, &Settings::Graphics,
			ScriptReserved_HairSettings, &Settings::Hair,
			ScriptReserved_HudSettings, &Settings::Hud,
			ScriptReserved_PFSettings, &Settings::Pathfinding,
			ScriptReserved_PhysicsSettings, &Settings::Physics,
			ScriptReserved_SystemSettings, &Settings::System,
			ScriptReserved_UISettings, &Settings::UI,
			ScriptReserved_WeaponSettings, &Settings::Weapons);
	}

	/// Animations
	// @section Animations
	// These settings determine whether a specific moveset is available in-game.
	// @usage
	// -- Example of enabling crawlspace roll
	// -- In Settings.lua
	// settings.Animations.crouchRoll = true
	//
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.Animations.crouchRoll = false
	// TEN.Flow.SetSettings(settings)

	void AnimSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<AnimSettings>(
			ScriptReserved_AnimSettings, sol::constructors<AnimSettings()>(),
			sol::call_constructor, sol::constructors<AnimSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(AnimSettings, ScriptReserved_AnimSettings),

		/// Extended crawl moveset.
		// @tfield[opt=true] bool crawlExtended When enabled, player will be able to traverse across one-click steps in crawlspaces.
		"crawlExtended", &AnimSettings::CrawlExtended,

		/// Crouch roll.
		// @tfield[opt=true] bool crouchRoll When enabled, player can perform crawlspace roll by pressing sprint key.
		"crouchRoll", &AnimSettings::CrouchRoll,

		/// Crawlspace dive.
		// @tfield[opt=true] bool crawlspaceSwandive When enabled, player will be able to swandive into crawlspaces.
		"crawlspaceSwandive", &AnimSettings::CrawlspaceDive,

		// Overhang climbing.
		// @tfield bool overhangClimb Enables overhang climbing feature. Currently does not work.
		"overhangClimb", &AnimSettings::OverhangClimb,

		// Extended slide mechanics.
		// @tfield bool slideExtended If enabled, player will be able to change slide direction with controls. Currently does not work.
		"slideExtended", &AnimSettings::SlideExtended,

		/// Sprint jump.
		// @tfield[opt=false] bool sprintJump If enabled, player will be able to perform extremely long jump when sprinting.
		"sprintJump", &AnimSettings::SprintJump,

		/// Ledge jumps.
		// @tfield[opt=false] bool ledgeJumps If this setting is enabled, player will be able to jump upwards while hanging on the ledge.
		"ledgeJumps", &AnimSettings::LedgeJumps,

		/// Pose timeout.
		// @tfield[opt=20] int poseTimeout If this setting is larger than 0, idle standing pose animation will be performed after given timeout (in seconds).
		"poseTimeout", &AnimSettings::PoseTimeout);
	}

	/// Camera
	// @section Camera
	// Parameters to customize camera and everything related to it.
	// @usage
	// -- Example of changing binocular and lasersight highlight colors
	// -- In Settings.lua
	// settings.Camera.binocularLightColor = TEN.Color(255, 0, 255)
	// settings.Camera.lasersightLightColor = TEN.Color(0, 255, 255)
	//
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.Camera.binocularLightColor = TEN.Color(255, 0, 255)
	// settings.Camera.lasersightLightColor = TEN.Color(0, 255, 255)
	// TEN.Flow.SetSettings(settings)

	void CameraSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<CameraSettings>(ScriptReserved_CameraSettings, sol::constructors<CameraSettings()>(),
			sol::call_constructor, sol::constructors<CameraSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(CameraSettings, ScriptReserved_CameraSettings),

		/// Determines highlight color in binocular mode.
		// @tfield[opt=TEN.Color(192&#44; 192&#44; 96)] Color binocularLightColor Color of highlight, when player presses action. Zero color means there will be no highlight.
		"binocularLightColor", &CameraSettings::BinocularLightColor,
	
		/// Determines highlight color in lasersight mode.
		// @tfield[opt=TEN.Color(255&#44; 0&#44; 0)] Color lasersightLightColor Lasersight highlight color. Zero color means there will be no highlight.
		"lasersightLightColor", &CameraSettings::LasersightLightColor,
	
		/// Specify whether camera can collide with objects.
		// @tfield[opt=true] bool objectCollision When enabled, camera will collide with moveables and statics. Disable for TR4-like camera behaviour.
		"objectCollision", &CameraSettings::ObjectCollision);
	}

	/// Flare
	// @section Flare
	// These settings change appearance and behaviour of a flare.
	// @usage
	// -- Example of changing flare color and disabling sparks
	// -- In Settings.lua
	// settings.Flare.color = TEN.Color(255, 128, 0)
	// settings.Flare.sparks = false
	//
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.Flare.color = TEN.Color(255, 128, 0)
	// settings.Flare.sparks = false
	// TEN.Flow.SetSettings(settings)

	void FlareSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<FlareSettings>(ScriptReserved_FlareSettings, sol::constructors<FlareSettings()>(),
			sol::call_constructor, sol::constructors<FlareSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(FlareSettings, ScriptReserved_FlareSettings),

		/// Flare color.
		// @tfield[opt=TEN.Color(128&#44; 64&#44; 0)] Color color Flare color. Used for sparks and lensflare coloring as well.
		"color", &FlareSettings::Color,

		/// Muzzle offset.
		// @tfield[opt=Vec3(0&#44; 0&#44; 41)] Vec3 offset A relative muzzle offset where light and particle effects originate from.
		"offset", &FlareSettings::Offset,

		/// Light range.
		// @tfield[opt=9] int range Flare light radius or range. Represented in "clicks" equal to 256 world units.
		"range", &FlareSettings::Range,

		/// Burn timeout.
		// @tfield[opt=60] int timeout Flare burn timeout. Flare will stop working after given timeout (specified in seconds).
		"timeout", &FlareSettings::Timeout,

		/// Default flare pickup count.
		// @tfield[opt=12] int pickupCount Specifies amount of flares that you get when you pick up a box of flares.
		"pickupCount", &FlareSettings::PickupCount,

		/// Lens flare brightness.
		// @tfield[opt=0.5] float lensflareBrightness Brightness multiplier. Specifies how bright lens flare is in relation to light (on a range from 0 to 1).
		"lensflareBrightness", &FlareSettings::LensflareBrightness,

		/// Toggle spark effect.
		// @tfield[opt=true] bool sparks Spark effect. Determines whether flare generates sparks when burning.
		"sparks", &FlareSettings::Sparks,

		/// Toggle smoke effect.
		// @tfield[opt=true] bool smoke Smoke effect. Determines whether flare generates smoke when burning.
		"smoke", &FlareSettings::Smoke,

		/// Toggle muzzle glow effect.
		// @tfield[opt=false] bool muzzleGlow Glow effect. Determines whether flare generates glow when burning.
		"muzzleGlow", &FlareSettings::MuzzleGlow,

		/// Toggle flicker effect.
		// @tfield[opt=true] bool flicker Light and lensflare flickering. When turned off, flare light will be constant.
		"flicker", &FlareSettings::Flicker);
	}

	/// Gameplay
	// @section Gameplay
	// These settings are used to enable or disable certain gameplay features.
	// @usage
	// -- Example of disabling inventory and enabling target occlusion
	// -- In Settings.lua
	// settings.Gameplay.enableInventory = false
	// settings.Gameplay.targetObjectOcclusion = true
	//
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.Gameplay.enableInventory = false
	// settings.Gameplay.targetObjectOcclusion = true
	// TEN.Flow.SetSettings(settings)

	void GameplaySettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<GameplaySettings>(ScriptReserved_GameplaySettings, sol::constructors<GameplaySettings()>(),
			sol::call_constructor, sol::constructors<GameplaySettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(GameplaySettings, ScriptReserved_GameplaySettings),

		/// Enable or disable original linear inventory functionality. Can be used to completely disable inventory handling
		// or to replace it with custom module, such as ring inventory.
		// @tfield[opt=true] bool enableInventory If false, inventory will not open.
		"enableInventory", &GameplaySettings::EnableInventory,

		/// Kill enemies which were poisoned by a crossbow poisoned ammo or by any other means. If disabled, enemy hit points will
		// reach minimum but will never go to zero. This behaviour replicates original TR4 behaviour.
		// @tfield[opt=true] bool killPoisonedEnemies If false, enemies won't be killed by poison.
		"killPoisonedEnemies", &GameplaySettings::KillPoisonedEnemies,

		/// Enable target occlusion by moveables and static meshes.
		// @tfield[opt=true] bool targetObjectOcclusion If enabled, player won't be able to target enemies through moveables and static meshes.
		"targetObjectOcclusion", &GameplaySettings::TargetObjectOcclusion);
	}

	/// Graphics
	// @section Graphics
	// These settings are used to enable or disable certain graphics features.
	// @usage
	// -- Example of disabling ambient occlusion and disabling skinning
	// -- In Settings.lua
	// settings.Graphics.ambientOcclusion = false
	// settings.Graphics.skinning = false
	//
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.Graphics.ambientOcclusion = false
	// settings.Graphics.skinning = false
	// TEN.Flow.SetSettings(settings)

	void GraphicsSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<GraphicsSettings>(ScriptReserved_GraphicsSettings, sol::constructors<GraphicsSettings()>(),
			sol::call_constructor, sol::constructors<GraphicsSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(GraphicsSettings, ScriptReserved_GraphicsSettings),

		/// Enable ambient occlusion.
		// @tfield[opt=true] bool ambientOcclusion If disabled, ambient occlusion setting will be forced to off, and corresponding menu entry in the Display Settings dialog will be grayed out.
		"ambientOcclusion", &GraphicsSettings::AmbientOcclusion,

		/// Enable skinning.
		// @tfield[opt=true] bool skinning If enabled, skinning will be used for animated objects with skinned mesh. Disable to force classic TR workflow.
		"skinning", &GraphicsSettings::Skinning);
	}

	/* @fieldtype HairSettings[] */
	/// Hair
	// @section Hair
	// This is a table of braid object settings. <br>
	// Table consists of three entries, with first one representing classic Lara braid, and 2 and 3 representing left and right young Lara braids respectively.
	// Therefore, if you want to access classic Lara braid settings, use `settings.Hair[1]`, and so on.
	// @usage
	// -- Example of changing offset for young left braid
	// -- In Settings.lua
	// settings.Hair[2].offset = TEN.Vec3(-50, -50, -50)
	// settings.Hair[3].offset = TEN.Vec3(50, -50, -50)
	// 
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.Hair[2].offset = TEN.Vec3(-50, -50, -50)
	// settings.Hair[3].offset = TEN.Vec3(50, -50, -50)
	// TEN.Flow.SetSettings(settings)

	void HairSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<HairSettings>(ScriptReserved_HairSettings, sol::constructors<HairSettings()>(),
			sol::call_constructor, sol::constructors<HairSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(HairSettings, ScriptReserved_HairSettings),

		/// Root mesh to which hair object will attach to.
		// @tfield[opt=14] int rootMesh Index of a root mesh to which hair will attach. Root mesh may be different for each hair object.
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
	// @usage
	// -- Example of disabling status bars and disabling pickup notifier
	// -- In Settings.lua
	// settings.Hud.statusBars = false
	// settings.Hud.pickupNotifier = false
	//
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.Hud.statusBars = false
	// settings.Hud.pickupNotifier = false

	void HudSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<HudSettings>(ScriptReserved_HudSettings, sol::constructors<HudSettings()>(),
			sol::call_constructor, sol::constructors<HudSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(HudSettings, ScriptReserved_HudSettings),

		/// Toggle in-game status bars visibility.
		// @tfield[opt=true] bool statusBars If disabled, all status bars (health, air, stamina) will be hidden.
		"statusBars", &HudSettings::StatusBars,

		/// Toggle loading bar visibility.
		// @tfield[opt=true] bool loadingBar If disabled, loading bar will be invisible in game.
		"loadingBar", &HudSettings::LoadingBar,

		/// Toggle speedometer visibility.
		// @tfield[opt=true] bool speedometer If disabled, speedometer will be invisible in game.
		"speedometer", &HudSettings::Speedometer,

		/// Toggle pickup notifier visibility.
		// @tfield[opt=true] bool pickupNotifier If disabled, pickup notifier will be invisible in game.
		"pickupNotifier", &HudSettings::PickupNotifier);
	}

	/// Pathfinding
	// @section Pathfinding
	// Features and enhancements that modify enemy behaviour during pathfinding and while tracking player and other enemies.
	// @usage
	// -- Example of changing pathfinding mode and disabling moveable avoidance
	// -- In Settings.lua
	// settings.Pathfinding.mode = TEN.Flow.PathfindingMode.Dijkstra
	// settings.Pathfinding.moveableAvoidance = false
	//
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.Pathfinding.mode = TEN.Flow.PathfindingMode.Dijkstra
	// settings.Pathfinding.moveableAvoidance = false
	// TEN.Flow.SetSettings(settings)

	void PathfindingSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<PathfindingSettings>(ScriptReserved_PFSettings, sol::constructors<PathfindingSettings()>(),
			sol::call_constructor, sol::constructors<PathfindingSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(PathfindingSettings, ScriptReserved_PFSettings),

		/// Pathfinding mode.
		// @tfield[opt=Flow.PathfindingMode.ASTAR] Flow.PathfindingMode mode The algorithm used for pathfinding. For more information, refer to @{Flow.PathfindingMode}.
		"mode", &PathfindingSettings::Mode,

		/// Pathfinding graph search depth.
		// @tfield[opt=5] int searchDepth Specifies how deep the AI will search the pathfinding graph when calculating a path to the target.
		"searchDepth", &PathfindingSettings::SearchDepth,

		/// Escape distance.
		// @tfield[opt=5] int escapeDistance If enemy is being attacked, it attempts to escape as far as possible from the attacker. This
		// value specifies the distance the enemy will try to reach when escaping.
		"escapeDistance", &PathfindingSettings::EscapeDistance,

		/// Stalk distance.
		// @tfield[opt=3] int stalkDistance Distance at which an enemy may start to track a target without attempting to attack.
		"stalkDistance", &PathfindingSettings::StalkDistance,

		/// Path prediction scale factor.
		// @tfield[opt=15.0] float predictionFactor Determines how far ahead enemy predicts the target's position based on its
		// current velocity. A higher value makes enemies intercept the target earlier, while a lower value reduces anticipation.
		// If set to 0, prediction will be disabled.
		"predictionFactor", &PathfindingSettings::PredictionFactor,

		/// Collision penalty threshold.
		// @tfield[opt=1.0] float collisionPenaltyThreshold Specifies the timeout in seconds after which the enemy will be punished for
		// collisions with illegal geometry and will be forced to ignore its current path to the target and recalculate it.
		// If set to 0, collision penalties will be disabled.
		"collisionPenaltyThreshold", &PathfindingSettings::CollisionPenaltyThreshold,

		/// Collision penalty cooldown.
		// @tfield[opt=6.0] float collisionPenaltyCooldown If a collision penalty was applied to an enemy, this value specifies the timeout
		// in seconds during which the enemy will ignore the path to the target which previously caused a penalty.
		"collisionPenaltyCooldown", &PathfindingSettings::CollisionPenaltyCooldown,

		/// Moveable avoidance.
		// @tfield[opt=false] bool moveableAvoidance Avoid collisions with moveables where possible. Enemy will attempt to turn away from the
		// moveable if it's in the way. Applies only to moveables not placed near room geometry. Experimental feature, use with caution.
		"moveableAvoidance", &PathfindingSettings::MoveableAvoidance,
			
		/// Static mesh avoidance.
		// @tfield[opt=false] bool staticMeshAvoidance Avoid collisions with static meshes where possible. Enemy will attempt to turn away from the
		// static mesh if it's in the way. Applies only to static meshes not placed near room geometry. Experimental feature, use with caution.
		"staticMeshAvoidance", &PathfindingSettings::StaticMeshAvoidance,

		/// Vertical geometry avoidance for swimming and flying enemies.
		// @tfield[opt=true] bool verticalGeometryAvoidance Avoid swimming or flying forward into illegal room geometry that can be avoided
		// by moving upwards.
		"verticalGeometryAvoidance", &PathfindingSettings::VerticalGeometryAvoidance,

		/// Water surface avoidance for swimming and flying enemies.
		// @tfield[opt=true] bool waterSurfaceAvoidance For flying enemies, prevents diving into the water and dying while attacking
		// the player or other enemies from above. For swimming enemies, adds extra measures to avoid glitching out of the water.
		"waterSurfaceAvoidance", &PathfindingSettings::WaterSurfaceAvoidance,

		/// Vertical movement smoothing for swimming and flying enemies.
		// @tfield[opt=true] bool verticalMovementSmoothing Smooths out vertical movement for swimming and flying enemies to prevent
		// sudden unnatural jerks or changes in direction.
		"verticalMovementSmoothing", &PathfindingSettings::VerticalMovementSmoothing);
	}

	/// Physics
	// @section Physics
	// Here you will find various settings for game world physics.
	// @usage
	// -- Example of changing global gravity and swim velocity
	// -- In Settings.lua
	// settings.Physics.gravity = 9.81
	// settings.Physics.swimVelocity = 6.0
	//
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.Physics.gravity = 9.81
	// settings.Physics.swimVelocity = 6.0
	// TEN.Flow.SetSettings(settings)

	void PhysicsSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<PhysicsSettings>(ScriptReserved_PhysicsSettings, sol::constructors<PhysicsSettings()>(),
			sol::call_constructor, sol::constructors<PhysicsSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(PhysicsSettings, ScriptReserved_PhysicsSettings),

		/// Global world gravity.
		// @tfield[opt=6.0] float gravity Specifies global gravity. Mostly affects Lara and several other objects.
		"gravity", &PhysicsSettings::Gravity,

		/// Swim velocity.
		// @tfield[opt=50.0] float swimVelocity Specifies swim velocity for Lara. Affects both surface and underwater.
		"swimVelocity", &PhysicsSettings::SwimVelocity);
	}

	/// System
	// @section System
	// Global system settings that is not directly related to gameplay.
	// @usage
	// -- Example of changing error mode and disabling fast reload
	// -- In Settings.lua
	// settings.System.errorMode = TEN.Flow.ErrorMode.Throw
	// settings.System.fastReload = false
	//
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.System.errorMode = TEN.Flow.ErrorMode.Throw
	// settings.System.fastReload = false
	// TEN.Flow.SetSettings(settings)

	void SystemSettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<SystemSettings>(ScriptReserved_SystemSettings, sol::constructors<SystemSettings()>(),
			sol::call_constructor, sol::constructors<SystemSettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(SystemSettings, ScriptReserved_SystemSettings),

		/// How should the application respond to script errors?
		// @tfield[opt=Flow.ErrorMode.WARN] Flow.ErrorMode errorMode Error mode to use.
		"errorMode", &SystemSettings::ErrorMode,

		/// Use multithreading in certain calculations. <br>
		// When set to `true`, some performance-critical calculations will be performed in parallel, which can give
		// a significant performance boost. Don't disable unless you have problems with launching or using TombEngine.
		// @tfield[opt=true] bool multithreaded Determines whether to use multithreading or not.
		"multithreaded", &SystemSettings::Multithreaded,

		/// Can the game utilize the fast reload feature? <br>
		// When set to `true`, the game will attempt to perform fast savegame reloading if current level is the same as
		// the level loaded from the savegame. It will not work if the level timestamp or checksum has changed
		// (i.e. level was updated). If set to `false`, this functionality is turned off.
		// @tfield[opt=true] bool fastReload Toggles fast reload on or off.
		"fastReload", &SystemSettings::FastReload);
	}

	/// UI
	// @section UI
	// System-wide user interface settings.
	// @usage
	// -- Example of changing title menu position and header text color
	// -- In Settings.lua
	// settings.UI.titleMenuPosition = TEN.Vec2(400, 300)
	// settings.UI.headerTextColor = TEN.Color(255, 128, 0)
	//
	// -- In the title's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.UI.titleMenuPosition = TEN.Vec2(400, 300)
	// settings.UI.headerTextColor = TEN.Color(255, 128, 0)
	// TEN.Flow.SetSettings(settings)

	void UISettings::Register(sol::table& parent)
	{
		parent.create().new_usertype<UISettings>(ScriptReserved_UISettings, sol::constructors<UISettings()>(),
			sol::call_constructor, sol::constructors<UISettings()>(),
			sol::meta_function::new_index, NewIndexErrorMaker(UISettings, ScriptReserved_UISettings),

		/// Header text color.
		// @tfield[opt=TEN.Color(216&#44; 117&#44; 49)] Color headerTextColor A color used for displaying header text in system menus.
		"headerTextColor", &UISettings::HeaderTextColor,

		/// Option text color.
		// @tfield[opt=TEN.Color(240&#44; 220&#44; 32)] Color optionTextColor A color used for displaying option text in system menus.
		"optionTextColor", &UISettings::OptionTextColor,

		/// Plain text color.
		// @tfield[opt=TEN.Color(255&#44; 255&#44; 255)] Color plainTextColor A color used for displaying plain text in system menus.
		"plainTextColor", &UISettings::PlainTextColor,

		/// Disabled text color.
		// @tfield[opt=TEN.Color(128&#44; 128&#44; 128)] Color disabledTextColor A color used for displaying any header text in menus.
		"disabledTextColor", &UISettings::DisabledTextColor,

		/// Shadow text color.
		// @tfield[opt=TEN.Color(0&#44; 0&#44; 0)] Color shadowTextColor A color used for drawing a shadow under any rendered text.
		"shadowTextColor", &UISettings::ShadowTextColor,

		/// Title logo center point position.
		// @tfield[opt=TEN.Vec2(50&#44; 20)] Vec2 titleLogoPosition Center point of a title level logo position.
		"titleLogoPosition", &UISettings::TitleLogoPosition,

		/// Title logo scale.
		// @tfield[opt=0.38] float titleLogoScale Title level logo scale.
		"titleLogoScale", &UISettings::TitleLogoScale,

		/// Title logo color.
		// @tfield[opt=TEN.Color(255&#44; 255&#44; 255)] Color titleLogoColor Title level logo color.
		"titleLogoColor", &UISettings::TitleLogoColor,
			
		/// Title menu position.
		// @tfield[opt=TEN.Vec2(50&#44; 66)] Vec2 titleMenuPosition Title level menu position. Horizontal coordinate represents an alignment baseline,
		// while vertical coordinate represents a first menu entry's vertical position.
		"titleMenuPosition", &UISettings::TitleMenuPosition,
			
		/// Title menu scale.
		// @tfield[opt=1.0] float titleMenuScale Title level menu scale.
		"titleMenuScale", &UISettings::TitleMenuScale,

		/// Title menu alignment.
		// @tfield[opt=Strings.DisplayStringOption.CENTER] Strings.DisplayStringOption titleMenuAlignment Specifies menu alignment.
		// Can be set to @{Strings.DisplayStringOption.CENTER} or @{Strings.DisplayStringOption.RIGHT}.
		// If set to `nil`, or set to any other value, menu will be aligned to the left side of the screen.
		"titleMenuAlignment", &UISettings::TitleMenuAlignment);
	}

	/* @fieldtype { [WeaponType]: WeaponSettings } */
	/// Weapons
	// @section Weapons
	// This is a table of weapon settings, with several parameters available for every weapon.
	// Access particular weapon's settings by using @{Objects.WeaponType} as an index for this table, e.g. `settings.Weapons[Flow.WeaponType.PISTOLS]`.
	// 
	// Default values for these settings are different for different weapons. Refer to *settings.lua* file to see them.
	// @usage
	// -- Example of changing pistols accuracy and damage
	// -- In Settings.lua
	// settings.Weapons[TEN.Objects.WeaponType.PISTOLS].accuracy = 5.0
	// settings.Weapons[TEN.Objects.WeaponType.PISTOLS].damage = 10
	//
	// -- In the level's lua file
	// local settings = TEN.Flow.GetSettings()
	// settings.Weapons[TEN.Objects.WeaponType.PISTOLS].accuracy = 5.0
	// settings.Weapons[TEN.Objects.WeaponType.PISTOLS].damage = 10
	// TEN.Flow.SetSettings(settings)

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
		// @tfield int alternateDamage For crossbow, specifies damage for explosive ammo.
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
		// @tfield int flashRange specifies the range of the gunflash.
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
}
