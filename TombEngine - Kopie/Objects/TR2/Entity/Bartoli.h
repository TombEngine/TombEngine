#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Creatures::TR2
{
	void InitializeBartoli(short itemNumber);
	void ControlBartoli(short itemNumber);
	void ControlBartoliTransformEffect(short itemNumber);
}
