#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::RHI::SDLGPU
{
    // Creates an SDL3 GPU device on the given SDL driver ("vulkan" or "direct3d12") and claims the window.
    // Returns nullptr when the driver is unavailable, so the caller can fall back to another backend.
    std::unique_ptr<IDevice> CreateSDLGPUDevice(SDL_Window* window, const char* driverName, bool debugMode, bool preferLowPower);
}
