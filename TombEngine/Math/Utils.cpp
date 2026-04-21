#include "framework.h"
#include "Math/Utils.h"

#include "Math/Constants.h"

namespace TEN::Math
{
	float FloorToStep(float value, float step)
	{
		return (floor(value / step) * step);
	}

	float CeilToStep(float value, float step)
	{
		return (ceil(value / step) * step);
	}

	float RoundToStep(float value, float step)
	{
		return (round(value / step) * step);
	}

	float Remap(float value, float min0, float max0, float min1, float max1)
	{
		float alpha = (value - min0) / (max0 - min0);
		return Lerp(min1, max1, alpha);
	}

	Vector3 RoundNormal(const Vector3& normal, float epsilon)
	{
		return Vector3(
			round(normal.x / epsilon),
			round(normal.y / epsilon),
			round(normal.z / epsilon)) * epsilon;
	}

	float Lerp(float value0, float value1, float alpha)
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);
		return (((1.0f - alpha) * value0) + (alpha * value1));
	}

	float Smoothstep(float value0, float value1, float alpha)
	{
		alpha = std::clamp(alpha, value0, value1);

		// Scale, bias, and saturate alpha to [0, 1] range.
		alpha = std::clamp((alpha - value0) / (value1 - value0), 0.0f, 1.0f);

		// Evaluate polynomial.
		return (CUBE(alpha) * (alpha * ((alpha * 6) - 15.0f) + 10.0f));
	}

	float Smoothstep(float alpha)
	{
		return Smoothstep(0.0f, 1.0f, alpha);
	}

	float EaseInSine(float value0, float value1, float alpha)
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);
		return Lerp(value0, value1, 1.0f - cos((alpha * PI) / 2));
	}

	float EaseInSine(float alpha)
	{
		return EaseInSine(0.0f, 1.0f, alpha);
	}

	float EaseOutSine(float value0, float value1, float alpha)
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);
		return Lerp(value0, value1, sin((alpha * PI) / 2));
	}
	
	float EaseOutSine(float alpha)
	{
		return EaseOutSine(0.0f, 1.0f, alpha);
	}

	float EaseInOutSine(float value0, float value1, float alpha)
	{
		alpha = std::clamp(alpha, 0.0f, 1.0f);
		return Lerp(value0, value1, (1.0f - cos(alpha * PI)) / 2);
	}
	
	float EaseInOutSine(float alpha)
	{
		return EaseInOutSine(0.0f, 1.0f, alpha);
	}

	float Spline(float alpha, const float* knots, int knotCount)
	{
		if (!knots || knotCount < 4)
			return 0.0f;

		alpha = std::clamp(alpha, 0.0f, 1.0f);

		int segmentCount = knotCount - 3;

		int segmentIndex = (int)(alpha * segmentCount);
		segmentIndex = std::min(segmentIndex, segmentCount - 1);

		const float* knot = &knots[segmentIndex];
		float segmentPos = alpha * segmentCount - (float)segmentIndex;

		float cCube   = (-knot[0] + 3.0f * knot[1] - 3.0f * knot[2] + knot[3]) * 0.5f;
		float cQuad   = knot[0] - 2.5f * knot[1] + 2.0f * knot[2] - 0.5f * knot[3];
		float cLinear = (knot[2] - knot[0]) * 0.5f;
		float cConst  = knot[1];

		return segmentPos * (segmentPos * (segmentPos * cCube + cQuad) + cLinear) + cConst;
	}

	float Luma(const Vector3& color)
	{
		constexpr auto RED_COEFF   = 0.2126f;
		constexpr auto GREEN_COEFF = 0.7152f;
		constexpr auto BLUE_COEFF  = 0.0722f;

		// Use Rec.709 trichromat formula to get perceptive luma value.
		return float((color.x * RED_COEFF) + (color.y * GREEN_COEFF) + (color.z * BLUE_COEFF));
	}

	float Chroma(const Vector3& color)
	{
		float r = color.x;
		float g = color.y;
		float b = color.z;

		float maxVal = std::max({ r, g, b });
		float minVal = std::min({ r, g, b });
		float chroma = maxVal - minVal;

		float normalizedChroma = (maxVal == 0.0f) ? 0.0f : (chroma / maxVal);
		return normalizedChroma;
	}

	std::pair<std::array<int, 3>, std::array<int, 3>> GenerateColorShift(Vector3 mainColor, Vector3 additionalColor)
	{
		std::array<int, 3> colorS =
		{
			int(mainColor.x * UCHAR_MAX),
			int(mainColor.y * UCHAR_MAX),
			int(mainColor.z * UCHAR_MAX)
		};

		std::array<int, 3> colorD =
		{
			int(additionalColor.x * UCHAR_MAX),
			int(additionalColor.y * UCHAR_MAX),
			int(additionalColor.z * UCHAR_MAX)
		};

		// Determine weakest RGB component
		int lowestS = *std::min_element(colorS.begin(), colorS.end());
		int lowestD = *std::min_element(colorD.begin(), colorD.end());

		constexpr auto CHROMA_SHIFT = 32;
		constexpr auto LUMA_SHIFT = 0.5f;

		for (int i = 0; i < 3; i++)
		{
			if (colorS[i] != lowestS)
				colorS[i] += Random::GenerateInt(-CHROMA_SHIFT, CHROMA_SHIFT);

			if (colorD[i] != lowestD)
				colorD[i] += Random::GenerateInt(-CHROMA_SHIFT, CHROMA_SHIFT);

			colorS[i] = int(colorS[i] * (1.0f + Random::GenerateFloat(-LUMA_SHIFT, 0)));
			colorD[i] = int(colorD[i] * (1.0f + Random::GenerateFloat(-LUMA_SHIFT, 0)));

			colorS[i] = std::clamp(colorS[i], 0, UCHAR_MAX);
			colorD[i] = std::clamp(colorD[i], 0, UCHAR_MAX);
		}

		return { colorS, colorD };
	}

	Vector3 Screen(const Vector3& ambient, const Vector3& tint)
	{
		float luma = Luma(tint);
		auto multiplicative = ambient * tint;
		auto additive = ambient + tint;

		return Vector3(
			Lerp(multiplicative.x, additive.x, luma),
			Lerp(multiplicative.y, additive.y, luma),
			Lerp(multiplicative.z, additive.z, luma));
	}

	Vector4 Screen(const Vector4& ambient, const Vector4& tint)
	{
		auto result = Screen(Vector3(ambient), Vector3(tint));
		return Vector4(result.x, result.y, result.z, ambient.w * tint.w);
	}

	unsigned int VectorColorToRGBA(Vector4 c)
	{
		auto to8 = [](float v) -> unsigned int {
			float x = std::clamp(v, 0.0f, 1.0f) * 255.0f;
			return static_cast<unsigned int>(std::lround(x));
			};

		unsigned int R = to8(c.x);
		unsigned int G = to8(c.y);
		unsigned int B = to8(c.z);
		unsigned int A = to8(c.w);

		return (R) | (G << 8) | (B << 16) | (A << 24);
	}
}
