#include "framework.h"
#include "Game/items.h"

#include "Game/collision/floordata.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/tomb4fx.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Generic/Object/BridgeObject.h"
#include "Objects/Generic/Object/Pushable/PushableInfo.h"
#include "Objects/Generic/Object/Pushable/PushableObject.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Objects/ScriptInterfaceObjectsHandler.h"
#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Scripting/Internal/TEN/Objects/ObjectIDs.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Collision::Room;
using namespace TEN::Control::Volumes;
using namespace TEN::Effects::Items;
using namespace TEN::Entities::Generic;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Utils;
using TEN::Renderer::g_Renderer;

constexpr auto ITEM_DEATH_TIMEOUT = 4 * FPS;

BoundingBox ItemInfo::GetAabb() const
{
	return Geometry::GetAabb(GetObb());
}

BoundingOrientedBox ItemInfo::GetObb() const
{
	auto frameData = GetFrameInterpData(*this);
	auto obb = BoundingOrientedBox();
	BoundingOrientedBox(
		Vector3::Lerp(frameData.Keyframe0.Aabb.Center, frameData.Keyframe1.Aabb.Center, frameData.Alpha),
		Vector3::Lerp(frameData.Keyframe0.Aabb.Extents, frameData.Keyframe1.Aabb.Extents, frameData.Alpha),
		Vector4::UnitY).Transform(obb, 1.0f, Pose.Orientation.ToQuaternion(), Pose.Position.ToVector3());
	return obb;
}

std::vector<BoundingSphere> ItemInfo::GetSpheres() const
{
	return g_Renderer.GetSpheres(Index);
}

bool ItemInfo::TestOcb(short ocbFlags) const
{
	return ((TriggerFlags & ocbFlags) == ocbFlags);
}

void ItemInfo::RemoveOcb(short ocbFlags)
{
	TriggerFlags &= ~ocbFlags;
}

void ItemInfo::ClearAllOcb()
{
	TriggerFlags = 0;
}

bool ItemInfo::TestFlags(int id, short flags) const
{
	if (id < 0 || id > 7)
		return false;

	return (ItemFlags[id] & flags) != 0;
}

bool ItemInfo::TestFlagField(int id, short flags) const
{
	if (id < 0 || id > 7)
		return false;

	return (ItemFlags[id] == flags);
}

short ItemInfo::GetFlagField(int id) const
{
	if (id < 0 || id > 7)
		return 0;

	return ItemFlags[id];
}

void ItemInfo::SetFlagField(int id, short flags)
{
	if (id < 0 || id > 7)
		return;

	ItemFlags[id] = flags;
}

void ItemInfo::ClearFlags(int id, short flags)
{
	if (id < 0 || id > 7)
		return;

	ItemFlags[id] &= ~flags;
}

bool ItemInfo::TestMeshSwapFlags(unsigned int flags)
{
	for (int i = 0; i < Model.MeshIndex.size(); i++)
	{
		if (flags & (1 << i))
		{
			if (Model.MeshIndex[i] == (Model.BaseMesh + i))
				return false;
		}
	}

	return true;
}

bool ItemInfo::TestMeshSwapFlags(const std::vector<unsigned int>& flags)
{
	auto bits = BitField::Default;
	bits.Set(flags);
	return TestMeshSwapFlags(bits.ToPackedBits());
}

void ItemInfo::SetMeshSwapFlags(unsigned int flags, bool clear)
{
	const auto& object = Objects[ObjectNumber];

	bool isMeshSwapPresent = (object.meshSwapSlot != NO_VALUE && Objects[object.meshSwapSlot].loaded);

	for (int i = 0; i < Model.MeshIndex.size(); i++)
	{
		if (isMeshSwapPresent && (flags & (1 << i)))
		{
			if (clear)
			{
				Model.MeshIndex[i] = Model.BaseMesh + i;
			}
			else
			{
				const auto& meshSwapObject = Objects[object.meshSwapSlot];
				Model.MeshIndex[i] = meshSwapObject.meshIndex + i;
			}
		}
		else
		{
			Model.MeshIndex[i] = Model.BaseMesh + i;
		}
	}
}

void ItemInfo::SetMeshSwapFlags(const std::vector<unsigned int>& flags, bool clear)
{
	auto bits = BitField::Default;
	bits.Set(flags);
	SetMeshSwapFlags(bits.ToPackedBits(), clear);
}

