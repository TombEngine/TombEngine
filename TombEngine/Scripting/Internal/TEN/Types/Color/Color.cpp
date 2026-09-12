#include "framework.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Math/Utils.h"

/// Represents an RGBA or RGB color.
// Components are specified as values clamped to the range [0, 255].
//
// @tenprimitive Color
// @pragma nostrip

namespace TEN::Scripting::Types
{
	void ScriptColor::Register(sol::table& parent)
	{
		using ctors = sol::constructors<ScriptColor(), 
			ScriptColor(unsigned char, unsigned char, unsigned char),
			ScriptColor(unsigned char, unsigned char, unsigned char, unsigned char)>;

		// Register type.
		parent.new_usertype<ScriptColor>(
			"Color",
			ctors(),
			sol::call_constructor, ctors(),
			sol::meta_function::to_string, &ScriptColor::ToString,
			sol::meta_function::equal_to, &ScriptColor::operator ==,

			/// (int) Red component.
			// @mem r
			"r", sol::property(&ScriptColor::GetR, &ScriptColor::SetR),

			/// (int) Green component.
			// @mem g
			"g", sol::property(&ScriptColor::GetG, &ScriptColor::SetG),

			/// (int) Blue component.
			// @mem b
			"b", sol::property(&ScriptColor::GetB, &ScriptColor::SetB),

			/// (int) Alpha component (0 = invisible, 255 = opaque).
			// @mem a
			"a", sol::property(&ScriptColor::GetA, &ScriptColor::SetA),

			// Register methods.
			"GetBrightness", &ScriptColor::GetBrightness,
			"GetSaturation", &ScriptColor::GetSaturation,
			"ToGrayscale", &ScriptColor::ToGrayscale,
			"Screen", &ScriptColor::Screen,
			"Invert", &ScriptColor::Invert,
			"GetHue", &ScriptColor::GetHue,
			"Lerp", &ScriptColor::Lerp
		);
	}

	/// Create a Color object.
	// @function Color
	// @int[opt=128] R Red component.
	// @int[opt=128] G Green component.
	// @int[opt=128] B Blue component.
	// @int[opt=255] A Alpha (transparency) component.
	// @treturn Color A new Color object.
	// @usage
	// local color1 = TEN.Color()               -- Default gray color (128, 128, 128, 255)
	// local color2 = TEN.Color(255, 0, 0)      -- Red color with full opacity (255, 0, 0, 255)
	// local color3 = TEN.Color(0, 255, 0, 128) -- Green color with 50% opacity
	ScriptColor::ScriptColor() :
		_color(128, 128, 128, 255)
	{
	}
	ScriptColor::ScriptColor(unsigned char r, unsigned char g, unsigned char b) :
		_color(r, g, b)
	{
	}

