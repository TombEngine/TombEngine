#pragma once

#include "Game/Animation/Animation.h"
#include "Game/itemdata/itemdata.h"
#include "Math/Math.h"
#include "Specific/BitField.h"
#include "Objects/game_object_ids.h"
#include "Specific/newtypes.h"

using namespace TEN::Animation;
using namespace TEN::Utils;

constexpr auto MAX_SPAWNED_ITEM_COUNT = 256;
constexpr auto ITEM_FLAG_COUNT = 8;

constexpr auto NOT_TARGETABLE = SHRT_MIN / 2;

constexpr auto ALL_JOINT_BITS = UINT_MAX;
constexpr auto NO_JOINT_BITS  = 0u;

enum class EffectType
{
	None,
	Fire,
	Sparks,
	Smoke,
	ElectricIgnite,
	RedIgnite,
	Custom
};

enum ItemStatus
{
	ITEM_NOT_ACTIVE	 = 0,
	ITEM_ACTIVE		 = 1,
	ITEM_DEACTIVATED = 2,
	ITEM_INVISIBLE	 = 3
};

enum ItemFlags
{
	IFLAG_TRIGGERED       = 1 << 5,
	IFLAG_CLEAR_BODY	  = 1 << 7,
	IFLAG_INVISIBLE		  = 1 << 8,
	IFLAG_ACTIVATION_MASK = 0x1F << 9, // Bits 9-13 (IFLAG_CODEBITS)
	IFLAG_REVERSE		  = 1 << 14,
	IFLAG_KILLED		  = 1 << 15
};

enum AIObjectType
{
	NO_AI	  = 0,
	GUARD	  = 1 << 0,
	AMBUSH	  = 1 << 1,
	PATROL1	  = 1 << 2,
	MODIFY	  = 1 << 3,
	FOLLOW	  = 1 << 4,
	PATROL2	  = 1 << 5,
	ALL_AIOBJ = GUARD | AMBUSH | PATROL1 | MODIFY | FOLLOW | PATROL2
};

struct EntityAnimationData
{
	GAME_OBJECT_ID AnimObjectID = ID_NO_OBJECT;

	int AnimNumber	  = 0;
	int FrameNumber	  = 0;
	int ActiveState	  = 0;
	int TargetState	  = 0;
	int RequiredState = NO_VALUE;

	// TODO: Have 3 velocity members:
	// ControlVelocity:		 relative velocity derived from animation.
	// ExtraControlVelocity: relative velocity set by code (used to control swimming, falling).
	// ExternalVelocity:	 absolute velocity set by environment (slippery ice, offset blending).
	Vector3 Velocity = Vector3::Zero; // CONVENTION: +X = Right, +Y = Down, +Z = Forward.

	bool IsAirborne = false;
};

struct EntityCallbackData
{
	std::string OnKilled		 = {};
	std::string OnHit			 = {};
	std::string OnObjectCollided = {};
	std::string OnRoomCollided	 = {};
};

struct EntityEffectData
{
	EffectType Type					= EffectType::None;
	Vector3	   LightColor			= Vector3::Zero;
	Vector3	   PrimaryEffectColor	= Vector3::Zero;
	Vector3	   SecondaryEffectColor = Vector3::Zero;
	int		   Count				= NO_VALUE;
};

struct EntityModelData
{
	int BaseMesh = 0;

	std::vector<int>		 MeshIndex = {};
	std::vector<BoneMutator> Mutators  = {};

	Vector4 Color = Vector4::Zero;
};

struct ItemInfo
{
	std::string	   Name			= {};
	int			   Index		= 0;			// ItemNumber // TODO: Make int.
	GAME_OBJECT_ID ObjectNumber = ID_NO_OBJECT; // ObjectID

	ItemStatus Status = ITEM_NOT_ACTIVE;
	bool	   Active = false;

	// TODO: Refactor linked list.
	int NextItem   = 0;
	int NextActive = 0;

