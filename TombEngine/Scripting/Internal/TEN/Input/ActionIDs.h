#pragma once

#include "Specific/Input/InputAction.h"

using namespace TEN::Input;

namespace TEN::Scripting::Input
{
	/// Constants for action key IDs.
	// @enum Input.ActionID
	// @pragma nostrip

	/// Table of Input.ActionID constants.
	// To be used with @{Input.IsKeyHit}, @{Input.IsKeyHeld}, and other similar functions.
	// 
	// 	FORWARD
	// 	BACK
	// 	LEFT
	// 	RIGHT
	// 	STEP_LEFT
	// 	STEP_RIGHT
	// 	WALK
	// 	SPRINT
	// 	CROUCH
	// 	JUMP
	// 	ROLL
	// 	ACTION
	// 	DRAW
	// 	LOOK
	// 
	// 	ACCELERATE
	// 	REVERSE
	// 	FASTER
	// 	SLOWER
	// 	BRAKE
	// 	FIRE
	// 
	// 	FLARE
	// 	SMALL_MEDIPACK
	// 	LARGE_MEDIPACK
	// 	PREVIOUS_WEAPON
	// 	NEXT_WEAPON
	// 	WEAPON_1
	// 	WEAPON_2
	// 	WEAPON_3
	// 	WEAPON_4
	// 	WEAPON_5
	// 	WEAPON_6
	// 	WEAPON_7
	// 	WEAPON_8
	// 	WEAPON_9
	// 	WEAPON_10
	// 
	// 	SELECT
	// 	DESELECT
	// 	PAUSE
	// 	INVENTORY
	// 	SAVE
	// 	LOAD
	// 
	//	A
	//	B
	//	C
	//	D
	//	E
	//	F
	//	G
	//	H
	//	I
	//	J
	//	K
	//	L
	//	M
	//	N
	//	O
	//	P
	//	Q
	//	R
	//	S
	//	T
	//	U
	//	V
	//	W
	//	X
	//	Y
	//	Z
	//	1
	//	2
	//	3
	//	4
	//	5
	//	6
	//	7
	//	8
	//	9
	//	0
	//	RETURN
	//	ESCAPE
	//	BACKSPACE
	//	TAB
	//	SPACE
	//	HOME
	//	END
	//	DELETE
	//	MINUS
	//	EQUALS
	//	BRACKET_LEFT
	//	BRACKET_RIGHT
	//	BACKSLASH
	//	SEMICOLON
	//	APOSTROPHE
	//	COMMA
	//	PERIOD
	//	SPASH
	//	ARROW_UP
	//	ARROW_DOWN
	//	ARROW_LEFT
	//	ARROW_RIGHT
	//	CTRL
	//	SHIFT
	//	ALT
	// 
	//	MOUSE_CLICK_LEFT
	//	MOUSE_CLICK_MIDDLE
	//	MOUSE_CLICK_RIGHT
	//	MOUSE_SCROLL_UP
	//	MOUSE_SCROLL_DOWN
	// 
	// @table Input.ActionID