void ItemInfo::ResetModelToDefault()
{
	const auto& object = Objects[ObjectNumber];

	if (object.nmeshes > 0)
	{
		Model.MeshIndex.resize(object.nmeshes);
		Model.BaseMesh = object.meshIndex;
		Model.SkinIndex = object.skinIndex;

		for (int i = 0; i < Model.MeshIndex.size(); i++)
			Model.MeshIndex[i] = Model.BaseMesh + i;

		Model.Mutators.resize(object.nmeshes);
		for (auto& mutator : Model.Mutators)
			mutator = {};
	}
	else
	{
		Model.Mutators.clear();
		Model.MeshIndex.clear();
	}
}

bool ItemInfo::IsLara() const
{
	return Data.is<LaraInfo*>();
}

bool ItemInfo::IsCreature() const
{
	return Data.is<CreatureInfo>();
}

bool ItemInfo::IsBridge() const
{
	return Contains(BRIDGE_OBJECT_IDS, ObjectNumber);
}

ItemInfo* ItemHandler::Get() const
{
	if (g_Level.Items.empty() || _index == NO_VALUE)
		return nullptr;

	if (_index < 0 || _index >= g_Level.Items.size())
	{
#if _DEBUG
		TENLog(fmt::format("Attempt to access invalid item index {}.", _index), LogLevel::Warning);
#endif
		return &g_Level.Items[0];
	}

	return &g_Level.Items[_index];
}

ItemHandler& ItemHandler::operator=(ItemInfo* ptr)
{
	if (ptr)
		_index = ptr->Index;
	else
		_index = NO_VALUE;

	return *this;
}

ItemHandler::operator ItemInfo* () const
{
	return Get();
}

ItemInfo* ItemHandler::operator->() const
{
	return static_cast<ItemInfo*>(*this);
}

ItemInfo& ItemHandler::operator*() const
{
	if (_index < 0 || _index >= g_Level.Items.size())
	{
#if _DEBUG
		TENLog(fmt::format("Attempt to dereference invalid item index {}.", _index), LogLevel::Warning);
#endif
		return g_Level.Items[0];
	}

	return g_Level.Items[_index];
}

bool TestState(int refState, const std::vector<int>& stateList)
{
	for (const auto& state : stateList)
	{
		if (state == refState)
			return true;
	}

	return false;
}

static void GameScriptHandleKilled(short itemNumber, bool destroyed)
{
	auto* item = &g_Level.Items[itemNumber];

	g_GameScriptEntities->TryRemoveColliding(itemNumber, true);
	if (!item->Callbacks.OnKilled.empty())
		g_GameScript->ExecuteFunction(item->Callbacks.OnKilled, itemNumber);

	if (destroyed)
	{
		g_GameScriptEntities->NotifyKilled(item);
		item->Name.clear();
		item->Callbacks.OnKilled.clear();
		item->Callbacks.OnHit.clear();
		item->Callbacks.OnObjectCollided.clear();
		item->Callbacks.OnRoomCollided.clear();
	}
}

void KillItem(short const itemNumber)
{
	if (itemNumber < 0 || itemNumber >= g_Level.Items.size())
	{
		TENLog(fmt::format("Attempted to moveable item with invalid index {}.", itemNumber), LogLevel::Error);
		return;
	}

	if (InItemControlLoop)
	{
		ItemNewRooms[2 * ItemNewRoomNo] = itemNumber | 0x8000;
		ItemNewRoomNo++;
	}
	else// if (NextItemActive != NO_VALUE)
	{
		auto* item = &g_Level.Items[itemNumber];

		DetatchSpark(itemNumber, SP_ITEM);
		item->Active = false;

		if (NextItemActive == itemNumber)
		{
			NextItemActive = item->NextActive;
		}
		else
		{
			short linkNumber;
			for (linkNumber = NextItemActive; linkNumber != NO_VALUE; linkNumber = g_Level.Items[linkNumber].NextActive)
			{
				if (g_Level.Items[linkNumber].NextActive == itemNumber)
				{
					g_Level.Items[linkNumber].NextActive = item->NextActive;
					break;
				}
			}
		}

		if (item->RoomNumber != NO_VALUE)
		{
			if (g_Level.Rooms[item->RoomNumber].itemNumber == itemNumber)
			{
				g_Level.Rooms[item->RoomNumber].itemNumber = item->NextItem;
			}
			else
			{
				short linkNumber;
				for (linkNumber = g_Level.Rooms[item->RoomNumber].itemNumber; linkNumber != NO_VALUE; linkNumber = g_Level.Items[linkNumber].NextItem)
				{
					if (g_Level.Items[linkNumber].NextItem == itemNumber)
					{
						g_Level.Items[linkNumber].NextItem = item->NextItem;
						break;
					}
				}
			}
		}

		if (item == Lara.TargetEntity)
			Lara.TargetEntity = nullptr;

		// AI target generation uses a hack with making a dummy item without ObjectNumber.
		// Therefore, a check should be done here to prevent access violation.
		if (item->ObjectNumber != GAME_OBJECT_ID::ID_NO_OBJECT && item->IsBridge())
		{
			auto& bridge = GetBridgeObject(*item);
			bridge.Disable(*item);
		}

		GameScriptHandleKilled(itemNumber, true);

		if (itemNumber >= g_Level.NumItems)
		{
			item->NextItem = NextItemFree;
			NextItemFree = itemNumber;
		}
		else
		{
			item->Flags |= IFLAG_KILLED;
		}
	}
}

