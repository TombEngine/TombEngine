#pragma once

namespace sol { class state; }

class GameVector;
class Pose;
class Vector3i;
namespace TEN::Scripting { class Rotation; };

using namespace TEN::Scripting;

class Vec3
{
public:
	static void Register(sol::table& parent);

	// Fields

	float x = 0;
	float y = 0;
	float z = 0;

	// Constructors

	Vec3() = default;
	Vec3(float x, float y, float z);
	Vec3(float value);
	Vec3(const Vector3& vector);
	Vec3(const Vector3i& vector);

	// Utilities

	Vec3  Normalize() const;
	Vec3  Translate(const Vec3& dir, float dist);
	Vec3  Translate(const Rotation& rot, float dist);
	Vec3  Translate(const Rotation& rot, const Vec3& relOffset);
	Vec3  Rotate(const Rotation& rot) const;
	Vec3  Lerp(const Vec3& vector, float alpha) const;
	Vec3  Cross(const Vec3& vector) const;
	float Dot(const Vec3& vector) const;
	float Distance(const Vec3& vector) const;
	float Length() const;

	// Meta functions

	std::string ToString() const;
	static Vec3 Add(const Vec3& vector0, const Vec3& vector1);
	static Vec3 Subtract(const Vec3& vector0, const Vec3& vector1);
	static Vec3 Multiply(const Vec3& vector0, const Vec3& vector1);
	static Vec3 MultiplyByScalar(const Vec3& vector, float scalar);
	static Vec3 DivideByScalar(const Vec3& vector, float scalar);
	static Vec3 UnaryMinus(const Vec3& vector);
	static bool IsEqualTo(const Vec3& vector0, const Vec3& vector1);

	// Converters

	Vector3	   ToVector3() const;
	Vector3i   ToVector3i() const;
	GameVector ToGameVector() const;

	// Operators

	operator Vector3() const;
};
