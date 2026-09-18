#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	void SetupFullBlockSwitch();
	void FullBlockSwitchControl(short itemNumber, byte switchIndex);
	void FullBlockSwitch1Control(short itemNumber);
	void FullBlockSwitch2Control(short itemNumber);
	void FullBlockSwitch3Control(short itemNumber);
	void FullBlockSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
