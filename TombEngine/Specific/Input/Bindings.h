#pragma once

namespace TEN::Input
{
	enum class ActionId;
	enum class EventId;

	using BindingProfile = std::unordered_map<ActionId, std::vector<EventId>>; // Key = action ID, value = event IDs.

	enum class BindingProfileId
	{
		// Custom

		CustomKeyboardMouse,
		CustomGamepad,

		// Raw

		RawKeyboard,
		RawMouse,
		RawGamepad,

		Count
	};

	extern const BindingProfile DEFAULT_USER_KEYBOARD_MOUSE_BINDING_PROFILE;
	extern const BindingProfile DEFAULT_USER_GAMEPAD_BINDING_PROFILE;
	extern const BindingProfile RAW_KEYBOARD_BINDING_PROFILE;
	extern const BindingProfile RAW_MOUSE_BINDING_PROFILE;
	extern const BindingProfile RAW_GAMEPAD_BINDING_PROFILE;

	extern const std::vector<BindingProfileId> CUSTOM_BINDING_PROFILE_IDS;
	extern const std::vector<BindingProfileId> RAW_BINDING_PROFILE_IDS;

	class BindingManager
	{
	private:
		// Fields

		std::unordered_map<BindingProfileId, BindingProfile> _bindings = {}; // Key = binding profile ID, value = binding profile.

	public:
		// Constructors

		BindingManager() = default;

		// Getters

		const BindingProfile&       GetProfile(BindingProfileId profileId) const;
		const std::vector<EventId>& GetBoundEventIds(BindingProfileId profileId, ActionId actionId) const;

		// Setters

		void SetEventBinding(BindingProfileId profileId, ActionId actionId, EventId eventId);
		void SetProfile(BindingProfileId profileId, const BindingProfile& profile);

		// Utilities

		void Initialize(const BindingProfile& customKeyboardMouseBinds, const BindingProfile& customGamepadBinds);
	};
}
