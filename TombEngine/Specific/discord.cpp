#include "framework.h"
#include "Scripting/Internal/TEN/Flow/FlowHandler.h"
#include <Game/savegame.h>

static Discord_Client   gClient;
static Discord_Activity gActivity;
static bool             gClientReady = false;
static constexpr auto   APPLICATION_ID = 1521229664541081730ULL;

using namespace TEN::Scripting;

void RPC_Init()
{
    Discord_Client_Init(&gClient);
    Discord_Client_SetApplicationId(&gClient, APPLICATION_ID);
    Discord_Activity_Init(&gActivity);
    gClientReady = true;
}

static const std::string RPC_GetLevelName()
{
    if (!g_GameFlow->GetLevel(CurrentLevel))
        return "In Title";

    return g_GameFlow->GetString(g_GameFlow->GetLevel(CurrentLevel)->NameStringKey.c_str());
}

static const char* RPC_GetTimer()
{
    static char buf[64];
    auto& gameTime = SaveGame::Statistics.Game.TimeTaken;
    sprintf(buf, "%02d:%02d:%02d", gameTime.GetHours(), gameTime.GetMinutes(), gameTime.GetSeconds());
    return buf;
}

static void RPC_UpdateCallback(Discord_ClientResult* result, void* /*userData*/)
{
    if (!Discord_ClientResult_Successful(result))
        std::cerr << "Failed to update Rich Presence\n";
}

void RPC_Update()
{
    if (!gClientReady)
        return;

    auto        levelName = RPC_GetLevelName();
    const char* timer     = RPC_GetTimer();

    Discord_String stateStr;
    stateStr.ptr  = (uint8_t*)timer;
    stateStr.size = strlen(timer);
    Discord_Activity_SetDetails(&gActivity, &stateStr);

    Discord_String detailsStr;
    detailsStr.ptr  = (uint8_t*)levelName.c_str();
    detailsStr.size = levelName.size();
    Discord_Activity_SetState(&gActivity, &detailsStr);

    Discord_Client_UpdateRichPresence(&gClient, &gActivity, RPC_UpdateCallback, nullptr, nullptr);
}

void RPC_close()
{
    if (!gClientReady)
        return;

    Discord_Activity_Drop(&gActivity);
    Discord_Client_Drop(&gClient);
    gClientReady = false;
}