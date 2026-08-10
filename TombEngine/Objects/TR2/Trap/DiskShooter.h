#pragma once

namespace TEN::Entities::Traps
{
	void ControlDisk(short itemNumber);
	void ControlDiskShooter(short itemNumber);

	void SpawnDartSmoke(const Vector3& pos, const Vector3& vel, bool isHit);
}
