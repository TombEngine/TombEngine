#include "framework.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"

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
			"a", sol::property(&ScriptColor::GetA, &ScriptColor::SetA));
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
}
