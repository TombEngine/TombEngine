#pragma once

#include "Game/control/control.h"
#include "Scripting/Internal/TEN/Types/Color/Color.h"
#include "Scripting/Internal/TEN/Types/Vec2/Vec2.h"

namespace TEN::Scripting::Types { class ScriptColor; }

using namespace TEN::Scripting::Types;

/***
Constants for Display String Options.
@enum Strings.DisplayStringOption
@pragma nostrip
*/

/*** Strings.DisplayStringOption constants. To be used with @{Strings.DisplayString} class.
@table Strings.DisplayStringOption

 - `CENTER` - Set the horizontal origin point to the center of the string.
 - `RIGHT` - Set the horizontal origin point to right of the string.
 - `SHADOW` - Gives the string a small drop shadow.
 - `BLINK` - Blinks the string.
*/

enum class DisplayStringOptions
{
	Center,
	Outline,
	Right,
	Blink,

	Count
};

// NOTE: Used to store data used to render the string. Separate from DisplayString because lifetimes of classes differ slightly.
using FlagArray = std::array<bool, (int)DisplayStringOptions::Count>;

static const std::unordered_map<std::string, DisplayStringOptions> DISPLAY_STRING_OPTION_NAMES
{
	{ "CENTER", DisplayStringOptions::Center },
	{ "SHADOW", DisplayStringOptions::Outline },
	{ "RIGHT", DisplayStringOptions::Right },
	{ "BLINK", DisplayStringOptions::Blink }
};

class UserDisplayString
{
private:
	friend class StringsHandler;
	friend class DisplayString;

	// Members
	std::string _key	  = {};
	Vec2		_position = Vec2(0, 0);
	float		_scale	  = 1.0f;
	D3DCOLOR	_color	  = 0xFFFFFFFF;
	FlagArray	_flags	  = {};

	float _timeRemaining = 0.0f; // NOTE: Seconds.

	bool _isInfinite	 = false;
	bool _isTranslated	 = false;
	bool _deleteWhenZero = false;

	FreezeMode _owner = FreezeMode::None;

	// Constructors
	UserDisplayString() = default;

public:
	UserDisplayString(const std::string& key, const Vec2& pos, float scale, D3DCOLOR color, const FlagArray& flags, bool isTranslated, FreezeMode owner);
};

using DisplayStringID	 = uintptr_t;
using SetItemCallback	 = std::function<bool(DisplayStringID, const UserDisplayString&)>;
using RemoveItemCallback = std::function<bool(DisplayStringID)>;
using GetItemCallback	 = std::function<std::optional<std::reference_wrapper<UserDisplayString>>(DisplayStringID)>;

class DisplayString
{
private:
	// Members
	DisplayStringID _id = 0;

public:
	static void Register(sol::table& parent);

	// Constructors
	DisplayString();

	// Destructors
	~DisplayString();

	// Getters
	DisplayStringID GetID() const;
	std::string		GetKey() const;
	Vec2			GetPosition() const;
	float			GetScale() const;
	ScriptColor		GetColor() const;

	// Setters
	void SetKey(const std::string& key);
	void SetPosition(const sol::variadic_args& args);
	void SetScale(float scale);
	void SetColor(const ScriptColor&);
	void SetTranslated(bool isTranslated);
	void SetFlags(const sol::table& flags);

	// Routines
	static SetItemCallback	  SetItemCallbackRoutine;
	static RemoveItemCallback RemoveItemCallbackRoutine;
	static GetItemCallback	  GetItemCallbackRoutine;

	// Creating a DisplayString requires adding an identifier to a data structure.
	// Callbacks are used so that this class doesn't have to know about said data structure.
	static void SetCallbacks(SetItemCallback cba, RemoveItemCallback cbr, GetItemCallback cbg)
	{
		SetItemCallbackRoutine = cba;
		RemoveItemCallbackRoutine = cbr;
		GetItemCallbackRoutine = cbg;
	}
};
