#pragma once

class Vector3i;

//namespace TEN::Math
//{
	class GameVector
	{
	public:
		// Fields

		int	  x			 = 0;
		int	  y			 = 0;
		int	  z			 = 0;
		short RoomNumber = 0;

		// Constants

		static const GameVector Zero;

		// Constructors

		GameVector() = default;
		GameVector(const Vector3i& pos);
		GameVector(const Vector3i& pos, short roomNumber);
		GameVector(int xPos, int yPos, int zPos);
		GameVector(int xPos, int yPos, int zPos, short roomNumber);

		// Converters

		Vector3	 ToVector3() const;
		Vector3i ToVector3i() const;

		// Operators

		bool		operator ==(const GameVector& vector) const;
		bool		operator !=(const GameVector& vector) const;
		GameVector& operator =(const GameVector& vector);
		GameVector& operator +=(const GameVector& vector);
		GameVector& operator -=(const GameVector& vector);
		GameVector& operator *=(const GameVector& vector);
		GameVector& operator *=(float scalar);
		GameVector& operator /=(float scalar);
		GameVector	operator +(const GameVector& vector) const;
		GameVector	operator -(const GameVector& vector) const;
		GameVector	operator *(const GameVector& vector) const;
		GameVector	operator *(float scalar) const;
		GameVector	operator /(float scalar) const;
	};
//}