	ScriptColor::ScriptColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) :
		ScriptColor(r, g, b)
	{
		SetA(a);
	}

	ScriptColor::ScriptColor(const Vector3& color) :
		_color(color)
	{
	}

	ScriptColor::ScriptColor(const Vector4& color) :
		_color(color)
	{
	}

	ScriptColor::ScriptColor(D3DCOLOR color) :
		_color(color)
	{
	}

	unsigned char ScriptColor::GetR() const
	{
		return _color.GetR();
	}

	unsigned char ScriptColor::GetG() const
	{
		return _color.GetG();
	}

	unsigned char ScriptColor::GetB() const
	{
		return _color.GetB();
	}

	unsigned char ScriptColor::GetA() const
	{
		return _color.GetA();
	}

	void ScriptColor::SetR(unsigned char value)
	{
		_color.SetR(value);
	}

	void ScriptColor::SetG(unsigned char value)
	{
		_color.SetG(value);
	}

	void ScriptColor::SetB(unsigned char value)
	{
		_color.SetB(value);
	}

	void ScriptColor::SetA(unsigned char value)
	{
		_color.SetA(value);
	}

	/// @tparam Color color This color.
	// @treturn string A string representing R, G, B, and A values.
	// @function __tostring
	// @usage
	// local color = TEN.Color(255, 100, 50, 200)
	// print(tostring(color)) -- Output: {255, 100, 50, 200}
	std::string ScriptColor::ToString() const
	{
		return "{" + std::to_string(GetR()) + ", " + std::to_string(GetG()) + ", " + std::to_string(GetB()) + ", " + std::to_string(GetA()) + "}";
	}

	ScriptColor ScriptColor::PremultiplyAlpha()
	{
		auto alpha = _color.GetA();
		_color = Vector3(_color) * ((float)_color.GetA() / (float)UCHAR_MAX);
		_color.SetA(alpha);
		return *this;
	}

	ScriptColor::operator Color() const
	{
		return _color;
	}

	ScriptColor::operator Vector3() const
	{
		return _color;
	}

	ScriptColor::operator Vector4() const
	{
		return _color;
	}

	// NOTE: D3DCOLOR is 32 bits and is laid out as ARGB.
	ScriptColor::operator D3DCOLOR() const
	{
		return _color;
	}

	ScriptColor::operator RGBAColor8Byte() const
	{
		return _color;
	}

	bool ScriptColor::operator ==(const ScriptColor& other) const
	{
		return _color.GetR() == other.GetR() &&
			_color.GetG() == other.GetG() &&
			_color.GetB() == other.GetB() &&
			_color.GetA() == other.GetA();
	}

	/// Methods for Color type.
	// @type Color

	/// Get the perceived brightness of this Color using Rec.709 luminance formula.
	// @function Color:GetBrightness
	// @treturn float The brightness value in the range [0.0, 1.0].
	// @usage
	// local color = TEN.Color(255, 0, 0) -- Red color
	// local brightness = color:GetBrightness()
	// print(brightness) -- Output: 0.2126
	float ScriptColor::GetBrightness() const
	{
		return Math::Luma(_color);
	}

	// Method not registered due to normalization issues.

	/// Get the saturation of this Color using the HSV color model.
	// @function Color:GetSaturation
	// @treturn float The saturation value in the range [0.0, 1.0].
	// @usage
	// local color = TEN.Color(255, 0, 0) -- Red color
	// local saturation = color:GetSaturation()
	// print(saturation) -- Output: 1.0
	float ScriptColor::GetSaturation() const
	{
		return Math::Chroma(_color);
	}

	// Method not registered due to normalization issues.

	/// Convert this Color to grayscale using perceived luminance (ITU-R BT.709).
	// @function Color:ToGrayscale
	// @treturn Color A grayscale version of this Color with RGB components set to the luminance value. Alpha remains unchanged.
	// @usage
	// local color = TEN.Color(255, 0, 0) -- Red color
	// local grayscaleColor = color:ToGrayscale()
	// print(tostring(grayscaleColor)) -- Output: {54, 54, 54, 255}
	ScriptColor ScriptColor::ToGrayscale() const
	{
		const unsigned char grayscaleValue = std::clamp(GetBrightness() * 255.0f, 0.0f, 255.0f);
		return ScriptColor(grayscaleValue, grayscaleValue, grayscaleValue, GetA());
	}

	/// Blend this Color with another Color using the Screen blend mode.
	// @function Color:Screen
	// @tparam Color other The other Color to blend with.
	// @tparam[opt=true] bool keepAlpha Whether to keep the alpha component unchanged.
	// @treturn Color The resulting blended Color.
	// @usage
	// local color1 = TEN.Color(255, 0, 0) -- Red color
	// local color2 = TEN.Color(0, 0, 255) -- Blue color
	// local blendedColor = color1:Screen(color2) -- Screen blend of red and blue
	ScriptColor ScriptColor::Screen(const ScriptColor& color, TypeOrNil<bool> keepAlpha) const
	{
		bool keepAlphaValue = ValueOr<bool>(keepAlpha, true);
		ScriptColor result;
		if (keepAlphaValue)
		{
			// If keeping alpha, we can simply use the existing alpha of _color.
			Vector3 screenRGB = Math::Screen(Vector3(_color), Vector3(color));
			result = ScriptColor(screenRGB);
			result.SetA(GetA());
		}
		else
		{
			// If not keeping alpha, we need to blend the alpha values as well.
			Vector4 screenRGBA = Math::Screen(Vector4(_color), Vector4(color));
			result = ScriptColor(screenRGBA);
		}
		return ScriptColor(result);
	}

	/// Get the hue of this Color using the HSV color model.
	// @function Color:GetHue
	// @treturn float The hue value in the range [0.0, 360.0) in degrees.
	// @usage
	// local color = TEN.Color(255, 0, 0) -- Red color
	// local hue = color:GetHue()
	// print(hue) -- Output: 0.0
	float ScriptColor::GetHue() const
	{
		return Math::Hue(_color);
	}

	/// Invert the RGB components of this Color (255 - component).
	// @function Color:Invert
	// @tparam[opt=true] bool keepAlpha Whether to keep the alpha component unchanged.
	// @treturn Color An inverted version of this Color with RGB components inverted. Alpha is kept unchanged if keepAlpha is true; otherwise, it is also inverted.
	// @usage
	// -- Invert color while keeping alpha unchanged
	// local color = TEN.Color(255, 0, 0) -- Red color
	// local invertedColor = color:Invert()
	// print(tostring(invertedColor)) -- Output: {0, 255, 255, 255}
	//
	// -- Invert color including alpha
	// local colorWithAlpha = TEN.Color(255, 0, 0, 128) -- Red color with 50% opacity
	// local fullyInvertedColor = colorWithAlpha:Invert(false)
	// print(tostring(fullyInvertedColor)) -- Output: {0, 255, 255, 127}
	ScriptColor ScriptColor::Invert(TypeOrNil<bool> keepAlpha) const
	{
		bool keepAlphaValue = ValueOr<bool>(keepAlpha, true);
		return ScriptColor(
			255 - GetR(),
			255 - GetG(),
			255 - GetB(),
			keepAlphaValue ? GetA() : 255 - GetA()
		);
	}

	/// Get the linearly interpolated Color between this Color and the input Color according to the input alpha.
	// @function Color:Lerp
	// @tparam Color color The target Color.
	// @tparam float alpha The interpolation factor in the range [0.0, 1.0]. If alpha is outside this range, it will be clamped.
	// @treturn Color The resulting Color.
	// @usage
	// local color1 = TEN.Color(255, 0, 0) -- Red color
	// local color2 = TEN.Color(0, 0, 255) -- Blue color
	// local lerpedColor = color1:Lerp(color2, 0.5) -- Halfway between red and blue
	// print(tostring(lerpedColor)) -- Output: {127, 0, 127, 255}
	ScriptColor ScriptColor::Lerp(const ScriptColor& color, float alpha) const
	{
		const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
		const Color result = Color::Lerp(_color, color, clampedAlpha);
		return ScriptColor(result);
	}

	static std::tuple<ScriptColor, ScriptColor> ColorShift(const ScriptColor& color, TypeOrNil<int> chromaShift, TypeOrNil<float> hueShift)
	{
		ScriptColor colorS;
		ScriptColor colorD;
		int chromaShiftValue = ValueOr<int>(chromaShift, 32);
		float hueShiftValue = ValueOr<float>(hueShift, 0.5f);
		return std::make_tuple(colorS, colorD);
	}
}
