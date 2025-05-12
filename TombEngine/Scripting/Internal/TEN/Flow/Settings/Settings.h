#pragma once

#include "Game/Lara/lara_struct.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
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
		int			Range				= 9;
		int			Timeout				= 60;
		int			PickupCount			= 12;

		static void Register(sol::table& parent);
	};

	struct GraphicsSettings
	{
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
		AnimSettings				Animations = {};
		CameraSettings				Camera	   = {};
		FlareSettings				Flare	   = {};
		GraphicsSettings			Graphics   = {};
		std::array<HairSettings, 3> Hair	   = {};
		HudSettings					Hud		   = {};
		PhysicsSettings				Physics	   = {};
		SystemSettings				System	   = {};
		std::array<WeaponSettings, (int)LaraWeaponType::NumWeapons - 1> Weapons = {};

		Settings();

		static void Register(sol::table& parent);
	};
}