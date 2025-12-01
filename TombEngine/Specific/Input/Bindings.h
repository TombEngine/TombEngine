#pragma once

namespace TEN::Input
{
	enum class ActionId;
	enum class EventId;

	using BindingProfile = std::unordered_map<ActionId, std::vector<EventId>>; // Key = action ID, value = event IDs.

	extern const BindingProfile DEFAULT_USER_KEYBOARD_MOUSE_BINDING_PROFILE;
	extern const BindingProfile DEFAULT_USER_GAMEPAD_BINDING_PROFILE;
	extern const BindingProfile RAW_KEYBOARD_BINDING_PROFILE;
	extern const BindingProfile RAW_MOUSE_BINDING_PROFILE;
	extern const BindingProfile RAW_GAMEPAD_BINDING_PROFILE;

	enum class BindingProfileId
	{
		// Custom

		CustomKeyboardMouse,
		CustomGamepad,

		// Default

		DefaultKeyboardMouse,
		DefaultGamepad,

		// Raw

		RawKeyboard,
		RawMouse,
		RawGamepad,

		Count
	};

	extern const std::vector<BindingProfileId> CUSTOM_USER_KEYBOARD_MOUSE_BINDING_PROFILE_IDS;
	extern const std::vector<BindingProfileId> CUSTOM_USER_GAMEPAD_BINDING_PROFILE_IDS;
	extern const std::vector<BindingProfileId> RAW_EVENT_BINDING_PROFILE_IDS;

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

		const std::vector<EventId>& GetBoundEventIds(BindingProfileId profileId, ActionId actionId) const;
		const BindingProfile&       GetBindingProfile(BindingProfileId profileId) const;

		// Setters

		void SetEventBinding(BindingProfileId profileId, ActionId actionId, EventId eventId);
		void SetBindingProfile(BindingProfileId profileId, const BindingProfile& profile);
		void SetDefaultBindingProfile(BindingProfileId profileId);
		void SetConflict(ActionId actionId, bool state);

		// Inquirers

		bool TestConflict(ActionId actionId);

		// Utilities

		void Initialize(const BindingProfile& customKeyboardMouseBinds, const BindingProfile& customGamepadBinds);
	};

	extern BindingManager g_Bindings;
}