void RemoveAllItemsInRoom(short roomNumber, short objectNumber)
{
	auto* room = &g_Level.Rooms[roomNumber];

	short currentItemNumber = room->itemNumber;
	while (currentItemNumber != NO_VALUE)
	{
		auto* item = &g_Level.Items[currentItemNumber];

		if (item->ObjectNumber == objectNumber)
		{
			RemoveActiveItem(currentItemNumber);
			item->Status = ITEM_NOT_ACTIVE;
			item->Flags &= 0xC1;
		}

		currentItemNumber = item->NextItem;
	}
}

void AddActiveItem(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];
	item->Flags |= IFLAG_TRIGGERED;

	if (Objects[item->ObjectNumber].control == nullptr)
	{
		item->Status = ITEM_NOT_ACTIVE;
		return;
	}

	if (!item->Active)
	{
		item->Active = true;
		item->NextActive = NextItemActive;
		NextItemActive = itemNumber;
	}
}

void ItemNewRoom(short itemNumber, short roomNumber)
{
	if (InItemControlLoop)
	{
		ItemNewRooms[2 * ItemNewRoomNo] = itemNumber;
		ItemNewRooms[2 * ItemNewRoomNo + 1] = roomNumber;
		ItemNewRoomNo++;
	}
	else
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->RoomNumber != NO_VALUE)
		{
			auto* room = &g_Level.Rooms[item->RoomNumber];

			if (room->itemNumber == itemNumber)
				room->itemNumber = item->NextItem;
			else
			{
				for (short linkNumber = room->itemNumber; linkNumber != NO_VALUE; linkNumber = g_Level.Items[linkNumber].NextItem)
				{
					if (g_Level.Items[linkNumber].NextItem == itemNumber)
					{
						g_Level.Items[linkNumber].NextItem = item->NextItem;
						break;
					}
				}
			}
		}

		item->RoomNumber = roomNumber;
		item->NextItem = g_Level.Rooms[roomNumber].itemNumber;
		g_Level.Rooms[roomNumber].itemNumber = itemNumber;
	}
}

void EffectNewRoom(short fxNumber, short roomNumber)
{
	if (InItemControlLoop)
	{
		ItemNewRooms[2 * ItemNewRoomNo] = fxNumber;
		ItemNewRooms[2 * ItemNewRoomNo + 1] = roomNumber;
		ItemNewRoomNo++;
	}
	else
	{
		auto* fx = &EffectList[fxNumber];
		auto* room = &g_Level.Rooms[fx->roomNumber];

		if (room->fxNumber == fxNumber)
			room->fxNumber = fx->nextFx;
		else
		{
			for (short linkNumber = room->fxNumber; linkNumber != NO_VALUE; linkNumber = EffectList[linkNumber].nextFx)
			{
				if (EffectList[linkNumber].nextFx == fxNumber)
				{
					EffectList[linkNumber].nextFx = fx->nextFx;
					break;
				}
			}
		}

		fx->roomNumber = roomNumber;
		fx->nextFx = g_Level.Rooms[roomNumber].fxNumber;
		g_Level.Rooms[roomNumber].fxNumber = fxNumber;
	}
}

