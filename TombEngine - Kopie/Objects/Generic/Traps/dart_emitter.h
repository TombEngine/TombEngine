#pragma once

namespace TEN::Entities::Traps
{
	void InitializeDartEmitter(short itemNumber);
	void ControlDart(short itemNumber);
	void ControlDartEmitter(short itemNumber);

	void SpawnDartSmoke(const Vector3& pos, const Vector3& vel, bool isHit);
}
