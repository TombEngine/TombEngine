#pragma once

namespace sol { class state; }
namespace TEN::Math { class Vector2i; };

// TODO: Vector2i methods have compiler errors.

class Vec2
{
public:
	static void Register(sol::table& parent);

	// Fields

	float x = 0;
	float y = 0;

	// Constructors

	Vec2() = default;
	Vec2(float x, float y);
	Vec2(float value);
	Vec2(const Vector2& vector);
	//Vec2(const Vector2i& vector);

	// Utilities

	Vec2  Normalize() const;
	Vec2  Translate(const Vec2& dir, float dist);
	Vec2  Translate(float rot, float dist);
	Vec2  Translate(float rot, const Vec2& relOffset);
	Vec2  Rotate(float rot) const;
	Vec2  Lerp(const Vec2& vector, float alpha) const;
	Vec2  Cross(const Vec2& vector) const;
	float Dot(const Vec2& vector) const;
	float Distance(const Vec2& vector) const;
	float Length() const;

	// Meta functions

	std::string ToString() const;
	static Vec2 Add(const Vec2& vector0, const Vec2& vector1);
	static Vec2 Subtract(const Vec2& vector0, const Vec2& vector1);
	static Vec2 Multiply(const Vec2& vector0, const Vec2& vector1);
	static Vec2 MultiplyScalar(const Vec2& vector, float scalar);
	static Vec2 DivideByScalar(const Vec2& vector, float scalar);
	static Vec2 UnaryMinus(const Vec2& vector);
	static bool IsEqualTo(const Vec2& vector0, const Vec2& vector1);

	// Converters

	Vector2	 ToVector2() const;
	//Vector2i ToVector2i() const;

	// Operators

	operator Vector2() const;
};
