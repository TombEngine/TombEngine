#pragma once

// ==========================================================================
// SimpleMath — GLM Compatibility Header
// ==========================================================================
// Drop-in replacement for DirectXTK SimpleMath types using GLM as backend.
// Memory layout is identical: SimpleMath rows = GLM columns (duality).
//
// GLM is column-major (M * v), SimpleMath is row-major (v * M).
// Matrix multiply order is inverted: SimpleMath A*B = GLM B*A.
// Translation lives at memory offsets 12-14 in both conventions.
// ==========================================================================

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>

namespace TEN::Math::Library
{
	struct Vector2;
	struct Vector3;
	struct Vector4;
	struct Matrix;
	struct Quaternion;
	struct Color;
	struct Plane;
	struct Ray;
}

// Forward declarations for collision types (used by Ray::Intersects)
namespace TEN::Math::Collision
{
	struct BoundingBox;
	struct BoundingSphere;
	struct BoundingOrientedBox;
}

namespace TEN::Math::Library
{

	// ======================================================================
	// Vector2
	// ======================================================================
	struct Vector2
	{
		float x, y;

		constexpr Vector2() : x(0), y(0) {}
		constexpr Vector2(float ix, float iy) : x(ix), y(iy) {}
		constexpr explicit Vector2(float v) : x(v), y(v) {}
		inline Vector2(const Vector3& v);
		Vector2(const glm::vec2& v) : x(v.x), y(v.y) {}

		operator glm::vec2() const { return glm::vec2(x, y); }

		bool operator==(const Vector2& v) const { return x == v.x && y == v.y; }
		bool operator!=(const Vector2& v) const { return !(*this == v); }

		Vector2 operator+(const Vector2& v) const { return Vector2(x + v.x, y + v.y); }
		Vector2 operator-(const Vector2& v) const { return Vector2(x - v.x, y - v.y); }
		Vector2 operator*(const Vector2& v) const { return Vector2(x * v.x, y * v.y); }
		Vector2 operator*(float s) const { return Vector2(x * s, y * s); }
		Vector2 operator/(float s) const { float inv = 1.0f / s; return Vector2(x * inv, y * inv); }
		Vector2 operator/(const Vector2& v) const { return Vector2(x / v.x, y / v.y); }
		Vector2 operator-() const { return Vector2(-x, -y); }

		Vector2& operator+=(const Vector2& v) { x += v.x; y += v.y; return *this; }
		Vector2& operator-=(const Vector2& v) { x -= v.x; y -= v.y; return *this; }
		Vector2& operator*=(const Vector2& v) { x *= v.x; y *= v.y; return *this; }
		Vector2& operator*=(float s) { x *= s; y *= s; return *this; }
		Vector2& operator/=(const Vector2& v) { x /= v.x; y /= v.y; return *this; }
		Vector2& operator/=(float s) { float inv = 1.0f / s; x *= inv; y *= inv; return *this; }

		float Length() const { return std::sqrt(x * x + y * y); }
		float LengthSquared() const { return x * x + y * y; }
		float Dot(const Vector2& v) const { return x * v.x + y * v.y; }
		Vector2 Cross(const Vector2& v) const { return Vector2(y * v.x - x * v.y, x * v.y - y * v.x); }
		void Normalize() { float len = Length(); if (len > 0) { float inv = 1.0f / len; x *= inv; y *= inv; } }

		static float Distance(const Vector2& a, const Vector2& b) { return (a - b).Length(); }
		static Vector2 Lerp(const Vector2& a, const Vector2& b, float t) { return Vector2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t); }
		static Vector2 Transform(const Vector2& v, const Matrix& m);

