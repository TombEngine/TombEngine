#include "framework.h"
#include "Specific/Input/Bindings.h"

#include "Specific/Input/Action.h"
#include "Specific/Input/Event.h"
#include "Specific/trutils.h"

using namespace TEN::Utils;

namespace TEN::Input
{
	static const auto RAW_KEYBOARD_BINDING_PROFILE = BindingProfile
	{
		{ In::A,            { EventId::A } },
		{ In::B,            { EventId::B } },
		{ In::C,            { EventId::C } },
		{ In::D,            { EventId::D } },
		{ In::E,            { EventId::E } },
		{ In::F,            { EventId::F } },
		{ In::G,            { EventId::G } },
		{ In::H,            { EventId::H } },
		{ In::I,            { EventId::I } },
		{ In::J,            { EventId::J } },
		{ In::K,            { EventId::K } },
		{ In::L,            { EventId::L } },
		{ In::M,            { EventId::M } },
		{ In::N,            { EventId::N } },
		{ In::O,            { EventId::O } },
		{ In::P,            { EventId::P } },
		{ In::Q,            { EventId::Q } },
		{ In::R,            { EventId::R } },
		{ In::S,            { EventId::S } },
		{ In::T,            { EventId::T } },
		{ In::U,            { EventId::U } },
		{ In::V,            { EventId::V } },
		{ In::W,            { EventId::W } },
		{ In::X,            { EventId::X } },
		{ In::Y,            { EventId::Y } },
		{ In::Z,            { EventId::Z } },
		{ In::Num1,         { EventId::Row1, EventId::Pad1 } },
		{ In::Num2,         { EventId::Row2, EventId::Pad2 } },
		{ In::Num3,         { EventId::Row3, EventId::Pad3 } },
		{ In::Num4,         { EventId::Row4, EventId::Pad4 } },
		{ In::Num5,         { EventId::Row5, EventId::Pad5 } },
		{ In::Num6,         { EventId::Row6, EventId::Pad6 } },
		{ In::Num7,         { EventId::Row7, EventId::Pad7 } },
		{ In::Num8,         { EventId::Row8, EventId::Pad8 } },
		{ In::Num9,         { EventId::Row9, EventId::Pad9 } },
		{ In::Num0,         { EventId::Row0, EventId::Pad0 } },
		{ In::Return,       { EventId::Return, EventId::PadEnter } },
		{ In::Escape,       { EventId::Escape } },
		{ In::Backspace,    { EventId::Backspace } },
		{ In::Tab,          { EventId::Tab } },
		{ In::Space,        { EventId::Space } },
		{ In::Home,         { EventId::Home } },
		{ In::End,          { EventId::End } },
		{ In::Delete,       { EventId::Delete } },
		{ In::Minus,        { EventId::Minus, EventId::PadMinus } },
		{ In::Equals,       { EventId::Equals, EventId::PadPlus } },
		{ In::BracketLeft,  { EventId::BracketLeft } },
		{ In::BracketRight, { EventId::BracketRight } },
		{ In::Backslash,    { EventId::Backslash } },
		{ In::Semicolon,    { EventId::Semicolon } },
		{ In::Apostrophe,   { EventId::Apostrophe } },
		{ In::Comma,        { EventId::Comma } },
		{ In::Period,       { EventId::Period, EventId::PadPeriod } },
		{ In::Slash,        { EventId::Slash, EventId::PadDivide } },
		{ In::ArrowUp,      { EventId::Up } },
		{ In::ArrowDown,    { EventId::Down } },
		{ In::ArrowLeft,    { EventId::Left } },
		{ In::ArrowRight,   { EventId::Right } },
		{ In::Ctrl,         { EventId::Ctrl } },
		{ In::Shift,        { EventId::Shift } },
		{ In::Alt,          { EventId::Alt } }
	};

	static const auto RAW_MOUSE_BINDING_PROFILE = BindingProfile
	{
		{ In::MouseClickLeft,   { EventId::MouseClickLeft } },
		{ In::MouseClickMiddle, { EventId::MouseClickMiddle } },
		{ In::MouseClickRight,  { EventId::MouseClickRight } },
		{ In::MouseScrollUp,    { EventId::MouseScrollUp } },
		{ In::MouseScrollDown,  { EventId::MouseScrollDown } },
		{ In::MouseUp,          { EventId::MouseUp } },
		{ In::MouseDown,        { EventId::MouseDown } },
		{ In::MouseLeft,        { EventId::MouseLeft } },
		{ In::MouseRight,       { EventId::MouseRight } }
	};

