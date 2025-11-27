#pragma once

namespace TEN::Math
{
	class BezierCurve2D
	{
	private:
		// Constants

		static constexpr auto CONTROL_POINT_COUNT = 4;

		// Fields

		std::array<Vector2, CONTROL_POINT_COUNT> _controlPoints = {};

	public:
		// Presets

		static const BezierCurve2D Zero;
		static const BezierCurve2D Linear;
		static const BezierCurve2D EaseIn;
		static const BezierCurve2D EaseOut;
		static const BezierCurve2D EaseInOut;

		// Constructors

		BezierCurve2D() = default;
		BezierCurve2D(const Vector2& start, const Vector2& end, const Vector2& startHandle, const Vector2& endHandle);

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

		bool operator ==(const BezierCurve2D& curve) const;
		bool operator !=(const BezierCurve2D& curve) const;

	private:
		// Helpers

		Vector2 GetDerivative(float alpha) const;
	};
}
