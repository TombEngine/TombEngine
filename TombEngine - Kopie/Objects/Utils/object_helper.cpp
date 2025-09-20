#include "framework.h"
#include "Objects/Utils/object_helper.h"

#include "Game/collision/collide_item.h"
#include "Game/Lara/lara_flare.h"
#include "Game/pickup/pickup.h"
#include "Objects/Generic/Object/objects.h"
#include "Objects/Generic/puzzles_keys.h"
#include "Objects/Generic/Object/Pushable/PushableObject.h"
#include "Specific/level.h"

void AssignObjectMeshSwap(ObjectInfo& object, int requiredMeshSwap, const std::string& baseName, const std::string& requiredName)
{
	if (Objects[requiredMeshSwap].loaded)
		object.meshSwapSlot = requiredMeshSwap;
	else
		TENLog("Slot " + requiredName + " not loaded. Meshswap issues with " + baseName + " may result in incorrect behaviour.", LogLevel::Warning);
}

bool AssignObjectAnimations(ObjectInfo& object, int requiredObjectID, const std::string& baseName, const std::string& requiredName)
{
	// Check if object has at least 1 animation with more than 1 frame.
	const auto& anim = GetAnimData(object.animIndex);
	if ((anim.frameEnd - anim.frameBase) > 1)
		return true;

	// Use slot if loaded.
	const auto& requiredObject = Objects[requiredObjectID];
	if (requiredObject.loaded)
	{
		// Check if the required object has at least 1 animation with more than 1 frame.
		const auto& newAnim = GetAnimData(requiredObject.animIndex);
		if ((newAnim.frameEnd - newAnim.frameBase) > 1)
		{
			object.animIndex = requiredObject.animIndex;
			object.frameBase = requiredObject.frameBase;
			return true;
		}
		else
		{
			TENLog("Slot " + requiredName + " has no animation data. " + baseName + " will have no animations.", LogLevel::Warning);
		}
	}
	else
	{
		TENLog("Slot " + requiredName + " not loaded. " + baseName + " will have no animations.", LogLevel::Warning);
	}

	return false;
}

bool CheckIfSlotExists(GAME_OBJECT_ID requiredObj, const std::string& baseName)
{
	bool result = Objects[requiredObj].loaded;

	if (!result)
	{
		TENLog("Slot " + GetObjectName(requiredObj) + " (" + std::to_string(requiredObj) + ") not loaded. " + 
				baseName + " may not work.", LogLevel::Warning);
	}

	return result;
}

void InitSmashObject(ObjectInfo* object, int objectNumber)
{
	object = &Objects[objectNumber];
	if (object->loaded)
	{
		object->Initialize = InitializeSmashObject;
		object->collision = ObjectCollision;
		object->control = SmashObjectControl;
		object->SetHitEffect(true);
	}
}

void InitKeyHole(ObjectInfo* object, int objectNumber)
{
	object = &Objects[objectNumber];
	if (object->loaded)
	{
		object->collision = KeyHoleCollision;
		object->SetHitEffect(true);
	}
}

void InitPuzzleHole(ObjectInfo* object, int objectNumber)
{
	object = &Objects[objectNumber];
	if (object->loaded)
	{
		object->Initialize = InitializePuzzleHole;
		object->collision = PuzzleHoleCollision;
		object->control = AnimatingControl;
		object->isPuzzleHole = true;
		object->SetHitEffect(true);
	}
}

void InitPuzzleDone(ObjectInfo* object, int objectNumber)
{
	object = &Objects[objectNumber];
	if (object->loaded)
	{
		object->Initialize = InitializePuzzleDone;
		object->collision = PuzzleDoneCollision;
		object->control = AnimatingControl;
		object->SetHitEffect(true);
	}
}

void InitAnimating(ObjectInfo* object, int objectNumber)
{
	object = &Objects[objectNumber];
	if (object->loaded)
	{
		object->Initialize = InitializeAnimating;
		object->control = AnimatingControl;
		object->collision = ObjectCollision;
		object->SetHitEffect(true);
	}
}

void InitPickup(ObjectInfo* object, int objectNumber)
{
	object = &Objects[objectNumber];
	if (object->loaded)
	{
		object->Initialize = InitializePickup;
		object->collision = PickupCollision;
		object->control = PickupControl;
		object->isPickup = true;
		object->SetHitEffect(true);
	}
}

void InitPickup(ObjectInfo* object, int objectNumber, std::function<ControlFunction> func)
{
	object = &Objects[objectNumber];
	if (object->loaded)
	{
		object->Initialize = InitializePickup;
		object->collision = PickupCollision;
		object->control = (func != nullptr) ? func : PickupControl;
		object->isPickup = true;
		object->SetHitEffect(true);
	}
}

void InitFlare(ObjectInfo* object, int objectNumber)
{
	object = &Objects[objectNumber];
	if (object->loaded)
	{
		object->collision = PickupCollision;
		object->control = FlareControl;
		object->pivotLength = 256;
		object->usingDrawAnimatingItem = false;
		object->isPickup = true;
	}
}

void InitProjectile(ObjectInfo* object, std::function<InitFunction> func, int objectNumber, bool noLoad)
{
	object = &Objects[objectNumber];
	if (object->loaded || noLoad)
	{
		object->Initialize = nullptr;
		object->collision = nullptr;
		object->control = func;
	}
}

void InitSearchObject(ObjectInfo* object, int objectNumber)
{
	object = &Objects[objectNumber];
	if (object->loaded)
	{
		object->Initialize = InitializeSearchObject;
		object->collision = SearchObjectCollision;
		object->control = SearchObjectControl;
	}
}

void InitPushableObject(ObjectInfo* object, int objectNumber)
{
	object = &Objects[objectNumber];
	if (object->loaded)
	{
		object->Initialize = InitializePushableBlock;
		object->control = ControlPushableBlock;
		object->collision = CollidePushableBlock;
		object->SetHitEffect(true);
	}
}