void KillEffect(short fxNumber)
{
	if (InItemControlLoop)
	{
		ItemNewRooms[2 * ItemNewRoomNo] = fxNumber | 0x8000;
		ItemNewRoomNo++;
	}
	else
	{
		auto* fx = &EffectList[fxNumber];
		DetatchSpark(fxNumber, SP_FX);

		if (NextFxActive == fxNumber)
			NextFxActive = fx->nextActive;
		else
		{
			for (short linkNumber = NextFxActive; linkNumber != NO_VALUE; linkNumber = EffectList[linkNumber].nextActive)
			{
				if (EffectList[linkNumber].nextActive == fxNumber)
				{
					EffectList[linkNumber].nextActive = fx->nextActive;
					break;
				}
			}
		}

		if (g_Level.Rooms[fx->roomNumber].fxNumber == fxNumber)
			g_Level.Rooms[fx->roomNumber].fxNumber = fx->nextFx;
		else
		{
			for (short linkNumber = g_Level.Rooms[fx->roomNumber].fxNumber; linkNumber != NO_VALUE; linkNumber = EffectList[linkNumber].nextFx)
			{
				if (EffectList[linkNumber].nextFx == fxNumber)
				{
					EffectList[linkNumber].nextFx = fx->nextFx;
					break;
				}
			}
		}

		fx->nextFx = NextFxFree;
		NextFxFree = fxNumber;
	}

	// HACK: Garbage collect nextFx if no active effects were detected.
	// This fixes random crashes after spawining multiple FXs (like body part).

	if (NextFxActive == NO_VALUE)
		InitializeFXArray();
}

short CreateNewEffect(short roomNumber) 
{
	short fxNumber = NextFxFree;

	if (NextFxFree != NO_VALUE)
	{
		auto* fx = &EffectList[NextFxFree];

		// HACK: Overcome a self-referencing deadlock by checking if nextFx points to the same index.
		if (fxNumber == fx->nextFx)
			return NO_VALUE;

		NextFxFree = fx->nextFx;

		auto* room = &g_Level.Rooms[roomNumber];

		fx->roomNumber = roomNumber;
		fx->nextFx = room->fxNumber;
		fx->nextActive = NextFxActive;

		NextFxActive = fxNumber;
		room->fxNumber = fxNumber;

		fx->speed = 0;
		fx->color = NEUTRAL_COLOR;
		fx->fallspeed = 0;
		fx->frameNumber = 0;
		fx->counter = 0;
		fx->flag1 = 0;
		fx->flag2 = 0;
		fx->DisableInterpolation = true;
	}

	return fxNumber;
}

void InitializeFXArray()
{
	NextFxActive = NO_VALUE;
	NextFxFree = 0;

	for (int i = 0; i < MAX_SPAWNED_ITEM_COUNT; i++)
		EffectList[i].nextFx = i + 1;

	EffectList[MAX_SPAWNED_ITEM_COUNT - 1].nextFx = NO_VALUE;
}

void RemoveDrawnItem(short itemNumber) 
{
	auto* item = &g_Level.Items[itemNumber];

	if (g_Level.Rooms[item->RoomNumber].itemNumber == itemNumber)
		g_Level.Rooms[item->RoomNumber].itemNumber = item->NextItem;
	else
	{
		for (short linkNumber = g_Level.Rooms[item->RoomNumber].itemNumber; linkNumber != NO_VALUE; linkNumber = g_Level.Items[linkNumber].NextItem)
		{
			if (g_Level.Items[linkNumber].NextItem == itemNumber)
			{
				g_Level.Items[linkNumber].NextItem = item->NextItem;
				break;
			}
		}
	}
}

void RemoveActiveItem(short itemNumber, bool killed) 
{
	if (g_Level.Items[itemNumber].Active)
	{
		g_Level.Items[itemNumber].Active = false;

		if (NextItemActive == itemNumber)
		{
			NextItemActive = g_Level.Items[itemNumber].NextActive;
		}
		else
		{
			for (short linkNumber = NextItemActive; linkNumber != NO_VALUE; linkNumber = g_Level.Items[linkNumber].NextActive)
			{
				if (g_Level.Items[linkNumber].NextActive == itemNumber)
				{
					g_Level.Items[linkNumber].NextActive = g_Level.Items[itemNumber].NextActive;
					break;
				}
			}
		}

		if (killed)
			GameScriptHandleKilled(itemNumber, false);
	}
}

