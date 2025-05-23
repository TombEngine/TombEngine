#include "framework.h"
#include "Objects/Generic/Object/Pushable/PushableSound.h"

#include "Game/collision/Point.h"
#include "Objects/Generic/Object/Pushable/PushableObject.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;

namespace TEN::Entities::Generic
{
	enum PushableSoundType
	{
		Loop,
		Stop,
		Fall
	};

	struct PushableSoundData
	{
		int LoopSoundID		  = 0;
		int StopSoundID		  = 0;
		int FallImpactSoundID = 0;
	};

	static PushableSoundData GetPushableSoundData(MaterialType material)
	{
		static const auto DEFAULT_SOUND_DATA = PushableSoundData
		{
			SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL
		};

		static const auto SOUND_DATA_MAP = std::unordered_map<MaterialType, PushableSoundData>
		{
			{ MaterialType::Mud, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_MUD, SFX_TEN_PUSHABLES_STOP_MUD, SFX_TEN_PUSHABLES_COLLIDE_MUD } },
			{ MaterialType::Snow, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_SNOW, SFX_TEN_PUSHABLES_STOP_SNOW, SFX_TEN_PUSHABLES_COLLIDE_SNOW } },
			{ MaterialType::Sand, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_SAND, SFX_TEN_PUSHABLES_STOP_SAND, SFX_TEN_PUSHABLES_COLLIDE_SAND } },
			{ MaterialType::Gravel, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_GRAVEL, SFX_TEN_PUSHABLES_STOP_GRAVEL, SFX_TEN_PUSHABLES_COLLIDE_GRAVEL } },
			{ MaterialType::Ice, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_ICE, SFX_TEN_PUSHABLES_STOP_ICE, SFX_TEN_PUSHABLES_COLLIDE_ICE } },
			{ MaterialType::Water, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_WATER, SFX_TEN_PUSHABLES_STOP_WATER, SFX_TEN_PUSHABLES_COLLIDE_WATER } },
			{ MaterialType::Stone, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_STONE, SFX_TEN_PUSHABLES_STOP_STONE, SFX_TEN_PUSHABLES_COLLIDE_STONE } },
			{ MaterialType::Wood, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_WOOD, SFX_TEN_PUSHABLES_STOP_WOOD, SFX_TEN_PUSHABLES_COLLIDE_WOOD } },
			{ MaterialType::Metal, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_METAL, SFX_TEN_PUSHABLES_STOP_METAL, SFX_TEN_PUSHABLES_COLLIDE_METAL } },
			{ MaterialType::Marble, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_MARBLE, SFX_TEN_PUSHABLES_STOP_MARBLE, SFX_TEN_PUSHABLES_COLLIDE_MARBLE } },
			{ MaterialType::Grass, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_GRASS, SFX_TEN_PUSHABLES_STOP_GRASS, SFX_TEN_PUSHABLES_COLLIDE_GRASS } },
			{ MaterialType::Concrete, PushableSoundData{ SFX_TEN_PUSHABLES_MOVE_CONCRETE, SFX_TEN_PUSHABLES_STOP_CONCRETE, SFX_TEN_PUSHABLES_COLLIDE_CONCRETE } },
			{ MaterialType::OldWood, PushableSoundData{ SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL } },
			{ MaterialType::OldMetal, PushableSoundData{ SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL } },
			{ MaterialType::Custom1, PushableSoundData{ SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL } },
			{ MaterialType::Custom2, PushableSoundData{ SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL } },
			{ MaterialType::Custom3, PushableSoundData{ SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL } },
			{ MaterialType::Custom4, PushableSoundData{ SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL } },
			{ MaterialType::Custom5, PushableSoundData{ SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL } },
			{ MaterialType::Custom6, PushableSoundData{ SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL } },
			{ MaterialType::Custom7, PushableSoundData{ SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL } },
			{ MaterialType::Custom8, PushableSoundData{ SFX_TR4_PUSHABLE_SOUND, SFX_TR4_PUSH_BLOCK_END, SFX_TR4_BOULDER_FALL } }
		};

		// TODO: Uncomment when we get actual sounds for them.
		//auto it = SOUND_DATA_MAP.find(material);
		//return ((it != SOUND_DATA_MAP.end()) ? it->second : SOUND_DATA_DEFAULT);

		return DEFAULT_SOUND_DATA;
	}

	static std::optional<int> GetPushableSoundID(const ItemInfo& pushableItem, PushableSoundType soundType)
	{
		// Get floor material.
		auto pointColl = GetPointCollision(pushableItem);
		auto material = pointColl.GetBottomSector().GetSurfaceMaterial(pointColl.GetPosition().x, pointColl.GetPosition().z, true);

		switch (soundType)
		{
		case PushableSoundType::Loop:
			return GetPushableSoundData(material).LoopSoundID;

		case PushableSoundType::Stop:
			return GetPushableSoundData(material).StopSoundID;

		case PushableSoundType::Fall:
			return GetPushableSoundData(material).FallImpactSoundID;
		
		default:
			TENLog("Missing pushable sound.", LogLevel::Error);
		}

		return std::nullopt;
	}

	void HandlePushableSoundState(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);
		
		auto soundID = std::optional<int>(std::nullopt);
		switch (pushable.SoundState)
		{
		default:
		case PushableSoundState::None:
			break;

		case PushableSoundState::Move:
			soundID = GetPushableSoundID(pushableItem, PushableSoundType::Loop);
			break;

		case PushableSoundState::Stop:
			soundID = GetPushableSoundID(pushableItem, PushableSoundType::Stop);
			pushable.SoundState = PushableSoundState::None;
			break;

		case PushableSoundState::Fall:
			soundID = GetPushableSoundID(pushableItem, PushableSoundType::Fall);
			pushable.SoundState = PushableSoundState::None;
			break;

		case PushableSoundState::Wade:
			soundID = SFX_TR4_LARA_WADE;
			pushable.SoundState = PushableSoundState::None;
			break;
		}

		if (!soundID.has_value())
			return;

		SoundEffect(*soundID, &pushableItem.Pose, SoundEnvironment::Always);
	}
}
