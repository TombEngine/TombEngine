#pragma once

#include "Game/control/box.h"
#include "Game/Lara/lara_struct.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/TEN/Strings/DisplayString/DisplayString.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"
#include "Specific/clock.h"

using namespace TEN::Scripting::Types;

namespace TEN::Scripting
{
	struct AnimSettings
	{
		int PoseTimeout = 20; // AFK pose timeout.

		bool SlideExtended	= false; // Extended slope sliding functionality (not ready yet).
		bool SprintJump		= false; // Sprint jump.
		bool CrawlspaceDive = true;	 // Dive into crawlspaces.
		bool CrawlExtended	= true;	 // Extended crawl moveset.
		bool CrouchRoll		= true;	 // Crouch roll.
		bool OverhangClimb	= false; // Overhang functionality.
		bool LedgeJumps		= false; // Jump up or back from a ledge.

		static void Register(sol::table& parent);
	};

	struct CameraSettings
	{
		ScriptColor BinocularLightColor	 = { 192, 192, 96 };
		ScriptColor LasersightLightColor = { 255, 0, 0 };
		bool		ObjectCollision		 = true;

		static void Register(sol::table& parent);
	};

	struct FlareSettings
	{
		ScriptColor Color				= ScriptColor(128, 64, 0);
		Vec3		Offset				= Vec3(0, 0, 41);
		float		LensflareBrightness = 0.5f;
		bool		Sparks				= true;
		bool		Smoke				= true;
		bool		Flicker				= true;
		bool		MuzzleGlow			= false;
		int			Range				= 9;
		int			Timeout				= 60;
		int			PickupCount			= 12;

		static void Register(sol::table& parent);
	};

	struct GameplaySettings
	{
		bool TargetObjectOcclusion = true;
		bool KillPoisonedEnemies = true;
		bool EnableInventory = true;

		static void Register(sol::table& parent);
	};

	struct GraphicsSettings
	{
		bool AmbientOcclusion = true;
		bool Skinning = true;

		static void Register(sol::table& parent);
	};

	struct HairSettings
	{
		int				 RootMesh = LM_HEAD;
		Vec3			 Offset	  = {};
		std::vector<int> Indices  = {};

		static void Register(sol::table& parent);
	};

	struct HudSettings
	{
		bool StatusBars		= true;
		bool LoadingBar		= true;
		bool Speedometer	= true;
		bool PickupNotifier = true;

		static void Register(sol::table& parent);
	};

	struct PathfindingSettings
	{
		PathfindingMode Mode = PathfindingMode::AStar;	// Pathfinding algorithm.

		int		SearchDepth					= 5;		// Pathfinding search depth.
		int		EscapeDistance				= BLOCK(5);	// Escape distance.
		int		StalkDistance				= BLOCK(3);	// Stalk distance.
		float	PredictionFactor			= 15.0f;	// Prediction distance scale.
		float	CollisionPenaltyThreshold	= 1.0f;		// Penalty threshold in seconds.
		float	CollisionPenaltyCooldown	= 6.0f;		// Penalty cooldown in seconds.
		bool	MoveableAvoidance			= false;	// Avoid moveable obstacles.
		bool	StaticMeshAvoidance			= false;	// Avoid static mesh obstacles.
		bool	VerticalGeometryAvoidance	= true;		// Avoid geometry obstacles for swimming or flying creatures.
		bool	WaterSurfaceAvoidance		= true;		// Avoid water surface for swimming or flying creatures.
		bool	VerticalMovementSmoothing = true;		// Smooth vertical movement for swimming or flying creatures.

		static void Register(sol::table& parent);
	};

	struct PhysicsSettings
	{
		float Gravity	   = 6.0f;
		float SwimVelocity = 50.0f;

		static void Register(sol::table& parent);
	};

	struct SnowSettings
	{
		// Enables the deformable snow overlay system.
		bool  Enabled         = false;

		// Maximum visual depth of the snow surface above the sector floor, in world units.
		// Lara's foot/body pushes down into this depth as a heightmap.
		int   MaxDepth        = 192;