bool IsItemInRoom(short itemNumber, short roomNumber)
{
	const auto& room = g_Level.Rooms[roomNumber];

	// Run through items in room.
	int currentItemNumber = room.itemNumber;
	while (currentItemNumber != NO_VALUE)
	{
		auto& item = g_Level.Items[currentItemNumber];

		if (currentItemNumber == itemNumber)
			return true;

		// HACK: Prevent possible deadlocks.
		if (currentItemNumber == item.NextItem)
			break;

		currentItemNumber = item.NextItem;
	}

	return false;
}

void InitializeItem(short itemNumber) 
{
	auto* item = &g_Level.Items[itemNumber];
	const auto& object = Objects[item->ObjectNumber];

	if (!object.Animations.empty())
		SetAnimation(item, 0);

	item->Animation.RequiredState = NO_VALUE;
	item->Animation.Velocity = Vector3::Zero;
	item->Animation.AnimObjectID = item->ObjectNumber;

	for (int i = 0; i < ITEM_FLAG_COUNT; i++)
		item->ItemFlags[i] = 0;

	item->Active = false;
	item->Status = ITEM_NOT_ACTIVE;
	item->Animation.IsAirborne = false;
	item->HitStatus = false;
	item->Collidable = true;
	item->LookedAt = false;
	item->Timer = 0;
	item->HitPoints = object.HitPoints;

	item->Effect = {};

	if (item->ObjectNumber == ID_HK_ITEM ||
		item->ObjectNumber == ID_HK_AMMO_ITEM ||
		item->ObjectNumber == ID_CROSSBOW_ITEM ||
		item->ObjectNumber == ID_REVOLVER_ITEM)
	{
		item->MeshBits = 1 << 0;
	}
	else
	{
		item->MeshBits = ALL_JOINT_BITS;
	}

	item->TouchBits = NO_JOINT_BITS;
	item->AfterDeath = 0;

	if (item->Flags & IFLAG_INVISIBLE)
	{
		item->Flags &= ~IFLAG_INVISIBLE;
		item->Status = ITEM_INVISIBLE;
	}
	else if (object.intelligent)
	{
		item->Status = ITEM_INVISIBLE;
	}

	if ((item->Flags & IFLAG_ACTIVATION_MASK) == IFLAG_ACTIVATION_MASK)
	{
		item->Flags &= ~IFLAG_ACTIVATION_MASK;
		item->Flags |= IFLAG_REVERSE;
		AddActiveItem(itemNumber);
		item->Status = ITEM_ACTIVE;
	}

	auto* room = &g_Level.Rooms[item->RoomNumber];
	item->NextItem = room->itemNumber;
	room->itemNumber = itemNumber;

	FloorInfo* floor = GetSector(room, item->Pose.Position.x - room->Position.x, item->Pose.Position.z - room->Position.z);
	item->Floor = floor->GetSurfaceHeight(item->Pose.Position.x, item->Pose.Position.z, true);
	item->BoxNumber = floor->PathfindingBoxID;

	item->ResetModelToDefault();

	if (object.Initialize != nullptr)
		object.Initialize(itemNumber);
}

short CreateItem()
{
	if (NextItemFree == NO_VALUE)
		return NO_VALUE;

	short itemNumber = NextItemFree;
	g_Level.Items[NextItemFree].Flags = 0;
	NextItemFree = g_Level.Items[NextItemFree].NextItem;

	g_Level.Items[itemNumber].DisableInterpolation = true;

	return itemNumber;
}

void InitializeItemArray(int totalItem)
{
	g_Level.Items.clear();
	g_Level.Items.resize(totalItem);

	for (int i = 0; i < totalItem; i++)
		g_Level.Items[i].Index = i;

	auto* item = &g_Level.Items[g_Level.NumItems];

	if (g_Level.NumItems + 1 < totalItem)
	{
		for (int i = g_Level.NumItems + 1; i < totalItem; i++, item++)
		{
			item->NextItem = i;
			item->Active = false;
			item->Data = nullptr;
		}
	}

	item->NextItem = NO_VALUE;
	NextItemActive = NO_VALUE;
	NextItemFree = g_Level.NumItems;
}

