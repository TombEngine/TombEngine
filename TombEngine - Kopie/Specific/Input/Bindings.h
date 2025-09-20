#pragma once

namespace TEN::Input
{
	enum class ActionID;

	using BindingProfile = std::unordered_map<ActionID, int>; // Key = action ID, value = key ID.

	extern const BindingProfile DEFAULT_KEYBOARD_MOUSE_BINDING_PROFILE;
	extern const BindingProfile DEFAULT_GAMEPAD_BINDING_PROFILE;
	extern const BindingProfile RAW_EVENT_BINDING_PROFILE;

	// TODO: The true ideal solution will be to have the following:
	//	KeyboardMouseDefault
	//	KeyboardMouseCustom
	//	GamepadDefault
	//	GamepadCustom
	//	Raw
	// And update the GUI accordingly to be capable of toggling between a keyboard/mouse bindings view and a gamepad bindings view.
	enum class BindingProfileID
	{
		Default,
		Custom,
		Raw,

		Count
	};

	// TODO: Allow different binding profiles for each device. Default, Custom1, Custom2.
	class BindingManager
	{
	private:
		// Fields

		std::unordered_map<BindingProfileID, BindingProfile> _bindings	= {}; // Key = binding profile ID, value = binding profile.
		std::unordered_map<ActionID, bool>					 _conflicts = {}; // Key = action ID, value = has conflict.

	public:
		// Constructors

		BindingManager() = default;

		// Getters

		int					  GetBoundKeyID(BindingProfileID profileID, ActionID actionID) const;
		const BindingProfile& GetBindingProfile(BindingProfileID profileID) const;

		// Setters

		void SetKeyBinding(BindingProfileID profileID, ActionID actionID, int keyID);
		void SetBindingProfile(BindingProfileID profileID, const BindingProfile& profile);
		void SetDefaultBindingProfile(BindingProfileID profileID);
		void SetConflict(ActionID actionID, bool value);

		// Inquirers

		bool TestConflict(ActionID actionID);

		// Utilities

		void Initialize();
	};

	extern BindingManager g_Bindings;
}
