#pragma once

#include "Specific/Input/InputAction.h"

using namespace TEN::Input;

namespace TEN::Scripting::Input
{
	/// Constants for action key IDs.
	// Contains mapped action IDs, raw keyboard input IDs, raw mouse input IDs, and Gamepad input IDs.
	// To be used with @{Input.IsKeyHit}, @{Input.IsKeyHeld}, and other similar functions.
	// @enum Input.ActionID
	// @pragma nostrip

	static const auto ACTION_IDS = std::unordered_map<std::string, ActionID>
	{
		/// Forward action.
		// @mem FORWARD
		{ "FORWARD", In::Forward },

		/// Back action.
		// @mem BACK
		{ "BACK", In::Back },

		/// Left action.
		// @mem LEFT
		{ "LEFT", In::Left },

		/// Right action.
		// @mem RIGHT
		{ "RIGHT", In::Right },

		/// Step left action.
		// @mem STEP_LEFT
		{ "STEP_LEFT", In::StepLeft },

		/// Step right action.
		// @mem STEP_RIGHT
		{ "STEP_RIGHT", In::StepRight },

		/// Walk action.
		// @mem WALK
		{ "WALK", In::Walk },

		/// Sprint action.
		// @mem SPRINT
		{ "SPRINT", In::Sprint },

		/// Crouch action.
		// @mem CROUCH
		{ "CROUCH", In::Crouch },

		/// Jump action.
		// @mem JUMP
		{ "JUMP", In::Jump },

		/// Roll action.
		// @mem ROLL
		{ "ROLL", In::Roll },

		/// Action key.
		// @mem ACTION
		{ "ACTION", In::Action },

		/// Draw weapon action.
		// @mem DRAW
		{ "DRAW", In::Draw },

		/// Look action.
		// @mem LOOK
		{ "LOOK", In::Look },

		/// Accelerate action.
		// @mem ACCELERATE
		{ "ACCELERATE", In::Accelerate },

		/// Reverse action.
		// @mem REVERSE
		{ "REVERSE", In::Reverse },

		/// Faster action.
		// @mem FASTER
		{ "FASTER", In::Faster },

		/// Slower action.
		// @mem SLOWER
		{ "SLOWER", In::Slower },

		/// Brake action.
		// @mem BRAKE
		{ "BRAKE", In::Brake },

		/// Fire action.
		// @mem FIRE
		{ "FIRE", In::Fire },

		/// Flare action.
		// @mem FLARE
		{ "FLARE", In::Flare },

		/// Small medipack action.
		// @mem SMALL_MEDIPACK
		{ "SMALL_MEDIPACK", In::SmallMedipack },

		/// Large medipack action.
		// @mem LARGE_MEDIPACK
		{ "LARGE_MEDIPACK", In::LargeMedipack },

		/// Previous weapon action.
		// @mem PREVIOUS_WEAPON
		{ "PREVIOUS_WEAPON", In::PreviousWeapon },

		/// Next weapon action.
		// @mem NEXT_WEAPON
		{ "NEXT_WEAPON", In::NextWeapon },

		/// Weapon 1 action.
		// @mem WEAPON_1
		{ "WEAPON_1", In::Weapon1 },

		/// Weapon 2 action.
		// @mem WEAPON_2
		{ "WEAPON_2", In::Weapon2 },

		/// Weapon 3 action.
		// @mem WEAPON_3
		{ "WEAPON_3", In::Weapon3 },

		/// Weapon 4 action.
		// @mem WEAPON_4
		{ "WEAPON_4", In::Weapon4 },

		/// Weapon 5 action.
		// @mem WEAPON_5
		{ "WEAPON_5", In::Weapon5 },

		/// Weapon 6 action.
		// @mem WEAPON_6
		{ "WEAPON_6", In::Weapon6 },

		/// Weapon 7 action.
		// @mem WEAPON_7
		{ "WEAPON_7", In::Weapon7 },

		/// Weapon 8 action.
		// @mem WEAPON_8
		{ "WEAPON_8", In::Weapon8 },

		/// Weapon 9 action.
		// @mem WEAPON_9
		{ "WEAPON_9", In::Weapon9 },

		/// Weapon 10 action.
		// @mem WEAPON_10
		{ "WEAPON_10", In::Weapon10 },

		/// Menu up action.
		// @mem MENU_UP
		{ "MENU_UP", In::MenuUp },

		/// Menu down action.
		// @mem MENU_DOWN
		{ "MENU_DOWN", In::MenuDown },

		/// Menu left action.
		// @mem MENU_LEFT
		{ "MENU_LEFT", In::MenuLeft },

		/// Menu right action.
		// @mem MENU_RIGHT
		{ "MENU_RIGHT", In::MenuRight },

		/// Select action.
		// @mem SELECT
		{ "SELECT", In::Select },

		/// Deselect action.
		// @mem DESELECT
		{ "DESELECT", In::Deselect },

		/// Pause action.
		// @mem PAUSE
		{ "PAUSE", In::Pause },

		/// Inventory action.
		// @mem INVENTORY
		{ "INVENTORY", In::Inventory },

		/// Save action.
		// @mem SAVE
		{ "SAVE", In::Save },

		/// Load action.
		// @mem LOAD
		{ "LOAD", In::Load },

		/// Keyboard A key.
		// @mem A
		{ "A", In::A },

		/// Keyboard B key.
		// @mem B
		{ "B", In::B },

		/// Keyboard C key.
		// @mem C
		{ "C", In::C },

		/// Keyboard D key.
		// @mem D
		{ "D", In::D },

		/// Keyboard E key.
		// @mem E
		{ "E", In::E },

		/// Keyboard F key.
		// @mem F
		{ "F", In::F },

		/// Keyboard G key.
		// @mem G
		{ "G", In::G },

		/// Keyboard H key.
		// @mem H
		{ "H", In::H },

		/// Keyboard I key.
		// @mem I
		{ "I", In::I },

		/// Keyboard J key.
		// @mem J
		{ "J", In::J },

		/// Keyboard K key.
		// @mem K
		{ "K", In::K },

		/// Keyboard L key.
		// @mem L
		{ "L", In::L },

		/// Keyboard M key.
		// @mem M
		{ "M", In::M },

		/// Keyboard N key.
		// @mem N
		{ "N", In::N },

		/// Keyboard O key.
		// @mem O
		{ "O", In::O },

		/// Keyboard P key.
		// @mem P
		{ "P", In::P },

		/// Keyboard Q key.
		// @mem Q
		{ "Q", In::Q },

		/// Keyboard R key.
		// @mem R
		{ "R", In::R },

		/// Keyboard S key.
		// @mem S
		{ "S", In::S },

		/// Keyboard T key.
		// @mem T
		{ "T", In::T },

		/// Keyboard U key.
		// @mem U
		{ "U", In::U },

		/// Keyboard V key.
		// @mem V
		{ "V", In::V },

		/// Keyboard W key.
		// @mem W
		{ "W", In::W },

		/// Keyboard X key.
		// @mem X
		{ "X", In::X },

		/// Keyboard Y key.
		// @mem Y
		{ "Y", In::Y },

		/// Keyboard Z key.
		// @mem Z
		{ "Z", In::Z },

		/// Keyboard 1 key.
		// @mem NUM1
		{ "NUM1", In::Num1 },

		/// Keyboard 2 key.
		// @mem NUM2
		{ "NUM2", In::Num2 },

		/// Keyboard 3 key.
		// @mem NUM3
		{ "NUM3", In::Num3 },

		/// Keyboard 4 key.
		// @mem NUM4
		{ "NUM4", In::Num4 },

		/// Keyboard 5 key.
		// @mem NUM5
		{ "NUM5", In::Num5 },

		/// Keyboard 6 key.
		// @mem NUM6
		{ "NUM6", In::Num6 },

		/// Keyboard 7 key.
		// @mem NUM7
		{ "NUM7", In::Num7 },

		/// Keyboard 8 key.
		// @mem NUM8
		{ "NUM8", In::Num8 },

		/// Keyboard 9 key.
		// @mem NUM9
		{ "NUM9", In::Num9 },

		/// Keyboard 0 key.
		// @mem NUM0
		{ "NUM0", In::Num0 },

		/// Return/Enter key.
		// @mem RETURN
		{ "RETURN", In::Return },

		/// Escape key.
		// @mem ESCAPE
		{ "ESCAPE", In::Escape },

		/// Backspace key.
		// @mem BACKSPACE
		{ "BACKSPACE", In::Backspace },

		/// Tab key.
		// @mem TAB
		{ "TAB", In::Tab },

		/// Space key.
		// @mem SPACE
		{ "SPACE", In::Space },

		/// Home key.
		// @mem HOME
		{ "HOME", In::Home },

		/// End key.
		// @mem END
		{ "END", In::End },

		/// Delete key.
		// @mem DELETE
		{ "DELETE", In::Delete },

		/// Minus key.
		// @mem MINUS
		{ "MINUS", In::Minus },

		/// Equals key.
		// @mem EQUALS
		{ "EQUALS", In::Equals },

		/// Left bracket key.
		// @mem BRACKET_LEFT
		{ "BRACKET_LEFT", In::BracketLeft },

		/// Right bracket key.
		// @mem BRACKET_RIGHT
		{ "BRACKET_RIGHT", In::BracketRight },

		/// Backslash key.
		// @mem BACKSLASH
		{ "BACKSLASH", In::Backslash },

		/// Semicolon key.
		// @mem SEMICOLON
		{ "SEMICOLON", In::Semicolon },

		/// Apostrophe key.
		// @mem APOSTROPHE
		{ "APOSTROPHE", In::Apostrophe },

		/// Comma key.
		// @mem COMMA
		{ "COMMA", In::Comma },

		/// Period key.
		// @mem PERIOD
		{ "PERIOD", In::Period },

		/// Slash key.
		// @mem SLASH
		{ "SLASH", In::Slash },

		/// Arrow up key.
		// @mem ARROW_UP
		{ "ARROW_UP", In::ArrowUp },

		/// Arrow down key.
		// @mem ARROW_DOWN
		{ "ARROW_DOWN", In::ArrowDown },

		/// Arrow left key.
		// @mem ARROW_LEFT
		{ "ARROW_LEFT", In::ArrowLeft },

		/// Arrow right key.
		// @mem ARROW_RIGHT
		{ "ARROW_RIGHT", In::ArrowRight },

		/// Control key.
		// @mem CTRL
		{ "CTRL", In::Ctrl },

		/// Shift key.
		// @mem SHIFT
		{ "SHIFT", In::Shift },

		/// Alt key.
		// @mem ALT
		{ "ALT", In::Alt },

		/// Mouse left click.
		// @mem MOUSE_CLICK_LEFT
		{ "MOUSE_CLICK_LEFT", In::MouseClickLeft },

		/// Mouse middle click.
		// @mem MOUSE_CLICK_MIDDLE
		{ "MOUSE_CLICK_MIDDLE", In::MouseClickMiddle },

		/// Mouse right click.
		// @mem MOUSE_CLICK_RIGHT
		{ "MOUSE_CLICK_RIGHT", In::MouseClickRight },

		/// Mouse scroll up.
		// @mem MOUSE_SCROLL_UP
		{ "MOUSE_SCROLL_UP", In::MouseScrollUp },

		/// Mouse scroll down.
		// @mem MOUSE_SCROLL_DOWN
		{ "MOUSE_SCROLL_DOWN", In::MouseScrollDown },

		/// Gamepad south button. Corresponds to the "A" button on Xbox controllers and "Cross" on PlayStation controllers.
		// @mem GAMEPAD_SOUTH
		{ "GAMEPAD_SOUTH", In::GamepadSouth },

		/// Gamepad east button. Corresponds to the "B" button on Xbox controllers and "Circle" on PlayStation controllers.
		// @mem GAMEPAD_EAST
		{ "GAMEPAD_EAST", In::GamepadEast },

		/// Gamepad west button. Corresponds to the "X" button on Xbox controllers and "Square" on PlayStation controllers.
		// @mem GAMEPAD_WEST
		{ "GAMEPAD_WEST", In::GamepadWest },

		/// Gamepad north button. Corresponds to the "Y" button on Xbox controllers and "Triangle" on PlayStation controllers.
		// @mem GAMEPAD_NORTH
		{ "GAMEPAD_NORTH", In::GamepadNorth },

		/// Gamepad back button.
		// @mem GAMEPAD_BACK
		{ "GAMEPAD_BACK", In::GamepadBack },

		/// Gamepad guide button.
		// @mem GAMEPAD_GUIDE
		{ "GAMEPAD_GUIDE", In::GamepadGuide },

		/// Gamepad start button.
		// @mem GAMEPAD_START
		{ "GAMEPAD_START", In::GamepadStart },

		/// Gamepad left stick click.
		// @mem GAMEPAD_LEFT_STICK
		{ "GAMEPAD_LEFT_STICK", In::GamepadLeftStick },

		/// Gamepad right stick click.
		// @mem GAMEPAD_RIGHT_STICK
		{ "GAMEPAD_RIGHT_STICK", In::GamepadRightStick },

		/// Gamepad left shoulder button.
		// @mem GAMEPAD_LEFT_SHOULDER
		{ "GAMEPAD_LEFT_SHOULDER", In::GamepadLeftShoulder },

		/// Gamepad right shoulder button.
		// @mem GAMEPAD_RIGHT_SHOULDER
		{ "GAMEPAD_RIGHT_SHOULDER", In::GamepadRightShoulder },

		/// Gamepad D-Pad up button.
		// @mem GAMEPAD_DPAD_UP
		{ "GAMEPAD_DPAD_UP", In::GamepadDPadUp },

		/// Gamepad D-Pad down button.
		// @mem GAMEPAD_DPAD_DOWN
		{ "GAMEPAD_DPAD_DOWN", In::GamepadDPadDown },

		/// Gamepad D-Pad left button.
		// @mem GAMEPAD_DPAD_LEFT
		{ "GAMEPAD_DPAD_LEFT", In::GamepadDPadLeft },

		/// Gamepad D-Pad right button.
		// @mem GAMEPAD_DPAD_RIGHT
		{ "GAMEPAD_DPAD_RIGHT", In::GamepadDPadRight },

		/// Gamepad right paddle 1 button.
		// @mem GAMEPAD_RIGHT_PADDLE1
		{ "GAMEPAD_RIGHT_PADDLE1", In::GamepadRightPaddle1 },

		/// Gamepad left paddle 1 button.
		// @mem GAMEPAD_LEFT_PADDLE1
		{ "GAMEPAD_LEFT_PADDLE1", In::GamepadLeftPaddle1 },

		/// Gamepad right paddle 2 button.
		// @mem GAMEPAD_RIGHT_PADDLE2
		{ "GAMEPAD_RIGHT_PADDLE2", In::GamepadRightPaddle2 },

		/// Gamepad left paddle 2 button.
		// @mem GAMEPAD_LEFT_PADDLE2
		{ "GAMEPAD_LEFT_PADDLE2", In::GamepadLeftPaddle2 },

		/// Gamepad touchpad button.
		// @mem GAMEPAD_TOUCHPAD
		{ "GAMEPAD_TOUCHPAD", In::GamepadTouchpad },

		/// Gamepad misc 1 button.
		// @mem GAMEPAD_MISC1
		{ "GAMEPAD_MISC1", In::GamepadMisc1 },

		/// Gamepad misc 2 button.
		// @mem GAMEPAD_MISC2
		{ "GAMEPAD_MISC2", In::GamepadMisc2 },

		/// Gamepad misc 3 button.
		// @mem GAMEPAD_MISC3
		{ "GAMEPAD_MISC3", In::GamepadMisc3 },

		/// Gamepad misc 4 button.
		// @mem GAMEPAD_MISC4
		{ "GAMEPAD_MISC4", In::GamepadMisc4 },

		/// Gamepad misc 5 button.
		// @mem GAMEPAD_MISC5
		{ "GAMEPAD_MISC5", In::GamepadMisc5 },

		/// Gamepad misc 6 button.
		// @mem GAMEPAD_MISC6
		{ "GAMEPAD_MISC6", In::GamepadMisc6 },

		/// Gamepad left trigger.
		// @mem GAMEPAD_LEFT_TRIGGER
		{ "GAMEPAD_LEFT_TRIGGER", In::GamepadLeftTrigger },

		/// Gamepad right trigger.
		// @mem GAMEPAD_RIGHT_TRIGGER
		{ "GAMEPAD_RIGHT_TRIGGER", In::GamepadRightTrigger }
	};
}