		// Snow depth (world units) at which Lara enters the wade state and walks through snow like water.
		// When MaxDepth is below this, Lara only leaves footprints.
		int   WadeThreshold   = 512;

		// Per-frame snow recovery factor toward undeformed state (0.0 = persistent trails, 1.0 = instant).
		float DecayRate       = 0.0f;

		// World-space radius (units) around Lara within which the deformation heightmap is valid.
		// Larger values give longer-lasting visible trails but cost more VRAM-resolution per unit.
		int   FieldRadius     = 8192;

		// Subdivisions per sector edge for the snow overlay mesh. 16 = 256 quads per sector.
		// Higher values give smoother trails but more vertex load. Practical range: 4-32.
		int   Subdivisions    = 16;

		// Snow surface tint (applied on top of the underlying sector texture).
		ScriptColor Tint      = ScriptColor(245, 248, 255);

		// Brightness of the rim highlight along the edge of a deformation trail.
		float RimStrength     = 0.6f;

		// Maximum amplitude of the procedural micro-hills baked into pristine snow,
		// in world units. 0 = perfectly flat surface. The hills fade out smoothly
		// where the snow is deformed (footprints, explosions) so trails stay clean.
		float HillHeight      = 64.0f;

		// Spatial frequency of the procedural hill noise, in radians per world unit.
		// Lower = larger, gentler mounds. Higher = tighter ripples.
		float HillFrequency   = 0.0015f;

		static void Register(sol::table& parent);
	};

	struct SystemSettings
	{
		ErrorMode ErrorMode		= ErrorMode::Warn;
		bool	  FastReload	= true;
		bool	  Multithreaded = true;

		static void Register(sol::table& parent);
	};

	struct UISettings
	{
		ScriptColor HeaderTextColor		= ScriptColor(216, 117, 49);	// Orange
		ScriptColor OptionTextColor		= ScriptColor(240, 220, 32);	// Yellow
		ScriptColor PlainTextColor		= ScriptColor(255, 255, 255);	// White
		ScriptColor DisabledTextColor	= ScriptColor(128, 128, 128);	// Gray
		ScriptColor ShadowTextColor		= ScriptColor(0, 0, 0);			// Black

		Vec2 TitleMenuPosition = Vec2(50, 66);
		float TitleMenuScale = 1.0f;
		sol::optional<DisplayStringOptions>	TitleMenuAlignment = DisplayStringOptions::Center;

		Vec2 TitleLogoPosition = Vec2(50, 20);
		float TitleLogoScale = 0.38f;
		ScriptColor TitleLogoColor = ScriptColor(255, 255, 255);

		static void Register(sol::table& parent);
	};

	struct WeaponSettings
	{
		float Accuracy = 0.0f;
		float Distance = BLOCK(8);
	
		int   Interval        = 0;
		int	  WaterLevel      = 0;
		int	  Damage          = 0;
		int	  AlternateDamage = 0;
		int   PickupCount     = 0;

		ScriptColor FlashColor	  = ScriptColor(192, 128, 0);
		int			FlashRange	  = 12;
		int			FlashDuration = 0;

		bool Smoke				 = false;
		bool Shell				 = false;
		bool MuzzleFlash		 = true;
		bool MuzzleGlow			 = true;
		bool ColorizeMuzzleFlash = false;
		Vec3 MuzzleOffset = {};

		static void Register(sol::table& parent);
	};

	struct Settings
	{
		AnimSettings				Animations  = {};
		CameraSettings				Camera	    = {};
		FlareSettings				Flare	    = {};
		GameplaySettings			Gameplay    = {};
		GraphicsSettings			Graphics    = {};
		std::array<HairSettings, 3> Hair	    = {};
		HudSettings					Hud		    = {};
		PathfindingSettings			Pathfinding = {};
		PhysicsSettings				Physics	    = {};
		SnowSettings				Snow	    = {};
		SystemSettings				System	    = {};
		UISettings					UI		    = {};
		std::array<WeaponSettings, (int)LaraWeaponType::NumWeapons - 1> Weapons = {};

		Settings();

		static void Register(sol::table& parent);
	};
}