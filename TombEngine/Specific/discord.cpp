#include "framework.h"
#include "Game/Lara/lara.h"
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

static const std::string RPC_GetLevelName()
{
    if (!g_GameFlow->GetLevel(CurrentLevel))
        return "In Title";

    return g_GameFlow->GetString(g_GameFlow->GetLevel(CurrentLevel)->NameStringKey.c_str());
}

void RPC_Update()
{
    Discord_RunCallbacks();

    if (!gReady)
        return;

    auto levelName = RPC_GetLevelName();

    static char healthBuf[32];
    sprintf(healthBuf, "Health: %d", LaraItem->HitPoints);

    DiscordRichPresence presence = {};
    presence.details = levelName.c_str();
    presence.state   = healthBuf;

    Discord_UpdatePresence(&presence);
}

void RPC_close()
{
    Discord_ClearPresence();
    Discord_Shutdown();
    gReady = false;
}