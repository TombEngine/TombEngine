#pragma once

struct CollisionInfo;
struct ItemInfo;

enum PulleyStatus
{
	PULLEY_OFF,
	PULLEY_ON,
	PULLEY_WAIT,
	PULLEY_ANIMATE,
	PULLEY_ANIMATE_UNDERWATER
};

namespace TEN::Entities::Switches
{
	void InitializePulleySwitch(short itemNumber);
	void CollisionPulleySwitch(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void ControlPulleySwitch(short itemNumber);
	bool TriggerPulley(short itemNumber, short timer);
}
