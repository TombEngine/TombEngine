#pragma once

namespace TEN::Input
{
	enum class ActionId;

	using BindingProfile = std::unordered_map<ActionId, int>; // Key = action ID, value = key ID.

	extern const BindingProfile DEFAULT_USER_KEYBOARD_MOUSE_BINDING_PROFILE;
	extern const BindingProfile DEFAULT_USER_GAMEPAD_BINDING_PROFILE;
	extern const BindingProfile RAW_EVENT_BINDING_PROFILE;

	// TODO: The true ideal solution will be to have the following:
	//	KeyboardMouseDefault
	//	KeyboardMouseCustom
	//	GamepadDefault
	//	GamepadCustom
	//	Raw
	// And update the GUI accordingly to be capable of toggling between a keyboard/mouse bindings view and a gamepad bindings view.
	enum class BindingProfileId
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

		std::unordered_map<BindingProfileId, BindingProfile> _bindings	= {}; // Key = binding profile ID, value = binding profile.
		std::unordered_map<ActionId, bool>					 _conflicts = {}; // Key = action ID, value = has conflict.

	public:
		// Constructors

		BindingManager() = default;

		// Getters

		int					  GetBoundEventIds(BindingProfileId profileId, ActionId actionId) const;
		const BindingProfile& GetBindingProfile(BindingProfileId profileId) const;

		// Setters

		void SetKeyBinding(BindingProfileId profileId, ActionId actionId, int keyID);
		void SetBindingProfile(BindingProfileId profileId, const BindingProfile& profile);
		void SetDefaultBindingProfile(BindingProfileId profileId);
		void SetConflict(ActionId actionId, bool value);

		// Inquirers

		bool TestConflict(ActionId actionId);

		// Utilities

		void Initialize();
	};

	extern BindingManager g_Bindings;
}