short SpawnItem(const ItemInfo& item, GAME_OBJECT_ID objectID)
{
	int itemNumber = CreateItem();
	if (itemNumber != NO_VALUE)
	{
		auto& newItem = g_Level.Items[itemNumber];

		newItem.ObjectNumber = objectID;
		newItem.RoomNumber = item.RoomNumber;
		newItem.Pose = item.Pose;
		newItem.Model.Color = NEUTRAL_COLOR;

		InitializeItem(itemNumber);

		newItem.Status = ITEM_NOT_ACTIVE;
	}
	else
	{
		TENLog("Failed to create new item.", LogLevel::Warning);
		itemNumber = NO_VALUE;
	}

	return itemNumber;
}

int GlobalItemReplace(short search, GAME_OBJECT_ID replace)
{
	int changed = 0;
	for (int i = 0; i < g_Level.Rooms.size(); i++)
	{
		auto* room = &g_Level.Rooms[i];

		for (short itemNumber = room->itemNumber; itemNumber != NO_VALUE; itemNumber = g_Level.Items[itemNumber].NextItem)
		{
			if (g_Level.Items[itemNumber].ObjectNumber == search)
			{
				g_Level.Items[itemNumber].ObjectNumber = replace;
				changed++;
			}
		}
	}

	return changed;
}

const std::string& GetObjectName(GAME_OBJECT_ID objectID)
{
	static const auto UNKNOWN_OBJECT = std::string("Unknown Object");

	for (const auto& [name, id] : GAME_OBJECT_IDS)
	{
		if (id == objectID)
			return name;
	}

	return UNKNOWN_OBJECT;
}

std::vector<int> FindAllItems(GAME_OBJECT_ID objectID)
{
	auto itemNumbers = std::vector<int>{};

	for (int i = 0; i < g_Level.NumItems; i++)
	{
		if (g_Level.Items[i].ObjectNumber == objectID)
			itemNumbers.push_back(i);
	}

	return itemNumbers;
}

std::vector<int> FindCreatedItems(GAME_OBJECT_ID objectID)
{
	auto itemNumbers = std::vector<int>{};

	if (NextItemActive == NO_VALUE)
		return itemNumbers;

	const auto* itemPtr = &g_Level.Items[NextItemActive];

	for (int nextActive = NextItemActive; nextActive != NO_VALUE; nextActive = itemPtr->NextActive)
	{
		itemPtr = &g_Level.Items[nextActive];

		if (itemPtr->ObjectNumber == objectID)
			itemNumbers.push_back(nextActive);
	}

	return itemNumbers;
}

ItemInfo* FindItem(GAME_OBJECT_ID objectID)
{
	for (int i = 0; i < g_Level.NumItems; i++)
	{
		auto* item = &g_Level.Items[i];

		if (item->ObjectNumber == objectID)
			return item;
	}

	return nullptr;
}

int FindItem(ItemInfo* item)
{
	if (item == LaraItem)
		return item->Index;

	for (int i = 0; i < g_Level.NumItems; i++)
		if (item == &g_Level.Items[i])
			return i;

	return NO_VALUE;
}

void UpdateAllItems()
{
	InItemControlLoop = true;

	int itemNumber = NextItemActive;
	while (itemNumber != NO_VALUE)
	{
		auto& item = g_Level.Items[itemNumber];
		itemNumber = item.NextActive;

		if (!Objects.CheckID(item.ObjectNumber))
			continue;

		if (g_GameFlow->LastFreezeMode != FreezeMode::None && !Objects[item.ObjectNumber].AlwaysActive)
			continue;

		if (item.AfterDeath <= ITEM_DEATH_TIMEOUT)
		{
			if (Objects[item.ObjectNumber].control)
				Objects[item.ObjectNumber].control(item.Index);

			TestVolumes(item.Index);
			ProcessEffects(&item);

			if (item.AfterDeath > 0 && item.AfterDeath < ITEM_DEATH_TIMEOUT && !(Wibble & 3))
				item.AfterDeath++;
			if (item.AfterDeath == ITEM_DEATH_TIMEOUT)
				KillItem(item.Index);
		}
		else
		{
			KillItem(item.Index);
		}
	}

	InItemControlLoop = false;
	KillMoveItems();
}

