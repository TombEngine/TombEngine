#include "framework.h"
#include "Scripting/Internal/LanguageScript.h"
#include "Scripting/Internal/TEN/Flow/FlowHandler.h"

static bool           gReady = false;
static constexpr auto APPLICATION_ID = "1521229664541081730";

using namespace TEN::Scripting;

void RPC_Init()
{
    DiscordEventHandlers handlers = {};
    handlers.ready = [](const DiscordUser* /*request*/)
    {
        gReady = true;
    };
    handlers.errored = [](int errorCode, const char* message)
    {
        std::cerr << "Discord RPC error: " << message << " (" << errorCode << ")\n";
    };
    handlers.disconnected = [](int errorCode, const char* message)
    {
        gReady = false;
        std::cerr << "Discord RPC disconnected: " << message << " (" << errorCode << ")\n";
    };

    Discord_Initialize(APPLICATION_ID, &handlers, 1, nullptr);
}

// Returns str if non-null and non-empty, otherwise returns fallback.
static const char* RPC_SafeStr(const char* str, const char* fallback)
{
    return (str && str[0]) ? str : fallback;
}

// Returns the current level display name, or nullptr if not in a level.
static const char* RPC_GetLevelName()
{
    if (!g_GameFlow->GetLevel(CurrentLevel))
        return nullptr;

    return g_GameFlow->GetString(g_GameFlow->GetLevel(CurrentLevel)->NameStringKey.c_str());
}

void RPC_Update()
{
    Discord_RunCallbacks();

    if (!gReady)
        return;

    // Details (top row): game window title, e.g. "TombEngine".
    const char* title = RPC_SafeStr(g_GameFlow->GetString(STRING_WINDOW_TITLE), "TombEngine");

    // State (second row): current level name, e.g. "The Great Pyramid".
    const char* levelName = RPC_SafeStr(RPC_GetLevelName(), "In Game");

    DiscordRichPresence presence = {};
    presence.details = title;
    presence.state   = levelName;

    Discord_UpdatePresence(&presence);
}

void RPC_close()
{
    Discord_ClearPresence();
    Discord_Shutdown();
    gReady = false;
}