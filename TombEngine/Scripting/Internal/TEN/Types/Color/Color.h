#pragma once

#include "Specific/RGBAColor8Byte.h"

typedef unsigned int D3DCOLOR;

namespace sol { class state; }
namespace sol { template <typename T> struct as_table_t; }

namespace TEN::Scripting::Types
{
	class ScriptColor
	{
	private:
		// Fields

		RGBAColor8Byte _color;

	public:
		static void Register(sol::table& parent);

		// Constructors
		ScriptColor();
		ScriptColor(unsigned char r, unsigned char g, unsigned char b);
		ScriptColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
		ScriptColor(const Vector3& color);
		ScriptColor(const Vector4& color);
		ScriptColor(D3DCOLOR);

		// Getters

		unsigned char GetR() const;
		unsigned char GetG() const;
		unsigned char GetB() const;
		unsigned char GetA() const;

		// Setters

		void SetR(unsigned char value);
		void SetG(unsigned char value);
		void SetB(unsigned char value);
		void SetA(unsigned char value);

		// Methods

		ScriptColor PremultiplyAlpha();

		// Converters

		std::string ToString() const;

		// Operators

		operator Color() const;
		operator Vector3() const;
		operator Vector4() const;
		operator D3DCOLOR() const;
		operator RGBAColor8Byte() const;
	};
}
