#pragma once

class Pose;
struct ItemInfo;
struct ObjectInfo;

namespace TEN::Entities::TR4
{
	void InitializeSeth(short itemNumber);
	void SethControl(short itemNumber);

	void SethProjectileAttack(const Pose& pose, int roomNumber, int flags);
	void SethAttack(int itemNumber);
}
