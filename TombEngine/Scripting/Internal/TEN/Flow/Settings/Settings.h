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
		bool	MoveableAvoidance			= true;		// Avoid moveable obstacles.
		bool	StaticMeshAvoidance			= true;		// Avoid static mesh obstacles.
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
		SystemSettings				System	    = {};
		UISettings					UI		    = {};
		std::array<WeaponSettings, (int)LaraWeaponType::NumWeapons - 1> Weapons = {};

		Settings();

		static void Register(sol::table& parent);
	};
}