	static const auto RAW_GAMEPAD_BINDING_PROFILE = BindingProfile
	{
		{ In::GamepadNorth,           { EventId::GamepadNorth } },
		{ In::GamepadSouth,           { EventId::GamepadSouth } },
		{ In::GamepadEast,            { EventId::GamepadEast } },
		{ In::GamepadWest,            { EventId::GamepadWest } },
		{ In::GamepadStart,           { EventId::GamepadStart } },
		{ In::GamepadSelect,          { EventId::GamepadSelect } },
		{ In::GamepadShoulderLeft,    { EventId::GamepadShoulderLeft } },
		{ In::GamepadShoulderRight,   { EventId::GamepadShoulderRight } },
		{ In::GamepadTriggerLeft,     { EventId::GamepadTriggerLeft } },
		{ In::GamepadTriggerRight,    { EventId::GamepadTriggerRight } },
		{ In::GamepadDpadUp,          { EventId::GamepadDpadUp } },
		{ In::GamepadDpadDown,        { EventId::GamepadDpadDown } },
		{ In::GamepadDpadLeft,        { EventId::GamepadDpadLeft } },
		{ In::GamepadDpadRight,       { EventId::GamepadDpadRight } },
		{ In::GamepadStickLeftIn,     { EventId::GamepadStickLeft } },
		{ In::GamepadStickLeftUp,     { EventId::GamepadStickLeftUp } },
		{ In::GamepadStickLeftDown,   { EventId::GamepadStickLeftDown } },
		{ In::GamepadStickLeftLeft,   { EventId::GamepadStickLeftLeft } },
		{ In::GamepadStickLeftRight,  { EventId::GamepadStickLeftRight } },
		{ In::GamepadStickRightIn,    { EventId::GamepadStickRight } },
		{ In::GamepadStickRightUp,    { EventId::GamepadStickRightUp } },
		{ In::GamepadStickRightDown,  { EventId::GamepadStickRightDown } },
		{ In::GamepadStickRightLeft,  { EventId::GamepadStickRightLeft } },
		{ In::GamepadStickRightRight, { EventId::GamepadStickRightRight } }
	};

	const BindingProfile DEFAULT_USER_KEYBOARD_MOUSE_BINDING_PROFILE =
	{
		{ In::Forward,        { EventId::Up } },
		{ In::Back,           { EventId::Down } },
		{ In::Left,           { EventId::Left } },
		{ In::Right,          { EventId::Right } },
		{ In::StepLeft,       { EventId::Delete } },
		{ In::StepRight,      { EventId::PageDown } },
		{ In::Action,         { EventId::Ctrl } },
		{ In::Walk,           { EventId::Shift } },
		{ In::Sprint,         { EventId::Slash } },
		{ In::Crouch,         { EventId::Period } },
		{ In::Jump,           { EventId::Alt } },
		{ In::Roll,           { EventId::End } },
		{ In::Draw,           { EventId::Space } },
		{ In::Look,           { EventId::Pad0 } },

		{ In::Accelerate,     { EventId::Ctrl } },
		{ In::Reverse,        { EventId::Down } },
		{ In::Faster,         { EventId::Slash } },
		{ In::Slower,         { EventId::Shift } },
		{ In::Brake,          { EventId::Alt } },
		{ In::Fire,           { EventId::Space } },

		{ In::Flare,          { EventId::Comma } },
		{ In::SmallMedipack,  { EventId::Minus } },
		{ In::LargeMedipack,  { EventId::Equals } },
		{ In::PreviousWeapon, { EventId::BracketLeft } },
		{ In::NextWeapon,     { EventId::BracketRight } },
		{ In::Weapon1,        { EventId::Row1 } },
		{ In::Weapon2,        { EventId::Row2 } },
		{ In::Weapon3,        { EventId::Row3 } },
		{ In::Weapon4,        { EventId::Row4 } },
		{ In::Weapon5,        { EventId::Row5 } },
		{ In::Weapon6,        { EventId::Row6 } },
		{ In::Weapon7,        { EventId::Row7 } },
		{ In::Weapon8,        { EventId::Row8 } },
		{ In::Weapon9,        { EventId::Row9 } },
		{ In::Weapon10,       { EventId::Row0 } },

		{ In::Select,         { EventId::Return } },
		{ In::Deselect,       { EventId::Escape } },
		{ In::Pause,          { EventId::P } },
		{ In::Inventory,      { EventId::Escape } },
		{ In::Save,           { EventId::F5 } },
		{ In::Load,           { EventId::F6 } }
	};