		static const Vector2 Zero;
		static const Vector2 One;
		static const Vector2 UnitX;
		static const Vector2 UnitY;
	};

	inline Vector2 operator*(float s, const Vector2& v) { return Vector2(s * v.x, s * v.y); }

	// ======================================================================
	// Vector3
	// ======================================================================
	struct Vector3
	{
		float x, y, z;

		constexpr Vector3() : x(0), y(0), z(0) {}
		constexpr Vector3(float ix, float iy, float iz) : x(ix), y(iy), z(iz) {}
		constexpr explicit Vector3(float v) : x(v), y(v), z(v) {}
		Vector3(const Vector2& v) : x(v.x), y(v.y), z(0) {}
		Vector3(const glm::vec3& v) : x(v.x), y(v.y), z(v.z) {}

		operator glm::vec3() const { return glm::vec3(x, y, z); }

		bool operator==(const Vector3& v) const { return x == v.x && y == v.y && z == v.z; }
		bool operator!=(const Vector3& v) const { return !(*this == v); }

		Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
		Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
		Vector3 operator*(const Vector3& v) const { return Vector3(x * v.x, y * v.y, z * v.z); }
		Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
		Vector3 operator/(float s) const { float inv = 1.0f / s; return Vector3(x * inv, y * inv, z * inv); }
		Vector3 operator/(const Vector3& v) const { return Vector3(x / v.x, y / v.y, z / v.z); }
		Vector3 operator-() const { return Vector3(-x, -y, -z); }

		Vector3& operator+=(const Vector3& v) { x += v.x; y += v.y; z += v.z; return *this; }
		Vector3& operator-=(const Vector3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
		Vector3& operator*=(const Vector3& v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
		Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
		Vector3& operator/=(float s) { float inv = 1.0f / s; x *= inv; y *= inv; z *= inv; return *this; }
		Vector3& operator/=(const Vector3& v) { x /= v.x; y /= v.y; z /= v.z; return *this; }

		float Length() const { return std::sqrt(x * x + y * y + z * z); }
		float LengthSquared() const { return x * x + y * y + z * z; }
		float Dot(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }
		Vector3 Cross(const Vector3& v) const { return Vector3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x); }
		void Normalize()
		{
			float len = Length();
			if (len > 0) { float inv = 1.0f / len; x *= inv; y *= inv; z *= inv; }
		}

		static float Distance(const Vector3& a, const Vector3& b) { return (a - b).Length(); }
		static float DistanceSquared(const Vector3& a, const Vector3& b) { return (a - b).LengthSquared(); }
		static Vector3 Lerp(const Vector3& a, const Vector3& b, float t) { return Vector3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }
		static Vector3 Cross(const Vector3& a, const Vector3& b) { return a.Cross(b); }
		static float Dot(const Vector3& a, const Vector3& b) { return a.Dot(b); }
		static Vector3 Normalize(const Vector3& v) { Vector3 r = v; r.Normalize(); return r; }
		static Vector3 Min(const Vector3& a, const Vector3& b) { return Vector3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)); }
		static Vector3 Max(const Vector3& a, const Vector3& b) { return Vector3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)); }

		void Clamp(const Vector3& lo, const Vector3& hi)
		{
			x = std::clamp(x, lo.x, hi.x); y = std::clamp(y, lo.y, hi.y); z = std::clamp(z, lo.z, hi.z);
		}
		static Vector3 Clamp(const Vector3& v, const Vector3& lo, const Vector3& hi)
		{
			return Vector3(std::clamp(v.x, lo.x, hi.x), std::clamp(v.y, lo.y, hi.y), std::clamp(v.z, lo.z, hi.z));
		}

		static Vector3 ClampLength(const Vector3& v, float minLength, float maxLength)
		{
			float len = v.Length();
			if (len < 1e-8f) return v;
			float clamped = std::clamp(len, minLength, maxLength);
			float scale = clamped / len;
			return Vector3(v.x * scale, v.y * scale, v.z * scale);
		}

		static Vector3 Transform(const Vector3& v, const Matrix& m);
		static Vector3 Transform(const Vector3& v, const Quaternion& q);
		static Vector3 TransformNormal(const Vector3& v, const Matrix& m);

		static const Vector3 Zero;
		static const Vector3 One;
		static const Vector3 UnitX;
		static const Vector3 UnitY;
		static const Vector3 UnitZ;
		static const Vector3 Up;
		static const Vector3 Down;
		static const Vector3 Forward;
		static const Vector3 Backward;
		static const Vector3 Right;
		static const Vector3 Left;
	};

	inline Vector3 operator*(float s, const Vector3& v) { return Vector3(s * v.x, s * v.y, s * v.z); }

	// ======================================================================
	// Vector4
	// ======================================================================
	struct Vector4
	{
		float x, y, z, w;

		constexpr Vector4() : x(0), y(0), z(0), w(0) {}
		constexpr Vector4(float ix, float iy, float iz, float iw) : x(ix), y(iy), z(iz), w(iw) {}
		constexpr explicit Vector4(float v) : x(v), y(v), z(v), w(v) {}
		constexpr Vector4(const Vector3& v, float iw) : x(v.x), y(v.y), z(v.z), w(iw) {}
		constexpr Vector4(const Vector3& v) : x(v.x), y(v.y), z(v.z), w(0.0f) {}
		Vector4(const glm::vec4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}

		operator glm::vec4() const { return glm::vec4(x, y, z, w); }
		operator Vector3() const { return Vector3(x, y, z); }

		bool operator==(const Vector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
		bool operator!=(const Vector4& v) const { return !(*this == v); }

		Vector4 operator+(const Vector4& v) const { return Vector4(x + v.x, y + v.y, z + v.z, w + v.w); }
		Vector4 operator-(const Vector4& v) const { return Vector4(x - v.x, y - v.y, z - v.z, w - v.w); }
		Vector4 operator*(const Vector4& v) const { return Vector4(x * v.x, y * v.y, z * v.z, w * v.w); }
		Vector4 operator*(float s) const { return Vector4(x * s, y * s, z * s, w * s); }
		Vector4 operator/(float s) const { float inv = 1.0f / s; return Vector4(x * inv, y * inv, z * inv, w * inv); }
		Vector4 operator/(const Vector4& v) const { return Vector4(x / v.x, y / v.y, z / v.z, w / v.w); }
		Vector4 operator-() const { return Vector4(-x, -y, -z, -w); }

		Vector4& operator+=(const Vector4& v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
		Vector4& operator-=(const Vector4& v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
		Vector4& operator*=(const Vector4& v) { x *= v.x; y *= v.y; z *= v.z; w *= v.w; return *this; }
		Vector4& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
		Vector4& operator/=(const Vector4& v) { x /= v.x; y /= v.y; z /= v.z; w /= v.w; return *this; }
		Vector4& operator/=(float s) { float inv = 1.0f / s; x *= inv; y *= inv; z *= inv; w *= inv; return *this; }

		float Length() const { return std::sqrt(x * x + y * y + z * z + w * w); }
		float LengthSquared() const { return x * x + y * y + z * z + w * w; }
		float Dot(const Vector4& v) const { return x * v.x + y * v.y + z * v.z + w * v.w; }
		void Normalize() { float len = Length(); if (len > 0) { float inv = 1.0f / len; x *= inv; y *= inv; z *= inv; w *= inv; } }

		static Vector4 Transform(const Vector4& v, const Matrix& m);
		static Vector4 Lerp(const Vector4& a, const Vector4& b, float t)
		{
			return Vector4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
		}

		static const Vector4 Zero;
		static const Vector4 One;
		static const Vector4 UnitX;
		static const Vector4 UnitY;
		static const Vector4 UnitZ;
		static const Vector4 UnitW;
	};

	inline Vector4 operator*(float s, const Vector4& v) { return Vector4(s * v.x, s * v.y, s * v.z, s * v.w); }

	// ======================================================================
	// Matrix
	// ======================================================================
	struct Matrix
	{
		union
		{
			struct
			{
				float _11, _12, _13, _14;
				float _21, _22, _23, _24;
				float _31, _32, _33, _34;
				float _41, _42, _43, _44;
			};
			float m[4][4];
			glm::mat4 _m;
		};

		Matrix() : _m(0.0f) {}

		Matrix(float m11, float m12, float m13, float m14,
			   float m21, float m22, float m23, float m24,
			   float m31, float m32, float m33, float m34,
			   float m41, float m42, float m43, float m44)
			: _11(m11), _12(m12), _13(m13), _14(m14),
			  _21(m21), _22(m22), _23(m23), _24(m24),
			  _31(m31), _32(m32), _33(m33), _34(m34),
			  _41(m41), _42(m42), _43(m43), _44(m44) {}

		Matrix(const Vector4& row0, const Vector4& row1, const Vector4& row2, const Vector4& row3)
			: _11(row0.x), _12(row0.y), _13(row0.z), _14(row0.w),
			  _21(row1.x), _22(row1.y), _23(row1.z), _24(row1.w),
			  _31(row2.x), _32(row2.y), _33(row2.z), _34(row2.w),
			  _41(row3.x), _42(row3.y), _43(row3.z), _44(row3.w) {}

		Matrix(const glm::mat4& mat) : _m(mat) {}

		operator glm::mat4() const { return _m; }

		bool operator==(const Matrix& rhs) const { return _m == rhs._m; }
		bool operator!=(const Matrix& rhs) const { return _m != rhs._m; }

		// SimpleMath A*B = GLM B*A (inverted order).
		Matrix operator*(const Matrix& rhs) const { return Matrix(rhs._m * _m); }
		Matrix& operator*=(const Matrix& rhs) { _m = rhs._m * _m; return *this; }

		Vector3 Translation() const { return Vector3(_41, _42, _43); }
		void Translation(const Vector3& v) { _41 = v.x; _42 = v.y; _43 = v.z; }

		Matrix Invert() const { return Matrix(glm::inverse(_m)); }
		Matrix Transpose() const { return Matrix(glm::transpose(_m)); }
		bool Decompose(Vector3& scale, Quaternion& rotation, Vector3& translation) const;

		static Matrix CreateTranslation(const Vector3& pos) { return Matrix(glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z))); }
		static Matrix CreateTranslation(float x, float y, float z) { return Matrix(glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z))); }

		static Matrix CreateScale(const Vector3& s) { return Matrix(glm::scale(glm::mat4(1.0f), glm::vec3(s.x, s.y, s.z))); }
		static Matrix CreateScale(float s) { return Matrix(glm::scale(glm::mat4(1.0f), glm::vec3(s))); }
		static Matrix CreateScale(float x, float y, float z) { return Matrix(glm::scale(glm::mat4(1.0f), glm::vec3(x, y, z))); }

		static Matrix CreateRotationX(float radians) { return Matrix(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(1, 0, 0))); }
		static Matrix CreateRotationY(float radians) { return Matrix(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0, 1, 0))); }
		static Matrix CreateRotationZ(float radians) { return Matrix(glm::rotate(glm::mat4(1.0f), radians, glm::vec3(0, 0, 1))); }
		static Matrix CreateFromAxisAngle(const Vector3& axis, float angle)
		{
			return Matrix(glm::rotate(glm::mat4(1.0f), angle, glm::vec3(axis.x, axis.y, axis.z)));
		}

		static Matrix CreateFromYawPitchRoll(float yaw, float pitch, float roll)
		{
			return Matrix(glm::yawPitchRoll(yaw, pitch, roll));
		}

		static Matrix CreateFromQuaternion(const Quaternion& q);

		static Matrix CreateLookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
		{
			return Matrix(glm::lookAtRH(glm::vec3(eye.x, eye.y, eye.z),
										glm::vec3(target.x, target.y, target.z),
										glm::vec3(up.x, up.y, up.z)));
		}

		static Matrix CreatePerspectiveFieldOfView(float fov, float aspect, float nearPlane, float farPlane)
		{
			return Matrix(glm::perspectiveRH_ZO(fov, aspect, nearPlane, farPlane));
		}
		static Matrix CreateOrthographic(float width, float height, float nearPlane, float farPlane)
		{
			return Matrix(glm::orthoRH_ZO(-width * 0.5f, width * 0.5f, -height * 0.5f, height * 0.5f, nearPlane, farPlane));
		}
		static Matrix CreateOrthographicOffCenter(float left, float right, float bottom, float top, float nearPlane, float farPlane)
		{
			return Matrix(glm::orthoRH_ZO(left, right, bottom, top, nearPlane, farPlane));
		}

		static Matrix CreateBillboard(const Vector3& objectPos, const Vector3& cameraPos, const Vector3& cameraUp);
		static Matrix CreateBillboard(const Vector3& objectPos, const Vector3& cameraPos, const Vector3& cameraUp, const Vector3& cameraForward);
		static Matrix CreateConstrainedBillboard(const Vector3& objectPos, const Vector3& cameraPos, const Vector3& rotateAxis, const Vector3& cameraForward, const Vector3& objectForward);
		static Matrix CreateConstrainedBillboard(const Vector3& objectPos, const Vector3& cameraPos, const Vector3& rotateAxis, const Vector3* cameraForward, const Vector3* objectForward);
		static Matrix CreateReflection(const Plane& plane);
		static Matrix Lerp(const Matrix& a, const Matrix& b, float t);

		static const Matrix Identity;
	};

	// ======================================================================
	// Quaternion
	// ======================================================================
	struct Quaternion
	{
		float x, y, z, w;

		Quaternion() : x(0), y(0), z(0), w(1) {}
		Quaternion(float ix, float iy, float iz, float iw) : x(ix), y(iy), z(iz), w(iw) {}
		Quaternion(const Vector3& axis, float angle)
		{
			glm::quat q = glm::angleAxis(angle, glm::vec3(axis.x, axis.y, axis.z));
			x = q.x; y = q.y; z = q.z; w = q.w;
		}
		Quaternion(const glm::quat& q) : x(q.x), y(q.y), z(q.z), w(q.w) {}
		Quaternion(const Vector4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}

		operator glm::quat() const { return glm::quat(w, x, y, z); }
		explicit operator Vector3() const { return Vector3(x, y, z); }
		operator Vector4() const { return Vector4(x, y, z, w); }

		bool operator==(const Quaternion& q) const { return x == q.x && y == q.y && z == q.z && w == q.w; }
		bool operator!=(const Quaternion& q) const { return !(*this == q); }

		// SimpleMath q1*q2 = GLM q2*q1 (inverted order).
		Quaternion operator*(const Quaternion& rhs) const
		{
			glm::quat result = glm::quat(rhs.w, rhs.x, rhs.y, rhs.z) * glm::quat(w, x, y, z);
			return Quaternion(result);
		}
		Quaternion& operator*=(const Quaternion& rhs)
		{
			*this = *this * rhs;
			return *this;
		}

		float Length() const { return std::sqrt(x * x + y * y + z * z + w * w); }
		float LengthSquared() const { return x * x + y * y + z * z + w * w; }
		void Normalize()
		{
			float len = Length();
			if (len > 0) { float inv = 1.0f / len; x *= inv; y *= inv; z *= inv; w *= inv; }
		}

		static Quaternion CreateFromYawPitchRoll(float yaw, float pitch, float roll)
		{
			glm::quat q(glm::yawPitchRoll(yaw, pitch, roll));
			return Quaternion(q);
		}
		static Quaternion CreateFromAxisAngle(const Vector3& axis, float angle)
		{
			glm::quat q = glm::angleAxis(angle, glm::vec3(axis.x, axis.y, axis.z));
			return Quaternion(q);
		}
		static Quaternion CreateFromRotationMatrix(const Matrix& m)
		{
			glm::quat q = glm::quat_cast(m._m);
			return Quaternion(q);
		}

		static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t)
		{
			glm::quat result = glm::slerp(glm::quat(a.w, a.x, a.y, a.z), glm::quat(b.w, b.x, b.y, b.z), t);
			return Quaternion(result);
		}
		static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t)
		{
			glm::quat result = glm::lerp(glm::quat(a.w, a.x, a.y, a.z), glm::quat(b.w, b.x, b.y, b.z), t);
			return Quaternion(result);
		}
		static Quaternion Concatenate(const Quaternion& q1, const Quaternion& q2)
		{
			glm::quat result = glm::quat(q2.w, q2.x, q2.y, q2.z) * glm::quat(q1.w, q1.x, q1.y, q1.z);
			return Quaternion(result);
		}
		static Quaternion Inverse(const Quaternion& q)
		{
			glm::quat result = glm::inverse(glm::quat(q.w, q.x, q.y, q.z));
			return Quaternion(result);
		}

		static const Quaternion Identity;
	};

	// ======================================================================
	// Color
	// ======================================================================
	struct Color
	{
		float x, y, z, w; // r, g, b, a

		constexpr Color() : x(0), y(0), z(0), w(1) {}
		explicit constexpr Color(float v) : x(v), y(v), z(v), w(1) {}
		constexpr Color(float r, float g, float b) : x(r), y(g), z(b), w(1) {}
		constexpr Color(float r, float g, float b, float a) : x(r), y(g), z(b), w(a) {}
		constexpr Color(const Vector4& v) : x(v.x), y(v.y), z(v.z), w(v.w) {}
		constexpr Color(const Vector3& v) : x(v.x), y(v.y), z(v.z), w(1) {}

		explicit Color(unsigned int argb)
			: x(((argb >> 16) & 0xFF) / 255.0f),
			  y(((argb >> 8) & 0xFF) / 255.0f),
			  z((argb & 0xFF) / 255.0f),
			  w(((argb >> 24) & 0xFF) / 255.0f) {}

		float R() const { return x; }
		float G() const { return y; }
		float B() const { return z; }
		float A() const { return w; }

		Vector3 ToVector3() const { return Vector3(x, y, z); }
		Vector4 ToVector4() const { return Vector4(x, y, z, w); }

		operator Vector3() const { return Vector3(x, y, z); }
		operator Vector4() const { return Vector4(x, y, z, w); }

		bool operator==(const Color& c) const { return x == c.x && y == c.y && z == c.z && w == c.w; }
		bool operator!=(const Color& c) const { return !(*this == c); }

		Color operator+(const Color& c) const { return Color(x + c.x, y + c.y, z + c.z, w + c.w); }
		Color operator-(const Color& c) const { return Color(x - c.x, y - c.y, z - c.z, w - c.w); }
		Color operator*(const Color& c) const { return Color(x * c.x, y * c.y, z * c.z, w * c.w); }
		Color operator/(const Color& c) const { return Color(x / c.x, y / c.y, z / c.z, w / c.w); }
		Color& operator+=(const Color& c) { x += c.x; y += c.y; z += c.z; w += c.w; return *this; }
		Color& operator-=(const Color& c) { x -= c.x; y -= c.y; z -= c.z; w -= c.w; return *this; }
		Color& operator*=(const Color& c) { x *= c.x; y *= c.y; z *= c.z; w *= c.w; return *this; }
		Color& operator/=(const Color& c) { x /= c.x; y /= c.y; z /= c.z; w /= c.w; return *this; }

		Color operator+(float s) const { return Color(x + s, y + s, z + s, w + s); }
		Color operator-(float s) const { return Color(x - s, y - s, z - s, w - s); }
		Color operator*(float s) const { return Color(x * s, y * s, z * s, w * s); }
		Color operator/(float s) const { return Color(x / s, y / s, z / s, w / s); }
		Color& operator+=(float s) { x += s; y += s; z += s; w += s; return *this; }
		Color& operator-=(float s) { x -= s; y -= s; z -= s; w -= s; return *this; }
		Color& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
		Color& operator/=(float s) { x /= s; y /= s; z /= s; w /= s; return *this; }

		Color operator-() const { return Color(-x, -y, -z, -w); }

		static Color Lerp(const Color& a, const Color& b, float t)
		{
			return Color(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
						 a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
		}
	};

	inline constexpr Color Color_White { 1.0f, 1.0f, 1.0f, 1.0f };
	inline constexpr Color Color_Black { 0.0f, 0.0f, 0.0f, 1.0f };
	inline constexpr Color Color_Red   { 1.0f, 0.0f, 0.0f, 1.0f };
	inline constexpr Color Color_Green { 0.0f, 1.0f, 0.0f, 1.0f };
	inline constexpr Color Color_Blue  { 0.0f, 0.0f, 1.0f, 1.0f };
	inline constexpr Color Color_Yellow{ 1.0f, 1.0f, 0.0f, 1.0f };

	inline Color operator+(float s, const Color& c) { return Color(s + c.x, s + c.y, s + c.z, s + c.w); }
	inline Color operator-(float s, const Color& c) { return Color(s - c.x, s - c.y, s - c.z, s - c.w); }
	inline Color operator*(float s, const Color& c) { return Color(s * c.x, s * c.y, s * c.z, s * c.w); }

	// ======================================================================
	// Plane
	// ======================================================================
	struct Plane
	{
		float x, y, z, w;

		Plane() : x(0), y(0), z(0), w(0) {}
		Plane(const Vector3& normal, float d) : x(normal.x), y(normal.y), z(normal.z), w(d) {}
		Plane(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}

		Plane(const Vector3& p0, const Vector3& p1, const Vector3& p2)
		{
			Vector3 e1 = p1 - p0;
			Vector3 e2 = p2 - p0;
			Vector3 n = e1.Cross(e2);
			n.Normalize();
			x = n.x; y = n.y; z = n.z;
			w = -(n.x * p0.x + n.y * p0.y + n.z * p0.z);
		}

		Vector3 Normal() const { return Vector3(x, y, z); }
		float D() const { return w; }

		bool operator==(const Plane& p) const { return x == p.x && y == p.y && z == p.z && w == p.w; }
		bool operator!=(const Plane& p) const { return !(*this == p); }

		float DotCoordinate(const Vector3& v) const { return x * v.x + y * v.y + z * v.z + w; }
		float DotNormal(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }
	};

	// ======================================================================
	// Viewport
	// ======================================================================
	struct Viewport
	{
		float x, y, width, height, minDepth, maxDepth;

		Viewport() : x(0), y(0), width(0), height(0), minDepth(0), maxDepth(1) {}
		Viewport(float ix, float iy, float iw, float ih, float iMinDepth, float iMaxDepth)
			: x(ix), y(iy), width(iw), height(ih), minDepth(iMinDepth), maxDepth(iMaxDepth) {}

		Vector3 Unproject(const Vector3& p, const Matrix& projection, const Matrix& view, const Matrix& world) const
		{
			float ndcX = (p.x - x) / width * 2.0f - 1.0f;
			float ndcY = -((p.y - y) / height * 2.0f - 1.0f);
			float scale = maxDepth - minDepth;
			float ndcZ = (scale > 0.0f) ? (p.z - minDepth) / scale : p.z;

			glm::mat4 wvp = projection._m * view._m * world._m;
			glm::mat4 inv = glm::inverse(wvp);

			glm::vec4 v = inv * glm::vec4(ndcX, ndcY, ndcZ, 1.0f);
			if (std::abs(v.w) > 1e-7f)
				v /= v.w;

			return Vector3(v.x, v.y, v.z);
		}
	};

	// ======================================================================
	// Ray
	// ======================================================================
	struct Ray
	{
		Vector3 position;
		Vector3 direction;

		Ray() : position(Vector3::Zero), direction(Vector3(0, 0, 1)) {}
		Ray(const Vector3& pos, const Vector3& dir) : position(pos), direction(dir) {}

		bool operator==(const Ray& r) const { return position == r.position && direction == r.direction; }
		bool operator!=(const Ray& r) const { return !(*this == r); }

		// Collision Intersects - defined out-of-line after collision types.
		bool Intersects(const TEN::Math::Collision::BoundingSphere& sphere, float& dist) const;
		bool Intersects(const TEN::Math::Collision::BoundingBox& box, float& dist) const;
		bool Intersects(const TEN::Math::Collision::BoundingOrientedBox& box, float& dist) const;

		bool Intersects(const Plane& plane, float& dist) const
		{
			float lenSq = plane.x * plane.x + plane.y * plane.y + plane.z * plane.z;
			float invLen = (lenSq > 0.0f) ? (1.0f / std::sqrt(lenSq)) : 0.0f;
			float nx = plane.x * invLen;
			float ny = plane.y * invLen;
			float nz = plane.z * invLen;
			float d  = plane.w * invLen;

			float denom = nx * direction.x + ny * direction.y + nz * direction.z;
			if (std::abs(denom) < 1e-8f) { dist = 0; return false; }
			float numer = nx * position.x + ny * position.y + nz * position.z + d;
			dist = -numer / denom;
			return dist >= 0;
		}

		bool Intersects(const Vector3& v0, const Vector3& v1, const Vector3& v2, float& dist) const
		{
			const float EPSILON = 1e-8f;
			Vector3 e1 = v1 - v0;
			Vector3 e2 = v2 - v0;
			Vector3 h = direction.Cross(e2);
			float a = e1.Dot(h);

			if (a > -EPSILON && a < EPSILON) { dist = 0; return false; }

			float f = 1.0f / a;
			Vector3 s = position - v0;
			float u = f * s.Dot(h);
			if (u < 0.0f || u > 1.0f) { dist = 0; return false; }

			Vector3 q = s.Cross(e1);
			float v = f * direction.Dot(q);
			if (v < 0.0f || u + v > 1.0f) { dist = 0; return false; }

			float t = f * e2.Dot(q);
			if (t > EPSILON)
			{
				dist = t;
				return true;
			}

			dist = 0;
			return false;
		}
	};

	// ======================================================================
	// Inline implementations (need full type definitions)
	// ======================================================================

	inline Vector2::Vector2(const Vector3& v) : x(v.x), y(v.y) {}

	inline Vector2 Vector2::Transform(const Vector2& v, const Matrix& m)
	{
		glm::vec4 result = m._m * glm::vec4(v.x, v.y, 0.0f, 1.0f);
		return Vector2(result.x, result.y);
	}

	inline Vector3 Vector3::Transform(const Vector3& v, const Matrix& m)
	{
		glm::vec4 result = m._m * glm::vec4(v.x, v.y, v.z, 1.0f);
		return Vector3(result.x, result.y, result.z);
	}

	inline Vector3 Vector3::Transform(const Vector3& v, const Quaternion& q)
	{
		glm::vec3 result = glm::rotate(glm::quat(q.w, q.x, q.y, q.z), glm::vec3(v.x, v.y, v.z));
		return Vector3(result);
	}

	inline Vector3 Vector3::TransformNormal(const Vector3& v, const Matrix& m)
	{
		glm::vec4 result = m._m * glm::vec4(v.x, v.y, v.z, 0.0f);
		return Vector3(result.x, result.y, result.z);
	}

	inline Vector4 Vector4::Transform(const Vector4& v, const Matrix& m)
	{
		glm::vec4 result = m._m * glm::vec4(v.x, v.y, v.z, v.w);
		return Vector4(result);
	}

	inline Matrix Matrix::CreateFromQuaternion(const Quaternion& q)
	{
		return Matrix(glm::mat4_cast(glm::quat(q.w, q.x, q.y, q.z)));
	}

	inline bool Matrix::Decompose(Vector3& scale, Quaternion& rotation, Vector3& translation) const
	{
		glm::vec3 s, t, skew;
		glm::vec4 perspective;
		glm::quat r;
		bool result = glm::decompose(_m, s, r, t, skew, perspective);
		scale = Vector3(s);
		rotation = Quaternion(r);
		translation = Vector3(t);
		return result;
	}

	inline Matrix Matrix::CreateBillboard(const Vector3& objectPos, const Vector3& cameraPos,
										  const Vector3& cameraUp)
	{
		return CreateBillboard(objectPos, cameraPos, cameraUp, Vector3(0, 0, -1));
	}

	inline Matrix Matrix::CreateBillboard(const Vector3& objectPos, const Vector3& cameraPos,
										  const Vector3& cameraUp, const Vector3& cameraForward)
	{
		Vector3 zAxis = objectPos - cameraPos;
		float lenSq = zAxis.LengthSquared();
		if (lenSq < 1e-6f)
			zAxis = cameraForward;
		else
			zAxis = zAxis * (1.0f / std::sqrt(lenSq));

		Vector3 xAxis = cameraUp.Cross(zAxis);
		xAxis.Normalize();
		Vector3 yAxis = zAxis.Cross(xAxis);

		return Matrix(
			Vector4(xAxis.x, xAxis.y, xAxis.z, 0.0f),
			Vector4(yAxis.x, yAxis.y, yAxis.z, 0.0f),
			Vector4(zAxis.x, zAxis.y, zAxis.z, 0.0f),
			Vector4(objectPos.x, objectPos.y, objectPos.z, 1.0f)
		);
	}

	inline Matrix Matrix::CreateConstrainedBillboard(const Vector3& objectPos, const Vector3& cameraPos,
													 const Vector3& rotateAxis, const Vector3& cameraForward,
													 const Vector3& objectForward)
	{
		Vector3 faceDir = objectPos - cameraPos;
		float lenSq = faceDir.LengthSquared();
		if (lenSq < 1e-6f)
			faceDir = cameraForward;
		else
			faceDir = faceDir * (1.0f / std::sqrt(lenSq));

		float dot = rotateAxis.Dot(faceDir);
		Vector3 yAxis = rotateAxis;
		Vector3 zAxis, xAxis;

		if (std::abs(dot) > 0.9982f)
		{
			zAxis = objectForward;
			dot = rotateAxis.Dot(zAxis);
			if (std::abs(dot) > 0.9982f)
				zAxis = (std::abs(rotateAxis.z) > 0.9982f) ? Vector3::Right : Vector3::Forward;

			xAxis = rotateAxis.Cross(zAxis);
			xAxis.Normalize();
			zAxis = xAxis.Cross(rotateAxis);
			zAxis.Normalize();
		}
		else
		{
			xAxis = rotateAxis.Cross(faceDir);
			xAxis.Normalize();
			zAxis = xAxis.Cross(rotateAxis);
			zAxis.Normalize();
		}

		return Matrix(
			Vector4(xAxis.x, xAxis.y, xAxis.z, 0.0f),
			Vector4(yAxis.x, yAxis.y, yAxis.z, 0.0f),
			Vector4(zAxis.x, zAxis.y, zAxis.z, 0.0f),
			Vector4(objectPos.x, objectPos.y, objectPos.z, 1.0f)
		);
	}

	inline Matrix Matrix::CreateConstrainedBillboard(const Vector3& objectPos, const Vector3& cameraPos,
													 const Vector3& rotateAxis, const Vector3* cameraForward,
													 const Vector3* objectForward)
	{
		static const Vector3 defaultForward(0, 0, -1);
		return CreateConstrainedBillboard(
			objectPos, cameraPos, rotateAxis,
			cameraForward ? *cameraForward : defaultForward,
			objectForward ? *objectForward : defaultForward);
	}

	inline Matrix Matrix::CreateReflection(const Plane& plane)
	{
		float len = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
		float a, b, c, d;
		if (len > 0)
		{
			float invLen = 1.0f / len;
			a = plane.x * invLen;
			b = plane.y * invLen;
			c = plane.z * invLen;
			d = plane.w * invLen;
		}
		else
		{
			a = b = c = d = 0;
		}

		float fa = -2.0f * a;
		float fb = -2.0f * b;
		float fc = -2.0f * c;

		return Matrix(
			fa * a + 1.0f, fa * b,        fa * c,        0.0f,
			fb * a,        fb * b + 1.0f,  fb * c,        0.0f,
			fc * a,        fc * b,         fc * c + 1.0f, 0.0f,
			fa * d,        fb * d,         fc * d,        1.0f
		);
	}

	inline Matrix Matrix::Lerp(const Matrix& a, const Matrix& b, float t)
	{
		const float* fa = &a._11;
		const float* fb = &b._11;
		Matrix result;
		float* fr = &result._11;
		for (int i = 0; i < 16; i++)
			fr[i] = fa[i] + (fb[i] - fa[i]) * t;
		return result;
	}

	// ======================================================================
	// Static constants
	// ======================================================================

	inline const Vector2 Vector2::Zero(0.0f, 0.0f);
	inline const Vector2 Vector2::One(1.0f, 1.0f);
	inline const Vector2 Vector2::UnitX(1.0f, 0.0f);
	inline const Vector2 Vector2::UnitY(0.0f, 1.0f);

	inline const Vector3 Vector3::Zero(0.0f, 0.0f, 0.0f);
	inline const Vector3 Vector3::One(1.0f, 1.0f, 1.0f);
	inline const Vector3 Vector3::UnitX(1.0f, 0.0f, 0.0f);
	inline const Vector3 Vector3::UnitY(0.0f, 1.0f, 0.0f);
	inline const Vector3 Vector3::UnitZ(0.0f, 0.0f, 1.0f);
	inline const Vector3 Vector3::Up(0.0f, 1.0f, 0.0f);
	inline const Vector3 Vector3::Down(0.0f, -1.0f, 0.0f);
	inline const Vector3 Vector3::Forward(0.0f, 0.0f, -1.0f);
	inline const Vector3 Vector3::Backward(0.0f, 0.0f, 1.0f);
	inline const Vector3 Vector3::Right(1.0f, 0.0f, 0.0f);
	inline const Vector3 Vector3::Left(-1.0f, 0.0f, 0.0f);

	inline const Vector4 Vector4::Zero(0.0f, 0.0f, 0.0f, 0.0f);
	inline const Vector4 Vector4::One(1.0f, 1.0f, 1.0f, 1.0f);
	inline const Vector4 Vector4::UnitX(1.0f, 0.0f, 0.0f, 0.0f);
	inline const Vector4 Vector4::UnitY(0.0f, 1.0f, 0.0f, 0.0f);
	inline const Vector4 Vector4::UnitZ(0.0f, 0.0f, 1.0f, 0.0f);
	inline const Vector4 Vector4::UnitW(0.0f, 0.0f, 0.0f, 1.0f);

	inline const Matrix Matrix::Identity = Matrix(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	inline const Quaternion Quaternion::Identity(0.0f, 0.0f, 0.0f, 1.0f);

} // namespace TEN::Math::Library

// ======================================================================
// Collision primitives
// ======================================================================

namespace TEN::Math::Collision
{
	using namespace TEN::Math::Library;

	enum ContainmentType
	{
		DISJOINT   = 0,
		INTERSECTS = 1,
		CONTAINS   = 2
	};

	struct BoundingBox;
	struct BoundingSphere;
	struct BoundingOrientedBox;

	struct BoundingBox
	{
		static constexpr size_t CORNER_COUNT = 8;

		Vector3 Center;
		Vector3 Extents;

		BoundingBox() : Center(0, 0, 0), Extents(1, 1, 1) {}
		BoundingBox(const Vector3& center, const Vector3& extents)
			: Center(center), Extents(extents) {}

		bool Intersects(const BoundingBox& other) const;
		bool Intersects(const BoundingSphere& sphere) const;
		bool Intersects(const BoundingOrientedBox& obb) const;
		bool Intersects(const Vector3& origin, const Vector3& direction, float& dist) const;

		ContainmentType Contains(const BoundingBox& other) const;
		ContainmentType Contains(const Vector3& point) const;

		void GetCorners(Vector3* corners) const;

		static void CreateFromPoints(BoundingBox& out, const Vector3& minPoint, const Vector3& maxPoint);
		static void CreateMerged(BoundingBox& out, const BoundingBox& a, const BoundingBox& b);
	};

	struct BoundingSphere
	{
		Vector3 Center;
		float Radius;

		BoundingSphere() : Center(0, 0, 0), Radius(1.0f) {}
		BoundingSphere(const Vector3& center, float radius)
			: Center(center), Radius(radius) {}

		bool Intersects(const BoundingSphere& other) const;
		bool Intersects(const BoundingBox& box) const;
		bool Intersects(const BoundingOrientedBox& obb) const;
		bool Intersects(const Vector3& origin, const Vector3& direction, float& dist) const;

		ContainmentType Contains(const Vector3& point) const;
		ContainmentType Contains(const BoundingSphere& other) const;
	};

	struct BoundingOrientedBox
	{
		static constexpr size_t CORNER_COUNT = 8;

		Vector3 Center;
		Vector3 Extents;
		Quaternion Orientation;

		BoundingOrientedBox()
			: Center(0, 0, 0), Extents(1, 1, 1), Orientation(0, 0, 0, 1) {}
		BoundingOrientedBox(const Vector3& center, const Vector3& extents, const Quaternion& orientation)
			: Center(center), Extents(extents), Orientation(orientation) {}

		bool Intersects(const BoundingOrientedBox& other) const;
		bool Intersects(const BoundingBox& aabb) const;
		bool Intersects(const BoundingSphere& sphere) const;
		bool Intersects(const Vector3& origin, const Vector3& direction, float& dist) const;

		ContainmentType Contains(const Vector3& point) const;
		ContainmentType Contains(const BoundingOrientedBox& other) const;
		ContainmentType Contains(const BoundingSphere& sphere) const;

		void Transform(BoundingOrientedBox& out, float scale, const Quaternion& rotation, const Vector3& translation) const;
		void GetCorners(Vector3* corners) const;
	};

	// --- BoundingBox implementations ---

	inline bool BoundingBox::Intersects(const BoundingBox& other) const
	{
		Vector3 d = Center - other.Center;
		Vector3 e = Extents + other.Extents;
		return (std::abs(d.x) <= e.x) && (std::abs(d.y) <= e.y) && (std::abs(d.z) <= e.z);
	}

	inline bool BoundingBox::Intersects(const BoundingSphere& sphere) const
	{
		Vector3 mn = Center - Extents;
		Vector3 mx = Center + Extents;
		Vector3 closest = Vector3::Clamp(sphere.Center, mn, mx);
		float distSq = Vector3::DistanceSquared(sphere.Center, closest);
		return distSq <= sphere.Radius * sphere.Radius;
	}

	inline bool BoundingBox::Intersects(const BoundingOrientedBox& obb) const
	{
		BoundingOrientedBox aabbAsObb(Center, Extents, Quaternion(0, 0, 0, 1));
		return obb.Intersects(aabbAsObb);
	}

	inline bool BoundingBox::Intersects(const Vector3& origin, const Vector3& direction, float& dist) const
	{
		dist = 0.0f;
		float tmin = -FLT_MAX;
		float tmax = FLT_MAX;

		Vector3 mn = Center - Extents;
		Vector3 mx = Center + Extents;

		const float* o = &origin.x;
		const float* d = &direction.x;
		const float* bmin = &mn.x;
		const float* bmax = &mx.x;

		for (int i = 0; i < 3; i++)
		{
			if (std::abs(d[i]) < 1e-8f)
			{
				if (o[i] < bmin[i] || o[i] > bmax[i])
					return false;
			}
			else
			{
				float invD = 1.0f / d[i];
				float t1 = (bmin[i] - o[i]) * invD;
				float t2 = (bmax[i] - o[i]) * invD;
				if (t1 > t2) std::swap(t1, t2);
				tmin = std::max(tmin, t1);
				tmax = std::min(tmax, t2);
				if (tmin > tmax)
					return false;
			}
		}

		if (tmax < 0.0f)
			return false;

		dist = (tmin >= 0.0f) ? tmin : tmax;
		return true;
	}

	inline ContainmentType BoundingBox::Contains(const BoundingBox& other) const
	{
		Vector3 minA = Center - Extents;
		Vector3 maxA = Center + Extents;
		Vector3 minB = other.Center - other.Extents;
		Vector3 maxB = other.Center + other.Extents;

		if (minA.x <= minB.x && maxA.x >= maxB.x &&
			minA.y <= minB.y && maxA.y >= maxB.y &&
			minA.z <= minB.z && maxA.z >= maxB.z)
			return CONTAINS;

		if (maxA.x < minB.x || minA.x > maxB.x ||
			maxA.y < minB.y || minA.y > maxB.y ||
			maxA.z < minB.z || minA.z > maxB.z)
			return DISJOINT;

		return INTERSECTS;
	}

	inline ContainmentType BoundingBox::Contains(const Vector3& point) const
	{
		Vector3 mn = Center - Extents;
		Vector3 mx = Center + Extents;
		if (point.x >= mn.x && point.x <= mx.x &&
			point.y >= mn.y && point.y <= mx.y &&
			point.z >= mn.z && point.z <= mx.z)
			return CONTAINS;
		return DISJOINT;
	}

	inline void BoundingBox::GetCorners(Vector3* corners) const
	{
		// Corner ordering matches Microsoft DirectXCollision g_BoxOffset:
		// 0-3: +Z face, 4-7: -Z face.
		Vector3 mn = Center - Extents;
		Vector3 mx = Center + Extents;
		corners[0] = Vector3(mn.x, mn.y, mx.z);
		corners[1] = Vector3(mx.x, mn.y, mx.z);
		corners[2] = Vector3(mx.x, mx.y, mx.z);
		corners[3] = Vector3(mn.x, mx.y, mx.z);
		corners[4] = Vector3(mn.x, mn.y, mn.z);
		corners[5] = Vector3(mx.x, mn.y, mn.z);
		corners[6] = Vector3(mx.x, mx.y, mn.z);
		corners[7] = Vector3(mn.x, mx.y, mn.z);
	}

	inline void BoundingBox::CreateFromPoints(BoundingBox& out, const Vector3& minPoint, const Vector3& maxPoint)
	{
		out.Center = (minPoint + maxPoint) * 0.5f;
		out.Extents = (maxPoint - minPoint) * 0.5f;
	}

	inline void BoundingBox::CreateMerged(BoundingBox& out, const BoundingBox& a, const BoundingBox& b)
	{
		Vector3 mn = Vector3::Min(a.Center - a.Extents, b.Center - b.Extents);
		Vector3 mx = Vector3::Max(a.Center + a.Extents, b.Center + b.Extents);
		out.Center = (mn + mx) * 0.5f;
		out.Extents = (mx - mn) * 0.5f;
	}

	// --- BoundingSphere implementations ---

	inline bool BoundingSphere::Intersects(const BoundingSphere& other) const
	{
		float distSq = Vector3::DistanceSquared(Center, other.Center);
		float radiusSum = Radius + other.Radius;
		return distSq <= radiusSum * radiusSum;
	}

	inline bool BoundingSphere::Intersects(const BoundingBox& box) const
	{
		return box.Intersects(*this);
	}

	inline bool BoundingSphere::Intersects(const BoundingOrientedBox& obb) const
	{
		return obb.Contains(*this) != DISJOINT;
	}

	inline bool BoundingSphere::Intersects(const Vector3& origin, const Vector3& direction, float& dist) const
	{
		Vector3 oc = origin - Center;
		float a = direction.Dot(direction);
		float b = 2.0f * oc.Dot(direction);
		float c = oc.Dot(oc) - Radius * Radius;
		float discriminant = b * b - 4.0f * a * c;

		if (discriminant < 0.0f)
		{
			dist = 0.0f;
			return false;
		}

		float sqrtD = std::sqrt(discriminant);
		float inv2a = 0.5f / a;
		float t0 = (-b - sqrtD) * inv2a;
		float t1 = (-b + sqrtD) * inv2a;

		if (t0 >= 0.0f) { dist = t0; return true; }
		if (t1 >= 0.0f) { dist = t1; return true; }

		dist = 0.0f;
		return false;
	}

	inline ContainmentType BoundingSphere::Contains(const Vector3& point) const
	{
		float distSq = Vector3::DistanceSquared(Center, point);
		return (distSq <= Radius * Radius) ? CONTAINS : DISJOINT;
	}

	inline ContainmentType BoundingSphere::Contains(const BoundingSphere& other) const
	{
		float d = Vector3::Distance(Center, other.Center);
		if (Radius >= d + other.Radius) return CONTAINS;
		if (d <= Radius + other.Radius) return INTERSECTS;
		return DISJOINT;
	}

	// --- BoundingOrientedBox helpers ---

	namespace detail
	{
		inline Vector3 RotateByQuat(const Vector3& v, const Quaternion& q)
		{
			return Vector3::Transform(v, q);
		}

		inline Vector3 RotateByQuatInverse(const Vector3& v, const Quaternion& q)
		{
			Quaternion conj(-q.x, -q.y, -q.z, q.w);
			return Vector3::Transform(v, conj);
		}

		inline void GetOBBAxes(const Quaternion& q, Vector3& axisX, Vector3& axisY, Vector3& axisZ)
		{
			axisX = RotateByQuat(Vector3::UnitX, q);
			axisY = RotateByQuat(Vector3::UnitY, q);
			axisZ = RotateByQuat(Vector3::UnitZ, q);
		}

		inline float ProjectOBB(const Vector3& axis, const Vector3& extents,
								const Vector3& axX, const Vector3& axY, const Vector3& axZ)
		{
			return std::abs(axis.Dot(axX)) * extents.x +
				   std::abs(axis.Dot(axY)) * extents.y +
				   std::abs(axis.Dot(axZ)) * extents.z;
		}

		inline bool SATTest(const Vector3& axis, const Vector3& centerDiff,
							const Vector3& extA, const Vector3 axA[3],
							const Vector3& extB, const Vector3 axB[3])
		{
			float lenSq = axis.LengthSquared();
			if (lenSq < 1e-10f) return true;

			float rA = ProjectOBB(axis, extA, axA[0], axA[1], axA[2]);
			float rB = ProjectOBB(axis, extB, axB[0], axB[1], axB[2]);
			float d = std::abs(axis.Dot(centerDiff));
			return d <= rA + rB;
		}
	}

	// --- BoundingOrientedBox implementations ---

	inline bool BoundingOrientedBox::Intersects(const BoundingOrientedBox& other) const
	{
		Vector3 axA[3], axB[3];
		detail::GetOBBAxes(Orientation, axA[0], axA[1], axA[2]);
		detail::GetOBBAxes(other.Orientation, axB[0], axB[1], axB[2]);
		Vector3 diff = other.Center - Center;

		for (int i = 0; i < 3; i++)
			if (!detail::SATTest(axA[i], diff, Extents, axA, other.Extents, axB))
				return false;
		for (int i = 0; i < 3; i++)
			if (!detail::SATTest(axB[i], diff, Extents, axA, other.Extents, axB))
				return false;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				if (!detail::SATTest(axA[i].Cross(axB[j]), diff, Extents, axA, other.Extents, axB))
					return false;

		return true;
	}

	inline bool BoundingOrientedBox::Intersects(const BoundingBox& aabb) const
	{
		BoundingOrientedBox aabbAsObb(aabb.Center, aabb.Extents, Quaternion(0, 0, 0, 1));
		return Intersects(aabbAsObb);
	}

	inline bool BoundingOrientedBox::Intersects(const BoundingSphere& sphere) const
	{
		return Contains(sphere) != DISJOINT;
	}

	inline bool BoundingOrientedBox::Intersects(const Vector3& origin, const Vector3& direction, float& dist) const
	{
		Vector3 localOrigin = detail::RotateByQuatInverse(origin - Center, Orientation);
		Vector3 localDir = detail::RotateByQuatInverse(direction, Orientation);

		float tmin = -FLT_MAX;
		float tmax = FLT_MAX;

		const float* o = &localOrigin.x;
		const float* d = &localDir.x;
		const float* e = &Extents.x;

		for (int i = 0; i < 3; i++)
		{
			if (std::abs(d[i]) < 1e-8f)
			{
				if (o[i] < -e[i] || o[i] > e[i])
					return false;
			}
			else
			{
				float invD = 1.0f / d[i];
				float t1 = (-e[i] - o[i]) * invD;
				float t2 = ( e[i] - o[i]) * invD;
				if (t1 > t2) std::swap(t1, t2);
				tmin = std::max(tmin, t1);
				tmax = std::min(tmax, t2);
				if (tmin > tmax)
					return false;
			}
		}

		if (tmax < 0.0f)
			return false;

		dist = (tmin >= 0.0f) ? tmin : tmax;
		return true;
	}

	inline ContainmentType BoundingOrientedBox::Contains(const Vector3& point) const
	{
		Vector3 local = detail::RotateByQuatInverse(point - Center, Orientation);
		if (std::abs(local.x) <= Extents.x &&
			std::abs(local.y) <= Extents.y &&
			std::abs(local.z) <= Extents.z)
			return CONTAINS;
		return DISJOINT;
	}

	inline ContainmentType BoundingOrientedBox::Contains(const BoundingOrientedBox& other) const
	{
		Vector3 corners[8];
		other.GetCorners(corners);

		bool allInside = true;
		bool anyInside = false;
		for (int i = 0; i < 8; i++)
		{
			if (Contains(corners[i]) == CONTAINS)
				anyInside = true;
			else
				allInside = false;
		}

		if (allInside) return CONTAINS;
		if (Intersects(other)) return INTERSECTS;
		return DISJOINT;
	}

	inline ContainmentType BoundingOrientedBox::Contains(const BoundingSphere& sphere) const
	{
		Vector3 local = detail::RotateByQuatInverse(sphere.Center - Center, Orientation);

		if (std::abs(local.x) + sphere.Radius <= Extents.x &&
			std::abs(local.y) + sphere.Radius <= Extents.y &&
			std::abs(local.z) + sphere.Radius <= Extents.z)
			return CONTAINS;

		Vector3 closest = Vector3::Clamp(local, -Extents, Extents);
		float distSq = Vector3::DistanceSquared(local, closest);
		if (distSq <= sphere.Radius * sphere.Radius)
			return INTERSECTS;

		return DISJOINT;
	}

	inline void BoundingOrientedBox::Transform(BoundingOrientedBox& out, float scale,
											   const Quaternion& rotation, const Vector3& translation) const
	{
		out.Center = Vector3::Transform(Center * scale, rotation) + translation;
		out.Extents = Extents * scale;
		out.Orientation = Orientation * rotation;
	}

	inline void BoundingOrientedBox::GetCorners(Vector3* corners) const
	{
		// Corner ordering matches Microsoft DirectXCollision g_BoxOffset:
		// 0-3: +Z face, 4-7: -Z face.
		static const float signs[8][3] = {
			{-1, -1, +1}, {+1, -1, +1}, {+1, +1, +1}, {-1, +1, +1},
			{-1, -1, -1}, {+1, -1, -1}, {+1, +1, -1}, {-1, +1, -1}
		};

		for (int i = 0; i < 8; i++)
		{
			Vector3 local(signs[i][0] * Extents.x, signs[i][1] * Extents.y, signs[i][2] * Extents.z);
			corners[i] = Center + detail::RotateByQuat(local, Orientation);
		}
	}

} // namespace TEN::Math::Collision

// ======================================================================
// Ray::Intersects (collision overloads, need full collision type defs)
// ======================================================================

inline bool TEN::Math::Library::Ray::Intersects(const TEN::Math::Collision::BoundingSphere& sphere, float& dist) const
{
	return sphere.Intersects(position, direction, dist);
}

inline bool TEN::Math::Library::Ray::Intersects(const TEN::Math::Collision::BoundingBox& box, float& dist) const
{
	return box.Intersects(position, direction, dist);
}

inline bool TEN::Math::Library::Ray::Intersects(const TEN::Math::Collision::BoundingOrientedBox& box, float& dist) const
{
	return box.Intersects(position, direction, dist);
}
