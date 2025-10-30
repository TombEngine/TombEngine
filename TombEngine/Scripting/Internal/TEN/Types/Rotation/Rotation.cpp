#include "framework.h"
#include "Scripting/Internal/TEN/Types/Rotation/Rotation.h"

#include "Math/Math.h"
#include "Scripting/Internal/ReservedScriptNames.h"

using namespace TEN::Math;

namespace TEN::Scripting
{
	/// Represents a 3D rotation.
	// All angle components are in degrees, automatically clamped to the range [0, 360], excluding raw access.
	// @tenprimitive Rotation
	// @pragma nostrip

	void Rotation::Register(sol::table& parent)
	{
		using ctors = sol::constructors<
			Rotation(float, float, float)>;

		// Register type.
		parent.new_usertype<Rotation>(
			ScriptReserved_Rotation,
			ctors(), sol::call_constructor, ctors(),

			// Meta functions
			sol::meta_function::to_string, &Rotation::ToString,
			sol::meta_function::equal_to, &Rotation::operator ==,
			sol::meta_function::addition, &Rotation::operator +,
			sol::meta_function::subtraction, &Rotation::operator -,

			// Utilities
			ScriptReserved_RotationLerp, &Rotation::Lerp,
			ScriptReserved_RotationDirection, &Rotation::Direction,
			ScriptReserved_RotationSigned, &Rotation::Signed,

			/// (float) Raw X angle component in degrees. Does not automaticaly clamp to [0, 360].
			// @mem x
			"x", &Rotation::x,

			/// (float) Raw Y angle component in degrees. Does not automaticaly clamp to [0, 360].
			// @mem y
			"y", &Rotation::y,

			/// (float) Raw Z angle component in degrees. Does not automaticaly clamp to [0, 360].
			// @mem z
			"z", &Rotation::z);
	}

	/// Create a Rotation object.
	// @function Rotation
	// @tparam float x X angle component in degrees.
	// @tparam float y Y angle component in degrees.
	// @tparam float z Z angle component in degrees.
	// @treturn Rotation A new Rotation object.
	Rotation::Rotation(float x, float y, float z)
	{
		this->x = WrapToUnsignedAngle(x);
		this->y = WrapToUnsignedAngle(y);
		this->z = WrapToUnsignedAngle(z);
	}

	Rotation::Rotation(const Vector3& vec)
	{
		x = WrapToUnsignedAngle(vec.x);
		y = WrapToUnsignedAngle(vec.y);
		z = WrapToUnsignedAngle(vec.z);
	}

	Rotation::Rotation(const EulerAngles& eulers)
	{
		x = WrapToUnsignedAngle(TO_DEGREES(eulers.x));
		y = WrapToUnsignedAngle(TO_DEGREES(eulers.y));
		z = WrapToUnsignedAngle(TO_DEGREES(eulers.z));
	}

	/// Get the linearly interpolated Rotation between this Rotation and the input Rotation according to the input alpha.
	// @function Lerp
	// @tparam Rotation rot Interpolation target.
	// @tparam float alpha Interpolation alpha in the range [0, 1].
	// @treturn Rotation Linearly interpolated rotation.
	Rotation Rotation::Lerp(const Rotation& rot, float alpha) const
	{
		auto orientFrom = ToEulerAngles();
		auto orientTo = rot.ToEulerAngles();
		return Rotation(EulerAngles::Lerp(orientFrom, orientTo, alpha));
	}

	/// Get the normalized direction vector of this Rotation. 
	// Can be used to get an offset from a rotation by multiplying needed distance value with this vector.
	// @function Direction
	// @treturn Vec3 Normalized direction vector.
	Vec3 Rotation::Direction() const
	{
		auto eulers = ToEulerAngles();
		return Vec3(eulers.ToDirection());
	}

	/// Get the signed version of this Rotation, clamped to [-180, 180].
	// @function Signed
	// @treturn Rotation Signed rotation.
	Rotation Rotation::Signed() const
	{
		auto signedRot = Rotation();
		signedRot.x = WrapToSignedAngle(x);
		signedRot.y = WrapToSignedAngle(y);
		signedRot.z = WrapToSignedAngle(z);
		return signedRot;
	}

	/// @function __tostring
	// @tparam Rotation rot This Rotation.
	// @treturn string A string showing the X, Y, and Z angle components of this Rotation.
	std::string Rotation::ToString() const
	{
		return ("{" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + "}");
	}

	EulerAngles Rotation::ToEulerAngles() const
	{
		return EulerAngles(ANGLE(x), ANGLE(y), ANGLE(z));
	}

	Rotation::operator Vector3() const
	{
		return Vector3(x, y, z);
	};

	bool Rotation::operator ==(const Rotation& rot) const
	{
		return (WrapToUnsignedAngle(rot.x) == WrapToUnsignedAngle(x) &&
				WrapToUnsignedAngle(rot.y) == WrapToUnsignedAngle(y) &&
				WrapToUnsignedAngle(rot.z) == WrapToUnsignedAngle(z));
	}

	Rotation Rotation::operator +(const Rotation& rot) const
	{
		return Rotation(WrapToUnsignedAngle(x + rot.x), WrapToUnsignedAngle(y + rot.y), WrapToUnsignedAngle(z + rot.z));
	}

	Rotation Rotation::operator -(const Rotation& rot) const
	{
		return Rotation(WrapToUnsignedAngle(x - rot.x), WrapToUnsignedAngle(y - rot.y), WrapToUnsignedAngle(z - rot.z));
	}

	Rotation& Rotation::operator +=(const Rotation& rot)
	{
		x = WrapToUnsignedAngle(x + rot.x);
		y = WrapToUnsignedAngle(y + rot.y);
		z = WrapToUnsignedAngle(z + rot.z);
		return *this;
	}

	Rotation& Rotation::operator -=(const Rotation& rot)
	{
		x = WrapToUnsignedAngle(x - rot.x);
		y = WrapToUnsignedAngle(y - rot.y);
		z = WrapToUnsignedAngle(z - rot.z);
		return *this;
	}

	float Rotation::WrapToUnsignedAngle(float angle) const
	{
		angle -= std::floor(angle / 360.0f) * 360.0f;
		return ((angle < 0.0f) ? (angle + 360.0f) : angle);
	}
	
	float Rotation::WrapToSignedAngle(float angle) const
	{
		// Wrap to a signed angle first to clamp extra rotation loops.
		auto result = WrapToUnsignedAngle(angle);
		return (result > 180.0f) ? result - 360.0f : result;
	}
}
