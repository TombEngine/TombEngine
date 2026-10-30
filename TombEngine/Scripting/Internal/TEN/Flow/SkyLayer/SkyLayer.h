#pragma once

#include "Scripting/Internal/TEN/Types/Color/Color.h"

namespace sol { class state; }

namespace TEN::Scripting::Types { class ScriptColor; }

using namespace TEN::Scripting::Types;

struct SkyLayer
{
	bool Enabled{ false };
	unsigned char R{ 0 };
	unsigned char G{ 0 };
	unsigned char B{ 0 };
	short CloudSpeed{ 0 };

	SkyLayer() = default;
	SkyLayer(ScriptColor const & col, short speed);
	void SetColor(ScriptColor const & col);
	ScriptColor GetColor() const;

	static void Register(sol::table &);
};

