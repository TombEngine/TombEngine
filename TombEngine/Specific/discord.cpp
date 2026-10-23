#include "framework.h"
#include "Specific/Discord.h"

#include "Scripting/Internal/LanguageScript.h"
#include "Scripting/Internal/TEN/Flow/FlowHandler.h"

using namespace TEN::Scripting;

#if PLATFORM_64BIT

namespace TEN::Utils::Discord
{
    static constexpr auto APPLICATION_ID   = "1521229664541081730";
    static constexpr auto STRING_MAX_BYTES = 128;

    static constexpr auto FALLBACK_DETAILS = "TombEngine";
    static constexpr auto FALLBACK_STATE   = "In Game";

    static bool DiscordInitialized  = false;
    static long long StartTimestamp = 0;

    static std::string PresenceDetails;
    static std::string PresenceState;

    // Truncates UTF-8 text to a byte count without splitting a multibyte code point.
    static std::string TruncateUtf8(const char* text, int maxBytes)
    {
        if (!text)
            return std::string();

        auto result = std::string(text);
        if ((int)result.size() <= maxBytes)
            return result;

        int length = maxBytes;

        // Step back over continuation bytes so the last code point stays intact.
        while (length > 0 && ((unsigned char)result[length] & 0xC0) == 0x80)
            length--;

        result.resize(length);
        return result;
    }

    static std::string GetLevelName(int levelIndex)
    {
        auto* level = g_GameFlow->GetLevel(levelIndex);
        if (!level)
            return std::string();

        const char* name = g_GameFlow->GetString(level->NameStringKey.c_str());
        return name ? std::string(name) : std::string();
    }

    void InitializeDiscord()
    {
        if (DiscordInitialized)
            return;

        // Anchor the elapsed timer to the app session start so it never resets.
        StartTimestamp = (long long)time(nullptr);

        Discord_Initialize(APPLICATION_ID, nullptr, 1, nullptr);
        DiscordInitialized = true;
    }

    void UpdateDiscordPresence(int levelIndex)
    {
        auto levelName = GetLevelName(levelIndex);
        auto details   = TruncateUtf8(g_GameFlow->GetString(STRING_WINDOW_TITLE), STRING_MAX_BYTES);
        auto state     = TruncateUtf8(levelName.c_str(), STRING_MAX_BYTES);

        if (details.empty())
            details = FALLBACK_DETAILS;
        if (state.empty())
            state = FALLBACK_STATE;

        // Skip redundant IPC traffic when the published presence wouldn't change.
        if (details == PresenceDetails && state == PresenceState)
            return;

        PresenceDetails = details;
        PresenceState   = state;

        DiscordRichPresence presence = {};
        presence.details        = PresenceDetails.c_str();
        presence.state          = PresenceState.c_str();
        presence.startTimestamp = StartTimestamp;

        // The stable start timestamp keeps Discord's elapsed timer counting on its own between updates.
        Discord_UpdatePresence(&presence);
    }

    void DeInitializeDiscord()
    {
        if (!DiscordInitialized)
            return;

        Discord_ClearPresence();
        Discord_Shutdown();
        DiscordInitialized = false;
    }
}

#else

// Discord RPC binaries are only available for 64-bit platforms, so all calls are no-ops.
namespace TEN::Utils::Discord
{
    void InitializeDiscord()
    {
    }

    void UpdateDiscordPresence(int)
    {
    }

    void DeInitializeDiscord()
    {
    }
}

#endif