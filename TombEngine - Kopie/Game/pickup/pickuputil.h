#pragma once

enum GAME_OBJECT_ID : short;

enum class ModificationType
{
	Add,
	Remove, 
	Set
};

// Given an array and an Object ID, iterate through the array until we find
// an ID that matches the ID we've passed in.
template <typename T, size_t N> int GetArraySlot(std::array<T, N> const& arr, GAME_OBJECT_ID objectID)
{
	for (int i = 0; i < arr.size(); ++i)
	{
		if (objectID == arr[i].ObjectID)
			return i;
	}

	return NO_VALUE;
}
