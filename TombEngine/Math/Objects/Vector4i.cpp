#include "framework.h"
#include "Math/Objects/Vector4i.h"

//namespace TEN::Math
//{
const Vector4i Vector4i::Zero = Vector4i(0, 0, 0, 0);

Vector4i::Vector4i(const Vector4& vector)
{
	x = (int)round(vector.x);
	y = (int)round(vector.y);
	z = (int)round(vector.z);
	w = (int)round(vector.w);
}

float Vector4i::Distance(const Vector4i& origin, const Vector4i& target)
{
	return std::sqrt(DistanceSquared(origin, target));
}

float Vector4i::DistanceSquared(const Vector4i& origin, const Vector4i& target)
{
	return (SQUARE(target.x - origin.x) +
		SQUARE(target.y - origin.y) +
		SQUARE(target.z - origin.z) +
		SQUARE(target.w - origin.w));
}

void Vector4i::Lerp(const Vector4i& target, float alpha)
{
	*this = Lerp(*this, target, alpha);
}

Vector4i Vector4i::Lerp(const Vector4i& origin, const Vector4i& target, float alpha)
{
	return Vector4i(Vector4(
		(1.0f - alpha) * origin.x + alpha * target.x,
		(1.0f - alpha) * origin.y + alpha * target.y,
		(1.0f - alpha) * origin.z + alpha * target.z,
		(1.0f - alpha) * origin.w + alpha * target.w));
}

Vector4 Vector4i::ToVector4() const
{
	return Vector4((float)x, (float)y, (float)z, (float)w);
}

bool Vector4i::operator ==(const Vector4i& vector) const
{
	return ((x == vector.x) && (y == vector.y) && (z == vector.z) && (w == vector.w));
}

bool Vector4i::operator !=(const Vector4i& vector) const
{
	return !(*this == vector);
}

Vector4i& Vector4i::operator =(const Vector4i& vector)
{
	x = vector.x;
	y = vector.y;
	z = vector.z;
	w = vector.w;
	return *this;
}

Vector4i& Vector4i::operator +=(const Vector4i& vector)
{
	x += vector.x;
	y += vector.y;
	z += vector.z;
	w += vector.w;
	return *this;
}

Vector4i& Vector4i::operator -=(const Vector4i& vector)
{
	x -= vector.x;
	y -= vector.y;
	z -= vector.z;
	w -= vector.w;
	return *this;
}

Vector4i& Vector4i::operator *=(const Vector4i& vector)
{
	x *= vector.x;
	y *= vector.y;
	z *= vector.z;
	w *= vector.w;
	return *this;
}

Vector4i& Vector4i::operator *=(float scalar)
{
	x = (int)round(x * scalar);
	y = (int)round(y * scalar);
	z = (int)round(z * scalar);
	w = (int)round(w * scalar);
	return *this;
}

Vector4i& Vector4i::operator /=(float scalar)
{
	x = (int)round(x / scalar);
	y = (int)round(y / scalar);
	z = (int)round(z / scalar);
	w = (int)round(w / scalar);
	return *this;
}

Vector4i Vector4i::operator +(const Vector4i& vector) const
{
	return Vector4i(x + vector.x, y + vector.y, z + vector.z, w + vector.w);
}

Vector4i Vector4i::operator -(const Vector4i& vector) const
{
	return Vector4i(x - vector.x, y - vector.y, z - vector.z, w - vector.w);
}

Vector4i Vector4i::operator *(const Vector4i& vector) const
{
	return Vector4i(x * vector.x, y * vector.y, z * vector.z, w * vector.w);
}

Vector4i Vector4i::operator *(float scalar) const
{
	return Vector4i(
		(int)round(x * scalar),
		(int)round(y * scalar),
		(int)round(z * scalar),
		(int)round(w * scalar));
}

Vector4i Vector4i::operator /(float scalar) const
{
	return Vector4i(
		(int)round(x / scalar),
		(int)round(y / scalar),
		(int)round(z / scalar),
		(int)round(w / scalar));
}
//}
