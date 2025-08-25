#include "framework.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"

#include "Math/Math.h"
#include "Scripting/Internal/ReservedScriptNames.h"

using namespace TEN::Math;

/// Represents a float-based 2D vector.
// @tenprimitive Vec2
// @pragma nostrip

void Vec2::Register(sol::table& parent)
{
	using ctors = sol::constructors<
		Vec2(float, float),
		Vec2(float)>;

	// Register type.
	parent.new_usertype<Vec2>(
		ScriptReserved_Vec2,
		ctors(),
		sol::call_constructor, ctors(),

		sol::meta_function::to_string, &Vec2::ToString,
		sol::meta_function::addition, &Vec2::Add,
		sol::meta_function::subtraction, &Vec2::Subtract,
		sol::meta_function::multiplication, sol::overload(&Vec2::Multiply, &Vec2::MultiplyScalar),
		sol::meta_function::division, &Vec2::DivideByScalar,
		sol::meta_function::unary_minus, &Vec2::UnaryMinus,
		sol::meta_function::equal_to, &Vec2::IsEqualTo,

		ScriptReserved_Vec2Normalize, &Vec2::Normalize,
		ScriptReserved_Vec2Translate, sol::overload(
			(Vec2(Vec2::*)(const Vec2&, float))(&Vec2::Translate),
			(Vec2(Vec2::*)(float, float))(&Vec2::Translate),
			(Vec2(Vec2::*)(float, const Vec2&))(&Vec2::Translate)),
		ScriptReserved_Vec2Rotate, &Vec2::Rotate,
		ScriptReserved_Vec2Lerp, &Vec2::Lerp,
		ScriptReserved_Vec2Cross, &Vec2::Cross,
		ScriptReserved_Vec2Dot, &Vec2::Dot,
		ScriptReserved_Vec2Distance, &Vec2::Distance,
		ScriptReserved_Vec2Length, &Vec2::Length,

		/// (float) X component.
		// @mem x
		"x", &Vec2::x,

		/// (float) Y component.
		// @mem y
		"y", &Vec2::y);
}

/// Create a Vec2 object.
// @function Vec2
// @tparam float x X component.
// @tparam float y Y component.
// @treturn Vec2 A new Vec2 object.
Vec2::Vec2(float x, float y)
{
	this->x = x;
	this->y = y;
}

/// Create a Vec2 object.
// @function Vec
// @tparam float value X and Z component.
// @treturn Vec2 A new Vec2 object.
Vec2::Vec2(float value)
{
	x = value;
	y = value;
}

Vec2::Vec2(const Vector2& vector)
{
	x = vector.x;
	y = vector.y;
}

/*Vec2::Vec2(const Vector2i& vector)
{
	x = vector.x;
	y = vector.y;
}*/

/// Get a copy of this Vec2 normalized to length 1.
// @function Vec2:Normalize
// @treturn Vec2 Normalized vector.
Vec2 Vec2::Normalize() const
{
	auto vector = ToVector2();
	vector.Normalize();

	return vector;
}

/// Get a copy of this Vec2 translated in the input Vec2 direction by the input distance.
// @function Vec2:Translate
// @tparam Vec2 dir Direction vector. Normalized automatically to length 1.
// @tparam float dist Distance.
// @treturn Vec2 Translated vector.
Vec2 Vec2::Translate(const Vec2& dir, float dist)
{
	return Geometry::TranslatePoint(ToVector2(), dir.ToVector2(), dist);
}

/// Get a copy of this Vec2 translated in the direction of the input rotation in degrees by the input distance.
// @function Vec2:Translate
// @tparam float rot Rotation in degrees defining the direction.
// @tparam float dist Distance.
// @treturn Vec2 Translated vector.
Vec2 Vec2::Translate(float rot, float dist)
{
	return Geometry::TranslatePoint(ToVector2(), ANGLE(rot), dist);
}

/// Get a copy of this Vec2 translated by an offset, where the input relative offset Vec2 is rotated according to the input rotation in degrees.
// @function Vec2:Translate
// @tparam float rot Rotation in degrees rotating the input relative offset vector.
// @tparam Vec2 relOffset Relative offset vector before rotation.
// @treturn Vec2 Translated vector.
Vec2 Vec2::Translate(float rot, const Vec2& relOffset)
{
	return Geometry::TranslatePoint(ToVector2(), rot, relOffset.ToVector2());
}