	ItemData			Data	  = {};
	EntityAnimationData Animation = {};
	EntityCallbackData	Callbacks = {};
	EntityEffectData	Effect	  = {};
	EntityModelData		Model	  = {};

	Pose	   StartPose  = Pose::Zero;
	Pose	   Pose		  = Pose::Zero;
	RoomVector Location	  = {}; // NOTE: Describes vertical position in room.
	short	   RoomNumber = 0; // TODO: Make int.
	int		   Floor	  = 0;

	int	 HitPoints			  = 0;
	bool HitStatus			  = false;
	bool LookedAt			  = false;
	bool Collidable			  = false;
	bool InDrawRoom			  = false;
	bool DisableInterpolation = false;

	int BoxNumber = 0;
	int Timer	  = 0;

	BitField TouchBits = BitField::Default; // TouchFlags
	BitField MeshBits  = BitField::Default; // MeshFlags

	std::array<short, ITEM_FLAG_COUNT> ItemFlags = {};
	unsigned short Flags		= 0; // ItemFlags enum
	short		   TriggerFlags = 0;

	// TODO: Move to CreatureInfo?
	unsigned char AIBits	  = 0; // AIObjectFlags enum.
	short		  AfterDeath  = 0;
	short		  CarriedItem = 0;

	// OCB utilities

	bool TestOcb(short ocbFlags) const;
	void RemoveOcb(short ocbFlags);
	void ClearAllOcb();

	// ItemFlags utilities

	bool  TestFlags(int id, short flags) const;		// ItemFlags[id] & flags
	bool  TestFlagField(int id, short flags) const; // ItemFlags[id] == flags
	short GetFlagField(int id) const;				// ItemFlags[id]
	void  SetFlagField(int id, short flags);		// ItemFlags[id] = flags
	void  ClearFlags(int id, short flags);			// ItemFlags[id] &= ~flags

	// Model utilities

	bool TestMeshSwapFlags(unsigned int flags);
	bool TestMeshSwapFlags(const std::vector<unsigned int>& flags);
	void SetMeshSwapFlags(unsigned int flags, bool clear = false);
	void SetMeshSwapFlags(const std::vector<unsigned int>& flags, bool clear = false);
	void ResetModelToDefault();

	// Inquirers

	bool IsLara() const;
	bool IsCreature() const;
	bool IsBridge() const;

	// Getters

	BoundingBox					GetAabb() const;
	BoundingOrientedBox			GetObb() const;
	std::vector<BoundingSphere> GetSpheres() const;
};

bool TestState(int refState, const std::vector<int>& stateList);
void EffectNewRoom(short fxNumber, short roomNumber);
void ItemNewRoom(short itemNumber, short roomNumber);
void AddActiveItem(short itemNumber);
short CreateItem();
void RemoveAllItemsInRoom(short roomNumber, short objectNumber);
void RemoveActiveItem(short itemNumber, bool killed = true);
void RemoveDrawnItem(short itemNumber);
void InitializeFXArray();
short CreateNewEffect(short roomNumber);
void KillEffect(short fxNumber);
void InitializeItem(short itemNumber);
void InitializeItemArray(int totalItems);
void KillItem(short itemNumber);
bool UpdateItemRoom(short itemNumber);
void UpdateAllItems();
void UpdateAllEffects();
const std::string& GetObjectName(GAME_OBJECT_ID objectID);
std::vector<int> FindAllItems(GAME_OBJECT_ID objectID);
std::vector<int> FindCreatedItems(GAME_OBJECT_ID objectID);
ItemInfo* FindItem(GAME_OBJECT_ID objectID);
int FindItem(ItemInfo* item);
void DoDamage(ItemInfo* item, int damage, bool silent = false);
void DoItemHit(ItemInfo* target, int damage, bool isExplosive, bool allowBurn = true);
void DefaultItemHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex);
short SpawnItem(const ItemInfo& item, GAME_OBJECT_ID objectID);

Vector3i GetNearestSectorCenter(const Vector3i& pos);
