#include "framework.h"
#include "Scripting/Internal/TEN/Types/Vec3/Vec3.h"

#include "Math/Math.h"
#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"

using namespace TEN::Math;

/// Represents a float-based 3D vector.
// @tenprimitive Vec3
// @pragma nostrip

void Vec3::Register(sol::table& parent)
{
	using ctors = sol::constructors<
		Vec3(float, float, float),
		Vec3(float)>;

	// Register type.
	parent.new_usertype<Vec3>(
		ScriptReserved_Vec3,
		ctors(),
		sol::call_constructor, ctors(),

		sol::meta_function::to_string, &Vec3::ToString,
		sol::meta_function::addition, &Vec3::Add,
		sol::meta_function::subtraction, &Vec3::Subtract,
		sol::meta_function::multiplication, sol::overload(&Vec3::Multiply, &Vec3::MultiplyByScalar),
		sol::meta_function::division, &Vec3::DivideByScalar,
		sol::meta_function::unary_minus, &Vec3::UnaryMinus,
		sol::meta_function::equal_to, &Vec3::IsEqualTo,

		ScriptReserved_Vec3Normalize, &Vec3::Normalize,
		ScriptReserved_Vec3Translate, sol::overload(
			(Vec3(Vec3::*)(const Vec3&, float))(&Vec3::Translate),
			(Vec3(Vec3::*)(const Rotation&, float))(&Vec3::Translate),
			(Vec3(Vec3::*)(const Rotation&, const Vec3&))(&Vec3::Translate)),
		ScriptReserved_Vec3Rotate, &Vec3::Rotate,
		ScriptReserved_Vec3Lerp, &Vec3::Lerp,
		ScriptReserved_Vec3Cross, &Vec3::Cross,
		ScriptReserved_Vec3Dot, &Vec3::Dot,
		ScriptReserved_Vec3Distance, &Vec3::Distance,
		ScriptReserved_Vec3Length, &Vec3::Length,

		/// (float) X component.
		// @mem x
		"x", &Vec3::x,

		/// (float) Y component.
		// @mem y
		"y", &Vec3::y,

		/// (float) Z component.
		// @mem z
		"z", &Vec3::z);
}

/// Create a Vec3 object.
// @function Vec3
// @tparam float x X component.
// @tparam float y Y component.
// @tparam float z Z component.
// @treturn Vec3 A new Vec3 object.
Vec3::Vec3(float x, float y, float z)
{
	this->x = x;
	this->y = y;
	this->z = z;
}

/// Create a Vec3 object.
// @function Vec3
// @tparam float value X, Y, and Z component.
// @treturn Vec3 A new Vec3 object.
Vec3::Vec3(float value)
{
	x = value;
	y = value;
	z = value;
}

Vec3::Vec3(const Vector3& vector)
{
	x = vector.x;
	y = vector.y;
	z = vector.z;
}

Vec3::Vec3(const Vector3i& vector)
{
	x = vector.x;
	y = vector.y;
	z = vector.z;
}

/// Get a copy of this Vec3 normalized to length 1.
// @function Vec3:Normalize
// @treturn Vec3 Normalized vector.
Vec3 Vec3::Normalize() const
{
	auto vector = ToVector3();
	vector.Normalize();

	return vector;
}

/// Get a copy of this Vec3 translated in the input Vec3 direction by the input distance.
// @function Translate
// @tparam Vec3 dir Direction vector. Normalized automatically to length 1.
// @tparam float dist Distance.
// @treturn Vec3 Translated vector.
Vec3 Vec3::Translate(const Vec3& dir, float dist)
{
	return Geometry::TranslatePoint(ToVector3(), dir.ToVector3(), dist);
}

/// Get a copy of this Vec3 translated in the direction of the input Rotation object by the input distance.
// @function Translate
// @tparam Rotation rot Rotation object defining the direction.
// @tparam float dist Distance.
// @treturn Vec3 Translated vector.
Vec3 Vec3::Translate(const Rotation& rot, float dist)
{
	return Geometry::TranslatePoint(ToVector3(), rot.ToEulerAngles(), dist);
}

/// Get a copy of this Vec3 translated by an offset, where the input relative offset Vec3 is rotated according to the input Rotation object.
// @function Translate
// @tparam Rotation rot Rotation object rotating the input relative offset vector.
// @tparam Vec3 relOffset Relative offset vector before rotation.
// @treturn Vec3 Translated vector.
Vec3 Vec3::Translate(const Rotation& rot, const Vec3& relOffset)
{
	return Geometry::TranslatePoint(ToVector3(), rot.ToEulerAngles(), relOffset.ToVector3());
}

