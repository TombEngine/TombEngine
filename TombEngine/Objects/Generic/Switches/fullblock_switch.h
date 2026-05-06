#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Switches
{
	extern unsigned char SequenceUsed[6];
	extern unsigned char SequenceResults[3][3][3];
	extern unsigned char Sequences[3];
	extern unsigned char CurrentSequence;

	void FullBlockSwitchControl(short itemNumber);
	void FullBlockSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
