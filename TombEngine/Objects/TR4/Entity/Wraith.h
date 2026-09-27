#pragma once

class Vector3i;
struct ItemInfo;

namespace TEN::Entities::TR4
{
	void InitializeWraith(short itemNumber);
	void WraithControl(short itemNumber);

	void DrawWraith(Vector3i pos, Vector3i velocity, int objectNumber, const Vector4& wraith4Color = Vector4::Zero);
	void KillWraith(ItemInfo* item);
}
