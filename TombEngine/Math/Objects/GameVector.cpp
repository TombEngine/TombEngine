#include "framework.h"
#include "Math/Objects/GameVector.h"

#include "Math/Objects/Vector3i.h"

//namespace TEN::Math
//{
	const GameVector GameVector::Zero = GameVector(0, 0, 0, 0);

	GameVector::GameVector(const Vector3i& pos)
	{
		x = pos.x;
		y = pos.y;
		z = pos.z;
	}
	
	GameVector::GameVector(const Vector3i& pos, short roomNumber)
	{
		x = pos.x;
		y = pos.y;
		z = pos.z;
		RoomNumber = roomNumber;
	}
	
	GameVector::GameVector(int xPos, int yPos, int zPos)
	{
		x = xPos;
		y = yPos;
		z = zPos;
	}

	GameVector::GameVector(int xPos, int yPos, int zPos, short roomNumber)
	{
		x = xPos;
		y = yPos;
		z = zPos;
		RoomNumber = roomNumber;
	}

	Vector3 GameVector::ToVector3() const
	{
		return Vector3(x, y, z);
	}

	Vector3i GameVector::ToVector3i() const
	{
		return Vector3i(x, y, z);
	}

	bool GameVector::operator ==(const GameVector& vector) const
	{
		return ((x == vector.x) && (y == vector.y) && (z == vector.z) && (RoomNumber == vector.RoomNumber));
	}

	bool GameVector::operator !=(const GameVector& vector) const
	{
		return !(*this == vector);
	}

	GameVector& GameVector::operator =(const GameVector& vector)
	{
		x = vector.x;
		y = vector.y;
		z = vector.z;
		RoomNumber = vector.RoomNumber;
		return *this;
	}

	GameVector& GameVector::operator +=(const GameVector& vector)
	{
		x += vector.x;
		y += vector.y;
		z += vector.z;
		return *this;
	}

	GameVector& GameVector::operator -=(const GameVector& vector)
	{
		x -= vector.x;
		y -= vector.y;
		z -= vector.z;
		return *this;
	}

	GameVector& GameVector::operator *=(const GameVector& vector)
	{
		x *= vector.x;
		y *= vector.y;
		z *= vector.z;
		return *this;
	}

	GameVector& GameVector::operator *=(float scalar)
	{
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}

	GameVector& GameVector::operator /=(float scalar)
	{
		x /= scalar;
		y /= scalar;
		z /= scalar;
		return *this;
	}

	GameVector GameVector::operator +(const GameVector& vector) const
	{
		return GameVector(x + vector.x, y + vector.y, z + vector.z, RoomNumber);
	}

	GameVector GameVector::operator -(const GameVector& vector) const
	{
		return GameVector(x - vector.x, y - vector.y, z - vector.z, RoomNumber);
	}

	GameVector GameVector::operator *(const GameVector& vector) const
	{
		return GameVector(x * vector.x, y * vector.y, z * vector.z, RoomNumber);
	}

	GameVector GameVector::operator *(float scalar) const
	{
		return GameVector((int)round(x * scalar), (int)round(y * scalar), (int)round(z * scalar), RoomNumber);
	}

	GameVector GameVector::operator /(float scalar) const
	{
		return GameVector((int)round(x / scalar), (int)round(y / scalar), (int)round(z / scalar), RoomNumber);
	}
//}
