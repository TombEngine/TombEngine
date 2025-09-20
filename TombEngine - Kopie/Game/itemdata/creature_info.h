#pragma once
#include "Math/Math.h"

struct ItemInfo;

// Default zone loaded by TEN. They are added by TE at compile time.
enum class ZoneType
{
	Skeleton, // Enables jump, also 1 block vault and fall.
	Basic,	  // Enables 1 step vault, 2 step fall (default).

	// TODO: Underwater creatures can go on land like the crocodile, which is wrong since the flag IsAmphibious is not set for them.
	Water, // Enables movement exclusively underwater (exception: crocodile can go on land)
	// Amphibious, // TODO: For later since it's not on level file now.

	Human, // Enables 1 block vault and fall.
	Flyer, // Enables flying anywhere except water rooms.

	MaxZone // Used when loading level.
};

enum class MoodType
{
	Bored,
	Attack,
	Escape,
	Stalk
};

enum class CreatureAIPriority
{
	None,
	Low,
	Medium,
	High
};

struct BoxNode
{
	int exitBox		  = 0;
	int searchNumber  = 0;
	int nextExpansion = 0;
	int boxNumber	  = 0;
};

struct LOTInfo 
{
	bool Initialized = false;

	std::vector<BoxNode> Node = {};
	int Head = 0;
	int Tail = 0;

	ZoneType Zone	= ZoneType::Basic;
	Vector3i Target = Vector3i::Zero;

	int	  TargetBox	   = 0;
	int	  RequiredBox  = 0;
	int	  SearchNumber = 0;
	int	  BlockMask	   = 0;
	short ZoneCount	   = 0;
	short Step		   = 0;
	short Drop		   = 0;
	short Fly		   = 0;

	bool IsAmphibious = false;
	bool IsJumping	  = false;
	bool IsMonkeying  = false;

	bool CanJump	  = false;
	bool CanMonkey	  = false;
};

struct CreatureBiteInfo
{
	Vector3 Position = Vector3::Zero;
	int		BoneID	 = -1;

	CreatureBiteInfo() {}

	CreatureBiteInfo(const Vector3& pos, int boneID)
	{
		Position = pos;
		BoneID = boneID;
	}
};

struct CreatureMuzzleFlashInfo
{
	CreatureBiteInfo Bite = {};

	int	 Delay			 = 0;
	bool SwitchToMuzzle2 = false; // Changes muzzle object to ID_GUNFLASH2.
	bool ApplyXRotation	 = true;  // Applies X axis rotation for muzzleflash (required for creatures).
	bool ApplyZRotation	 = true;  // Applies Y axis rotation for muzzleflash (required for creatures).
	bool UseSmoke		 = true;  // Determines if CreatureAnimation calls TriggerGunSmokeParticles().

	CreatureMuzzleFlashInfo() {}

	CreatureMuzzleFlashInfo(const Vector3& pos, int boneID, int delay, bool changeToMuzzle2 = false)
	{
		Bite = CreatureBiteInfo(pos, boneID);
		Delay = delay;
		SwitchToMuzzle2 = changeToMuzzle2;
	}

	CreatureMuzzleFlashInfo(const CreatureBiteInfo& bite, int delay, bool changeToMuzzle2 = false)
	{
		Bite = bite;
		Delay = delay;
		SwitchToMuzzle2 = changeToMuzzle2;
	}

	CreatureMuzzleFlashInfo(const CreatureBiteInfo& bite, int delay, bool changeToMuzzle2 = false, bool applyXRot = true, bool applyZRot = true)
	{
		Bite = bite;
		Delay = delay;
		SwitchToMuzzle2 = changeToMuzzle2;
		ApplyXRotation = applyXRot;
		ApplyZRotation = applyZRot;
	}
};

struct CreatureInfo 
{
	int ItemNumber = NO_VALUE;

	LOTInfo	  LOT			 = {};
	MoodType  Mood			 = MoodType::Bored;
	ItemInfo* Enemy			 = nullptr;
	ItemInfo* AITarget		 = nullptr;
	int		  AITargetNumber = NO_VALUE;
	Vector3i  Target		 = Vector3i::Zero;

	short MaxTurn		   = 0;
	short JointRotation[4] = {};
	bool  HeadLeft		   = false;
	bool  HeadRight		   = false;

	bool Patrol			  = false; // Unused?
	bool Alerted		  = false;
	bool Friendly		  = false;
	bool HurtByLara		  = false;
	bool Poisoned		  = false;
	bool JumpAhead		  = false;
	bool MonkeySwingAhead = false;
	bool ReachedGoal	  = false;

	CreatureMuzzleFlashInfo MuzzleFlash[2];
	short Tosspad	  = 0;
	short LocationAI  = 0;
	short Flags		  = 0;

	bool IsTargetAlive();

#ifdef CREATURE_AI_PRIORITY_OPTIMIZATION
	CreatureAIPriority Priority = CreatureAIPriority::None;
	size_t FramesSinceLOTUpdate = 0;
#endif
};