void UpdateAllEffects()
{
	InItemControlLoop = true;

	short fxNumber = NextFxActive;
	while (fxNumber != NO_VALUE)
	{
		short nextFx = EffectList[fxNumber].nextActive;
		auto* fx = &EffectList[fxNumber];
		if (Objects[fx->objectNumber].control)
			Objects[fx->objectNumber].control(fxNumber);

		fxNumber = nextFx;
	}

	InItemControlLoop = false;
	KillMoveEffects();
}

bool UpdateItemRoom(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];
	auto yOffset = GameBoundingBox(item).GetCenter().y;

	auto roomNumber = GetPointCollision(
		Vector3i(item->Pose.Position.x, item->Pose.Position.y + yOffset, item->Pose.Position.z),
		item->RoomNumber).GetRoomNumber();

	if (roomNumber != item->RoomNumber)
	{
		ItemNewRoom(itemNumber, roomNumber);
		return true;
	}

	return false;
}

void DoDamage(ItemInfo* item, int damage, bool silent)
{
	static int lastHurtTime = 0;

	if (item->HitPoints <= 0)
		return;

	if (item->IsLara() && GetLaraInfo(*item).Control.WaterStatus == WaterStatus::FlyCheat)
		return;
	
	const int oldHitPoints = item->HitPoints;

	item->HitStatus = true;
	item->HitPoints -= damage;

	if (item->HitPoints <= 0)
	{
		item->HitPoints = 0;

		if (!item->IsLara())
		{
			SaveGame::Statistics.Level.Kills++;
			SaveGame::Statistics.Game.Kills++;
		}
	}

	if (item->IsLara())
	{
		if (damage > 0)
		{
			if (!silent)
			{
				float power = item->HitPoints ? Random::GenerateFloat(0.1f, 0.4f) : 0.5f;
				Rumble(power, 0.15f);
			}

			int damageDelta = std::max(0, oldHitPoints - item->HitPoints);
			SaveGame::Statistics.Game.DamageTaken += damageDelta;
			SaveGame::Statistics.Level.DamageTaken += damageDelta;
		}

		if (!silent && (GlobalCounter - lastHurtTime) > (FPS * 2 + Random::GenerateInt(0, FPS)))
		{
			SoundEffect(SFX_TR4_LARA_INJURY, &LaraItem->Pose);
			lastHurtTime = GlobalCounter;
		}
	}
}

void DoItemHit(ItemInfo* target, int damage, bool isExplosive, bool allowBurn)
{
	const auto& object = Objects[target->ObjectNumber];

	if ((object.damageType == DamageMode::Any) ||
		(object.damageType == DamageMode::Explosion && isExplosive))
	{
		if (target->HitPoints > 0)
		{
			SaveGame::Statistics.Game.AmmoHits++;
			SaveGame::Statistics.Level.AmmoHits++;
			DoDamage(target, damage);
		}
	}

	if (isExplosive && allowBurn && Random::TestProbability(1 / 2.0f))
		ItemBurn(target);

	if (!target->Callbacks.OnHit.empty())
	{
		short index = g_GameScriptEntities->GetIndexByName(target->Name);

		if (index != NO_VALUE)
			g_GameScript->ExecuteFunction(target->Callbacks.OnHit, index);
	}
}

void DefaultItemHit(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
{
	const auto& object = Objects[target.ObjectNumber];

	if (object.hitEffect != HitEffect::None && pos.has_value())
	{
		switch (object.hitEffect)
		{
		case HitEffect::Blood:
			DoBloodSplat(pos->x, pos->y, pos->z, Random::GenerateInt(4, 8), target.Pose.Orientation.y, target.RoomNumber);
			break;

		case HitEffect::Richochet:
			TriggerRicochetSpark(pos.value(), source.Pose.Orientation.y);
			break;

		case HitEffect::Smoke:
			TriggerShatterSmoke(pos.value().x, pos.value().y, pos.value().z);
			break;

		case HitEffect::NonExplosive:
			DoBloodSplat(pos->x, pos->y, pos->z, Random::GenerateInt(4, 8), target.Pose.Orientation.y, target.RoomNumber);
			break;
		}
	}

	DoItemHit(&target, damage, isExplosive);
}

void SyncItemAnimation(ItemInfo& item0, const ItemInfo& item1)
{
	SetAnimation(item0, item1.Animation.AnimNumber, item1.Animation.FrameNumber);
}
