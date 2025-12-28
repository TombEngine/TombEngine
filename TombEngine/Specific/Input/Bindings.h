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

	extern const BindingProfile                DEFAULT_USER_KEYBOARD_MOUSE_BINDING_PROFILE;
	extern const BindingProfile                DEFAULT_USER_GAMEPAD_BINDING_PROFILE;
	extern const std::vector<BindingProfileId> CUSTOM_BINDING_PROFILE_IDS;
	extern const std::vector<BindingProfileId> RAW_BINDING_PROFILE_IDS;

	class BindingManager
	{
	private:
		// Fields

		std::vector<BindingProfile> _bindings = {}; // Index = binding profile ID.

	public:
		// Constructors

		BindingManager() = default;

		// Getters

		const std::string&    GetBoundKeyName(ActionId actionID);
		const BindingProfile& GetProfile(BindingProfileId profileId) const;

		// Setters

		void SetProfile(BindingProfileId profileId, const BindingProfile& newProfile);
		void SetBinding(BindingProfileId profileId, ActionId actionId, EventId eventId);

		// Utilities

		void Initialize(const BindingProfile& customKeyboardMouseProfile, const BindingProfile& customGamepadProfile);
	};
}
