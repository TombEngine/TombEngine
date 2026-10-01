#include "framework.h"
#include "Objects/TR3/Trap/FireBreathingStatue.h"

#include "Game/Animation/Animation.h"
#include "Game/control/trigger.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Math/Math.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Scripting::Properties;

namespace TEN::Entities::Traps
{
	constexpr auto FIRE_BREATHING_STATUE_EMITTER_JOINT = 6;
	constexpr auto FIRE_BREATHING_STATUE_DURATION      = 90;  // frames (~3 Sekunden)
	constexpr auto FIRE_BREATHING_STATUE_FLAME_SPEED   = 256; // anpassbar: Spray-Stärke

	enum FireBreathingStatueState
	{
		FIRE_BREATHING_STATUE_STATE_IDLE = 0,
		FIRE_BREATHING_STATUE_STATE_WINDUP = 1,
		FIRE_BREATHING_STATUE_STATE_SHOOTING = 2,
	};

	enum FireBreathingStatueAnim
	{
		FIRE_BREATHING_STATUE_ANIM_IDLE = 0,
		FIRE_BREATHING_STATUE_ANIM_WINDUP = 1,
		FIRE_BREATHING_STATUE_ANIM_SHOOTING = 2,
	};

	// Returns the local-space spray velocity for the emitter joint that yields a horizontal, forward-facing flame (compensates the slanted mesh).
	static Vector3i GetHorizontalSprayVelocity(const ItemInfo& item, int jointID, int speed)
	{
		auto boneOrient = GetBoneOrientation(item, jointID);
		boneOrient.Normalize();

		auto worldDir = Vector3::Transform(Vector3(1, 0, 0), boneOrient);
		worldDir.z = 0;
		worldDir.x = 0;
		worldDir.Normalize();

		auto inverseOrient = Quaternion::Identity;
		boneOrient.Inverse(inverseOrient);

		auto localVel = Vector3::Transform(worldDir, inverseOrient) * (float)speed;
		return Vector3i((int)localVel.x, (int)localVel.y, (int)localVel.z);
	}

	void InitializeFireBreathingStatue(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		SetAnimation(item, FIRE_BREATHING_STATUE_ANIM_WINDUP);
		item.ItemFlags[0] = FIRE_BREATHING_STATUE_DURATION;
		item.ItemFlags[1] = FIRE_BREATHING_STATUE_EMITTER_JOINT;
	}

	void FireBreathingStatueControl(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];


		switch (item.Animation.ActiveState)
		{
			case FIRE_BREATHING_STATUE_STATE_IDLE:
				if (TriggerActive(&item))
					item.Animation.TargetState = FIRE_BREATHING_STATUE_STATE_WINDUP;
				break;

			case FIRE_BREATHING_STATUE_STATE_WINDUP:
				if (TriggerActive(&item))
					item.ItemFlags[0] = PropertyHandler::Get(item, PropName_EffectDuration, FIRE_BREATHING_STATUE_DURATION);
				else
					item.Animation.TargetState = FIRE_BREATHING_STATUE_STATE_IDLE;
				break;

			case FIRE_BREATHING_STATUE_STATE_SHOOTING:
				if (TriggerActive(&item))
				{
					item.ItemFlags[1] = PropertyHandler::Get(item, PropName_EffectMeshID, FIRE_BREATHING_STATUE_EMITTER_JOINT);
					ThrowFire(itemNumber, item.ItemFlags[1], Vector3i(0, 0, 0), GetHorizontalSprayVelocity(item, item.ItemFlags[1], FIRE_BREATHING_STATUE_FLAME_SPEED));

					if (item.ItemFlags[0] > 0)
						item.ItemFlags[0]--;
					else
						item.Animation.TargetState = FIRE_BREATHING_STATUE_STATE_WINDUP;
				}
				else
					item.Animation.TargetState = FIRE_BREATHING_STATUE_STATE_IDLE;
				break;
		}

		AnimateItem(&item);
	}
}