/// Get a copy of this Vec3 rotated by the input Rotation object.
// @function Vec3:Rotate
// @tparam Rotation rot Rotation object.
// @treturn Vec3 Rotated Vec3.
Vec3 Vec3::Rotate(const Rotation& rot) const
{
	auto vector = ToVector3();
	auto eulerRot = rot.ToEulerAngles();
	auto rotMatrix = eulerRot.ToRotationMatrix();

	return Vec3(Vector3::Transform(vector, rotMatrix));
}

/// Get the linearly interpolated Vec3 between this Vec3 and the input Vec3 according to the input interpolation alpha.
// @function Vec3:Lerp
// @tparam Vec3 vector Interpolation target.
// @tparam float alpha Interpolation alpha in the range [0, 1].
// @treturn Vec3 Linearly interpolated vector.
Vec3 Vec3::Lerp(const Vec3& vector, float alpha) const
{
	auto vector0 = ToVector3();
	auto vector1 = vector.ToVector3();

	return Vec3(Vector3::Lerp(vector0, vector1, alpha));
}

/// Get the cross product of this Vec3 and the input Vec3.
// @function Vec3:Cross
// @tparam Vec3 vector Input vector.
// @treturn Vec3 Cross product.
Vec3 Vec3::Cross(const Vec3& vector) const
{
	auto vector0 = ToVector3();
	auto vector1 = vector.ToVector3();

	return vector0.Cross(vector1);
}

/// Get the dot product of this Vec3 and the input Vec3.
// @function Vec3:Dot
// @tparam Vec3 vector Input vector.
// @treturn float Dot product.
float Vec3::Dot(const Vec3& vector) const
{
	auto vector0 = ToVector3();
	auto vector1 = vector.ToVector3();

	return vector0.Dot(vector1);
}

/// Get the distance between this Vec3 and the input Vec3.
// @function Vec3:Distance
// @tparam Vec3 vector Input vector.
// @treturn float Distance.
float Vec3::Distance(const Vec3& vector) const
{
	auto vector0 = ToVector3();
	auto vector1 = vector.ToVector3();

	return Vector3::Distance(vector0, vector1);
}

/// Get the length of this Vec3.
// @function Vec3:Length
// @treturn float Length.
float Vec3::Length() const
{
	return ToVector3().Length();
}

/// Metafunction. Use tostring(vector).
// @tparam Vec3 This Vec3.
// @treturn string A string showing the X, Y, and Z components of the Vec3.
// @function __tostring
std::string Vec3::ToString() const
{
	return "{" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + "}";
}

Vec3 Vec3::Add(const Vec3& vector0, const Vec3& vector1)
{
	return Vec3(vector0.x + vector1.x, vector0.y + vector1.y, vector0.z + vector1.z);
}

Vec3 Vec3::Subtract(const Vec3& vector0, const Vec3& vector1)
{
	return Vec3(vector0.x - vector1.x, vector0.y - vector1.y, vector0.z - vector1.z);
}

Vec3 Vec3::Multiply(const Vec3& vector0, const Vec3& vector1)
{
	return Vec3(vector0.x * vector1.x, vector0.y * vector1.y, vector0.z * vector1.z);
}

Vec3 Vec3::MultiplyByScalar(const Vec3& vector, float scalar)
{
	return Vec3(vector.x * scalar, vector.y * scalar, vector.z * scalar);
}

Vec3 Vec3::DivideByScalar(const Vec3& vector, float scalar)
{
	if (scalar == 0.0f)
	{
		TENLog("Vec3 attempted division by 0.", LogLevel::Warning);
		return vector;
	}

	return Vec3(vector.x / scalar, vector.y / scalar, vector.z / scalar);
}

Vec3 Vec3::UnaryMinus(const Vec3& vector)
{
	return Vec3(vector.x * -1, vector.y * -1, vector.z * -1);
}

bool Vec3::IsEqualTo(const Vec3& vector0, const Vec3& vector1)
{
	if (vector0.Distance(vector1) <= EPSILON)
		return true;

	return false;
}

Vector3 Vec3::ToVector3() const
{
	return Vector3(x, y, z);
}

Vector3i Vec3::ToVector3i() const
{
	return Vector3i(x, y, z);
}

GameVector Vec3::ToGameVector() const
{
	return GameVector(x, y, z);
}

Vec3::operator Vector3() const
{
	return ToVector3();
};
