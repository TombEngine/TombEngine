#pragma once

enum GAME_OBJECT_ID : short;
class EulerAngles;
class Pose;
struct ItemInfo;
struct ObjectInfo;

//namespace TEN::Math
//{
	class GameBoundingBox
	{
	public:
		// Fields

		int X1 = 0;
		int X2 = 0;
		int Y1 = 0;
		int Y2 = 0;
		int Z1 = 0;
		int Z2 = 0;

		// Constants

		static const GameBoundingBox Zero;

		// Constructors

		GameBoundingBox() = default;
		GameBoundingBox(float x1, float x2, float y1, float y2, float z1, float z2);
		GameBoundingBox(const BoundingBox& aabb);
		GameBoundingBox(GAME_OBJECT_ID objectID, int animNumber = 0, int frameNumber = 0);
		GameBoundingBox(const ItemInfo* item);

		// Getters

		int		GetWidth() const;
		int		GetHeight() const;
		int		GetDepth() const;
		Vector3 GetCenter() const;
		Vector3 GetExtents() const;

		// Utilities

		void Rotate(const EulerAngles& rot);

		// Converters

		BoundingSphere		ToLocalBoundingSphere() const;
		BoundingBox			ToConservativeBoundingBox(const Pose& pose) const; // TODO: item.GetAabb() method.
		BoundingOrientedBox ToBoundingOrientedBox(const Pose& pose) const;
		BoundingOrientedBox ToBoundingOrientedBox(const Vector3& pos, const Quaternion& orient) const; // TODO: item.GetObb() method.

		// Operators

		GameBoundingBox operator +(const GameBoundingBox& bounds) const;
		GameBoundingBox operator +(const Pose& pose) const;
		GameBoundingBox operator -(const GameBoundingBox& bounds) const;
		GameBoundingBox operator -(const Pose& pose) const;
		GameBoundingBox operator *(float scalar) const;
		GameBoundingBox operator /(float scalar) const;
	};
//}
