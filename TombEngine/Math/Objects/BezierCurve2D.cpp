#include "framework.h"
#include "Math/Objects/BezierCurve2D.h"

namespace TEN::Math
{
	const BezierCurve2D BezierCurve2D::Zero		 = BezierCurve2D(Vector2::Zero, Vector2::Zero, Vector2::Zero, Vector2::Zero);
	const BezierCurve2D BezierCurve2D::Linear	 = BezierCurve2D(Vector2::Zero, Vector2::One, Vector2::Zero, Vector2::One);
	const BezierCurve2D BezierCurve2D::EaseIn	 = BezierCurve2D(Vector2::Zero, Vector2::One, Vector2(0.25f, 0.0f), Vector2::One);
	const BezierCurve2D BezierCurve2D::EaseOut	 = BezierCurve2D(Vector2::Zero, Vector2::One, Vector2::Zero, Vector2(0.75f, 1.0f));
	const BezierCurve2D BezierCurve2D::EaseInOut = BezierCurve2D(Vector2::Zero, Vector2::One, Vector2(0.25f, 0.0f), Vector2(0.75f, 1.0f));

	BezierCurve2D::BezierCurve2D(const Vector2& start, const Vector2& end, const Vector2& startHandle, const Vector2& endHandle)
	{
		SetStart(start);
		SetEnd(end);
		SetStartHandle(startHandle);
		SetEndHandle(endHandle);
	}

	const Vector2& BezierCurve2D::GetStart() const
	{
		return _controlPoints[0];
	}

	const Vector2& BezierCurve2D::GetEnd() const
	{
		return _controlPoints[3];
	}

	const Vector2& BezierCurve2D::GetStartHandle() const
	{
		return _controlPoints[1];
	}

	const Vector2& BezierCurve2D::GetEndHandle() const
	{
		return _controlPoints[2];
	}

	Vector2 BezierCurve2D::GetPoint(float alpha) const
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);

		// De Casteljau interpolation for point at alpha.
		auto points = _controlPoints;
		for (int i = 1; i < _controlPoints.size(); i++)
		{
			for (int j = 0; j < (_controlPoints.size() - i); j++)
				points[j] = Vector2::Lerp(points[j], points[j + 1], alpha);
		}

		return points.front();
	}

	float BezierCurve2D::GetY(float x) const
	{
		constexpr auto TOLERANCE		   = 0.001f;
		constexpr auto ITERATION_COUNT_MAX = 100;

		// Directly return Y for exact endpoint.
		if (x <= (GetStart().x + TOLERANCE))
		{
			return GetStart().y;
		}
		else if (x >= (GetEnd().x - TOLERANCE))
		{
			return GetEnd().y;
		}

		// Newton-Raphson iteration for approximate Y alpha.
		float alpha = x / GetEnd().x;
		for (int i = 0; i < ITERATION_COUNT_MAX; i++)
		{
			auto point = GetPoint(alpha);
			auto derivative = GetDerivative(alpha);

			float delta = (point.x - x) / derivative.x;
			alpha -= delta;
			if (abs(delta) <= TOLERANCE)
				break;
		}

		return GetPoint(alpha).y;
	}

	void BezierCurve2D::SetStart(const Vector2& point)
	{
		_controlPoints[0] = point;
	}

	void BezierCurve2D::SetEnd(const Vector2& point)
	{
		_controlPoints[3] = point;
	}

	void BezierCurve2D::SetStartHandle(const Vector2& point)
	{
		_controlPoints[1] = point;
	}

	void BezierCurve2D::SetEndHandle(const Vector2& point)
	{
		_controlPoints[2] = point;
	}

	Vector2 BezierCurve2D::GetDerivative(float alpha) const
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);

		auto points = _controlPoints;
		int count = (int)_controlPoints.size() - 1;

		// Calculate derivative control points.
		for (int i = 0; i < count; i++)
			points[i] = (_controlPoints[i + 1] - _controlPoints[i]) * count;

		// Reduce points using De Casteljau interpolation.
		for (int i = 1; i < count; i++)
		{
			for (int j = 0; j < (count - i); j++)
				points[j] = Vector2::Lerp(points[j], points[j + 1], alpha);
		}

		return points.front();
	}

	bool BezierCurve2D::operator ==(const BezierCurve2D& curve) const
	{
		return (GetStart() == curve.GetStart() &&
				GetEnd() == curve.GetEnd() &&
				GetStartHandle() == curve.GetStartHandle() &&
				GetEndHandle() == curve.GetEndHandle());
	}

	bool BezierCurve2D::operator !=(const BezierCurve2D& curve) const
	{
		return !(*this == curve);
	}
}
