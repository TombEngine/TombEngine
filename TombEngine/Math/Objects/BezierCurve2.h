#pragma once

namespace TEN::Math
{
	class BezierCurve2
	{
	private:
		// Constants

		static constexpr int CONTROL_POINT_COUNT = 4;

		// Fields

		std::array<Vector2, CONTROL_POINT_COUNT> _controlPoints = {};

	public:
		// Presets

		static const BezierCurve2 Zero;
		static const BezierCurve2 Linear;
		static const BezierCurve2 EaseIn;
		static const BezierCurve2 EaseOut;
		static const BezierCurve2 EaseInOut;

		// Constructors

		BezierCurve2() = default;
		BezierCurve2(const Vector2& start, const Vector2& end, const Vector2& startHandle, const Vector2& endHandle);

		// Getters

		const Vector2& GetStart() const;
		const Vector2& GetEnd() const;
		const Vector2& GetStartHandle() const;
		const Vector2& GetEndHandle() const;

		Vector2 GetPoint(float alpha) const;
		float	GetY(float x) const;

		// Setters

		void SetStart(const Vector2& point);
		void SetEnd(const Vector2& point);
		void SetStartHandle(const Vector2& point);
		void SetEndHandle(const Vector2& point);

		// Operators

		bool operator ==(const BezierCurve2& curve) const;
		bool operator !=(const BezierCurve2& curve) const;

	private:
		// Helpers

		Vector2 GetDerivative(float alpha) const;
	};
}