	const BindingProfile DEFAULT_USER_GAMEPAD_BINDING_PROFILE =
	{
		{ In::Forward,        { EventId::GamepadStickLeftUp } },
		{ In::Back,           { EventId::GamepadStickLeftDown } },
		{ In::Left,           { EventId::GamepadStickLeftLeft } },
		{ In::Right,          { EventId::GamepadStickLeftRight } },
		{ In::StepLeft,       { EventId::GamepadStickLeft } },
		{ In::StepRight,      { EventId::GamepadStickRight } },
		{ In::Action,         { EventId::GamepadSouth } },
		{ In::Walk,           { EventId::GamepadShoulderRight } },
		{ In::Sprint,         { EventId::GamepadTriggerRight } },
		{ In::Crouch,         { EventId::GamepadTriggerLeft } },
		{ In::Jump,           { EventId::GamepadWest } },
		{ In::Roll,           { EventId::GamepadEast } },
		{ In::Draw,           { EventId::GamepadNorth } },
		{ In::Look,           { EventId::GamepadShoulderLeft } },

		{ In::Accelerate,     { EventId::GamepadSouth } },
		{ In::Reverse,        { EventId::GamepadStickLeftDown } },
		{ In::Faster,         { EventId::GamepadTriggerRight } },
		{ In::Slower,         { EventId::GamepadShoulderRight } },
		{ In::Brake,          { EventId::GamepadWest } },
		{ In::Fire,           { EventId::GamepadTriggerLeft } },

		{ In::Flare,          { EventId::GamepadDpadDown } },
		{ In::SmallMedipack,  { EventId::Minus } },
		{ In::LargeMedipack,  { EventId::Equals } },
		{ In::PreviousWeapon, { EventId::BracketLeft } },
		{ In::NextWeapon,     { EventId::BracketRight } },
		{ In::Weapon1,        { EventId::Row1 } },
		{ In::Weapon2,        { EventId::Row2 } },
		{ In::Weapon3,        { EventId::Row3 } },
		{ In::Weapon4,        { EventId::Row4 } },
		{ In::Weapon5,        { EventId::Row5 } },
		{ In::Weapon6,        { EventId::Row6 } },
		{ In::Weapon7,        { EventId::Row7 } },
		{ In::Weapon8,        { EventId::Row8 } },
		{ In::Weapon9,        { EventId::Row9 } },
		{ In::Weapon10,       { EventId::Row0 } },

		{ In::Select,         { EventId::Return } },
		{ In::Deselect,       { EventId::GamepadSelect } },
		{ In::Pause,          { EventId::GamepadStart } },
		{ In::Inventory,      { EventId::GamepadSelect } },
		{ In::Save,           { EventId::F5 } },
		{ In::Load,           { EventId::F6 } }
	};

	const std::vector<BindingProfileId> CUSTOM_BINDING_PROFILE_IDS
	{
		BindingProfileId::CustomKeyboardMouse,
		BindingProfileId::CustomGamepad
	};

	const std::vector<BindingProfileId> RAW_BINDING_PROFILE_IDS
	{
		BindingProfileId::RawKeyboard,
		BindingProfileId::RawMouse,
		BindingProfileId::RawGamepad
	};

	const std::string& BindingManager::GetBoundKeyName(ActionId actionId)
	{
		const auto& profile = GetProfile(BindingProfileId::CustomKeyboardMouse);

		auto it = profile.find(actionId);
		if (it != profile.end() && !it->second.empty())
			return GetEventName(it->second.front());

		static const std::string empty;
		return empty;
	}

	const BindingProfile& BindingManager::GetProfile(BindingProfileId profileId) const
	{
		return _bindings[(int)profileId];
	}

	void BindingManager::SetProfile(BindingProfileId profileId, const BindingProfile& newProfile)
	{
		// Check if profile is customizable.
		if (!Contains(CUSTOM_BINDING_PROFILE_IDS, profileId))
		{
			TENLog(fmt::format("Attempted to set all bindings for non-customizable binding profile {}.", (int)profileId), Debug::LogLevel::Warning);
			return;
		}

		// Set new bindings for profile.
		_bindings[(int)profileId] = newProfile;
	}

	void BindingManager::SetBinding(BindingProfileId profileId, ActionId actionId, EventId eventId)
	{
		// Check if profile is customizable.
		if (!Contains(CUSTOM_BINDING_PROFILE_IDS, profileId))
		{
			TENLog(fmt::format("Attempted to bind event {} to action {} in non-customizable binding profile {}.", (int)eventId, (int)actionId, (int)profileId),
				LogLevel::Warning);
			return;
		}

		// Get profile.
		auto& profile = _bindings[(int)profileId];

		// Swap binding if event is already bound to another action in same group.
		bool hasSwap = false;
		for (auto actionGroupId : USER_ACTION_GROUP_IDS)
		{
			// Check if action ID exists in current group.
			const auto& actionIds = ACTION_ID_GROUPS[(int)actionGroupId];
			if (!Contains(actionIds, actionId))
				continue;

			// Run through action IDs in group.
			for (auto otherActionId : actionIds)
			{
				// Skip action to bind.
				if (actionId == otherActionId)
					continue;

				// Swap if event is already bound.
				auto& eventIds = profile.at(otherActionId);
				if (Contains(eventIds, eventId))
				{
					std::swap(eventIds, profile[actionId]);
					hasSwap = true;
					break;
				}
			}

			break;
		}

		// Add binding.
		if (!hasSwap)
			profile[actionId] = { eventId };
	}

	void BindingManager::Initialize(const BindingProfile& customKeyboardMouseProfile, const BindingProfile& customGamepadProfile)
	{
		_bindings =
		{
			customKeyboardMouseProfile,   // BindingProfileId::CustomKeyboardMouse
			customGamepadProfile,         // BindingProfileId::CustomGamepad
			RAW_KEYBOARD_BINDING_PROFILE, // BindingProfileId::RawKeyboard
			RAW_KEYBOARD_BINDING_PROFILE, // BindingProfileId::RawMouse
			RAW_KEYBOARD_BINDING_PROFILE  // BindingProfileId::RawGamepad
		};
	}
}
