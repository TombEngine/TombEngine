#pragma once

struct SinkInfo
{
	std::string Name	 = {};
	Vector3		Position = Vector3::Zero;
	int			Strength = 0;
	int			BoxIndex = 0;

	SinkInfo()
	{
	}

	SinkInfo(const Vector3& pos, int strength = 0, int boxIndex = 0)
	{
		Position = pos;
		Strength = strength;
		BoxIndex = boxIndex;
	}
};
