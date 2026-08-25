#include "framework.h"
#include "Specific/Discord.h"

#include "Scripting/Internal/LanguageScript.h"
#include "Scripting/Internal/TEN/Flow/FlowHandler.h"

using namespace TEN::Scripting;

namespace TEN::Utils::Discord
{
    static bool DiscordReady = false;
    static bool DiscordInitialized = false;
    static constexpr auto APPLICATION_ID = "1521229664541081730";

    void InitializeDiscord()
    {
        if (DiscordInitialized)
            return;

        DiscordEventHandlers handlers = {};
        handlers.ready = [](const DiscordUser*)
        {
            DiscordReady = true;
        };
        handlers.errored = [](int errorCode, const char* message)
        {
            TENLog("Discord RPC error: " + std::string(message) + " (" + std::to_string(errorCode) + ")", LogLevel::Error);
        };
        handlers.disconnected = [](int errorCode, const char* message)
        {
            DiscordReady = false;
            TENLog("Discord RPC disconnected: " + std::string(message) + " (" + std::to_string(errorCode) + ")", LogLevel::Warning);
        };

        Discord_Initialize(APPLICATION_ID, &handlers, 1, nullptr);
        DiscordInitialized = true;
    }

    void UpdateDiscord()
    {
        Discord_RunCallbacks();

        if (!DiscordReady)
            return;

        // Details (top row): game window title, e.g. "TombEngine".
        const char* title = g_GameFlow->GetString(STRING_WINDOW_TITLE);

        // State (second row): current level name, e.g. "The Great Pyramid".
        const char* levelName;

        if (!g_GameFlow->GetLevel(CurrentLevel))
            levelName = nullptr;
        else
            levelName = g_GameFlow->GetString(g_GameFlow->GetLevel(CurrentLevel)->NameStringKey.c_str());

        DiscordRichPresence presence = {};
        presence.details = (title && title[0]) ? title : "TombEngine";
        presence.state   = (levelName && levelName[0]) ? levelName : "In Game";

        Discord_UpdatePresence(&presence);
    }

    void DeInitializeDiscord()
    {
        if (!DiscordInitialized)
            return;

        Discord_ClearPresence();
        Discord_Shutdown();
        DiscordReady = false;
        DiscordInitialized = false;
    }
}