	static const auto ACTION_IDS = std::unordered_map<std::string, ActionID>
	{
		{ "FORWARD", In::Forward },
		{ "BACK", In::Back },
		{ "LEFT", In::Left },
		{ "RIGHT", In::Right },
		{ "STEP_LEFT", In::StepLeft },
		{ "STEP_RIGHT", In::StepRight },
		{ "WALK", In::Walk },
		{ "SPRINT", In::Sprint },
		{ "CROUCH", In::Crouch },
		{ "JUMP", In::Jump },
		{ "ROLL", In::Roll },
		{ "ACTION", In::Action },
		{ "DRAW", In::Draw },
		{ "LOOK", In::Look },

		{ "ACCELERATE", In::Accelerate },
		{ "REVERSE", In::Reverse },
		{ "FASTER", In::Faster },
		{ "SLOWER", In::Slower },
		{ "BRAKE", In::Brake },
		{ "FIRE", In::Fire },

		{ "FLARE", In::Flare },
		{ "SMALL_MEDIPACK", In::SmallMedipack },
		{ "LARGE_MEDIPACK", In::LargeMedipack },
		{ "PREVIOUS_WEAPON", In::PreviousWeapon },
		{ "NEXT_WEAPON", In::NextWeapon },
		{ "WEAPON_1", In::Weapon1 },
		{ "WEAPON_2", In::Weapon2 },
		{ "WEAPON_3", In::Weapon3 },
		{ "WEAPON_4", In::Weapon4 },
		{ "WEAPON_5", In::Weapon5 },
		{ "WEAPON_6", In::Weapon6 },
		{ "WEAPON_7", In::Weapon7 },
		{ "WEAPON_8", In::Weapon8 },
		{ "WEAPON_9", In::Weapon9 },
		{ "WEAPON_10", In::Weapon10 },

		{ "SELECT", In::Select },
		{ "DESELECT", In::Deselect },
		{ "PAUSE", In::Pause },
		{ "INVENTORY", In::Inventory },
		{ "SAVE", In::Save },
		{ "LOAD", In::Load },

		{ "A", In::A },
		{ "B", In::B },
		{ "C", In::C },
		{ "D", In::D },
		{ "E", In::E },
		{ "F", In::F },
		{ "G", In::G },
		{ "H", In::H },
		{ "I", In::I },
		{ "J", In::J },
		{ "K", In::K },
		{ "L", In::L },
		{ "M", In::M },
		{ "N", In::N },
		{ "O", In::O },
		{ "P", In::P },
		{ "Q", In::Q },
		{ "R", In::R },
		{ "S", In::S },
		{ "T", In::T },
		{ "U", In::U },
		{ "V", In::V },
		{ "W", In::W },
		{ "X", In::X },
		{ "Y", In::Y },
		{ "Z", In::Z },
		{ "1", In::Num1 },
		{ "2", In::Num2 },
		{ "3", In::Num3 },
		{ "4", In::Num4 },
		{ "5", In::Num5 },
		{ "6", In::Num6 },
		{ "7", In::Num7 },
		{ "8", In::Num8 },
		{ "9", In::Num9 },
		{ "0", In::Num0 },
		{ "RETURN", In::Return },
		{ "ESCAPE", In::Escape },
		{ "BACKSPACE", In::Backspace },
		{ "TAB", In::Tab },
		{ "SPACE", In::Space },
		{ "HOME", In::Home },
		{ "END", In::End },
		{ "DELETE", In::Delete },
		{ "MINUS", In::Minus },
		{ "EQUALS", In::Equals },
		{ "BRACKET_LEFT", In::BracketLeft },
		{ "BRACKET_RIGHT", In::BracketRight },
		{ "BACKSLASH", In::Backslash },
		{ "SEMICOLON", In::Semicolon },
		{ "APOSTROPHE", In::Apostrophe },
		{ "COMMA", In::Comma },
		{ "PERIOD", In::Period },
		{ "SLASH", In::Slash },
		{ "ARROW_UP", In::ArrowUp },
		{ "ARROW_DOWN", In::ArrowDown },
		{ "ARROW_LEFT", In::ArrowLeft },
		{ "ARROW_RIGHT", In::ArrowRight },
		{ "CTRL", In::Ctrl },
		{ "SHIFT", In::Shift },
		{ "ALT", In::Alt },

		{ "MOUSE_CLICK_LEFT", In::MouseClickLeft },
		{ "MOUSE_CLICK_MIDDLE", In::MouseClickMiddle },
		{ "MOUSE_CLICK_RIGHT", In::MouseClickRight },
		{ "MOUSE_SCROLL_UP", In::MouseScrollUp },
		{ "MOUSE_SCROLL_DOWN", In::MouseScrollDown }
	};
}
