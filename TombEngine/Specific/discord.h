#pragma once

namespace TEN::Utils::Discord
{
    void InitializeDiscord();

    // Publishes presence data for the given level if it changed.
    void UpdateDiscordPresence(int levelIndex);

    void DeInitializeDiscord();
}