/// Get a copy of this Vec2 rotated by the input rotation in degrees.
// @function Vec2:Rotate
// @tparam float rot Rotation in degrees.
// @treturn Vec2 Rotated Vec2.
Vec2 Vec2::Rotate(float rot) const
{
	auto vector = ToVector2();
	auto rotMatrix = Matrix::CreateRotationZ(DEG_TO_RAD(rot));

	return Vec2(Vector2::Transform(vector, rotMatrix));
}

/// Get the linearly interpolated Vec2 between this Vec2 and the input Vec2 according to the input interpolation alpha.
// @function Vec2:Lerp
// @tparam Vec2 vector Interpolation target.
// @tparam float alpha Interpolation alpha in the range [0, 1].
// @treturn Vec2 Linearly interpolated vector.
Vec2 Vec2::Lerp(const Vec2& vector, float alpha) const
{
	auto vector0 = ToVector2();
	auto vector1 = vector.ToVector2();

	return Vec2(Vector2::Lerp(vector0, vector1, alpha));
}

/// Get the cross product of this Vec2 and the input Vec2.
// @function Vec2:Cross
// @tparam Vec2 vector Input vector.
// @treturn Vec2 Cross product.
Vec2 Vec2::Cross(const Vec2& vector) const
{
	auto vector0 = ToVector2();
	auto vector1 = vector.ToVector2();

	return vector0.Cross(vector1);
}

/// Get the dot product of this Vec2 and the input Vec2.
// @function Vec2:Dot
// @tparam Vec2 vector Input vector.
// @treturn float Dot product.
float Vec2::Dot(const Vec2& vector) const
{
	auto vector0 = ToVector2();
	auto vector1 = vector.ToVector2();

	return vector0.Dot(vector1);
}

/// Get the distance between this Vec2 and the input Vec2.
// @function Vec2:Distance
// @tparam Vec2 vector Input vector.
// @treturn float Distance.
float Vec2::Distance(const Vec2& vector) const
{
	auto vector0 = ToVector2();
	auto vector1 = vector.ToVector2();

	return Vector2::Distance(vector0, vector1);
}

/// Get the length of this Vec2.
// @function Vec2:Length
// @treturn float Length.
float Vec2::Length() const
{
	return ToVector2().Length();
}

/// Metafunction. Use tostring(vector).
// @tparam Vec2 This Vec2.
// @treturn string A string showing the X and Y components of the Vec2.
// @function __tostring
std::string Vec2::ToString() const
{
	return "{" + std::to_string(x) + ", " + std::to_string(y) + "}";
}

Vec2 Vec2::Add(const Vec2& vector0, const Vec2& vector1)
{
	return Vec2(vector0.x + vector1.x, vector0.y + vector1.y);
}

Vec2 Vec2::Subtract(const Vec2& vector0, const Vec2& vector1)
{
	return Vec2(vector0.x - vector1.x, vector0.y - vector1.y);
}

Vec2 Vec2::Multiply(const Vec2& vector0, const Vec2& vector1)
{
	return Vec2(vector0.x * vector1.x, vector0.y * vector1.y);
}

Vec2 Vec2::MultiplyScalar(const Vec2& vector, float scalar)
{
	return Vec2(vector.x * scalar, vector.y * scalar);
}

Vec2 Vec2::DivideByScalar(const Vec2& vector, float scalar)
{
	if (scalar == 0.0f)
	{
		TENLog("Vec2 attempted division by 0.", LogLevel::Warning);
		return vector;
	}

	return Vec2(vector.x / scalar, vector.y / scalar);
}

Vec2 Vec2::UnaryMinus(const Vec2& vector)
{
	return Vec2(vector.x * -1, vector.y * -1);
}

bool Vec2::IsEqualTo(const Vec2& vector0, const Vec2& vector1)
{
	if (vector0.Distance(vector1) <= EPSILON)
		return true;

	return false;
}

Vector2 Vec2::ToVector2() const
{
	return Vector2(x, y);
}

/*Vector2i Vec2::ToVector2i() const
{
	return Vector2i(x, y);
}*/

Vec2::operator Vector2() const
{
	return ToVector2();
};
