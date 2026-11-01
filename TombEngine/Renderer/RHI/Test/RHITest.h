#pragma once

#include "Renderer/RHI/RHITypes.h"

struct SDL_Window;

namespace TEN::Renderer::RHI
{
    // Renders a deterministic validation scene with the given backend for a number of frames, then saves
    // the last frame to Logs/RHITest_<backend>.png. Must run on the thread that created the window.
    bool RunRHITest(SDL_Window* window, GraphicsApi backend, const std::string& gameDirectory, int frameCount);
}
