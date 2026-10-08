#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::RHI::DirectX11
{
    // Creates a Direct3D 11 device implementing the RHI on top of the immediate context.
    // Returns nullptr on failure or on platforms without Direct3D 11.
    std::unique_ptr<IDevice> CreateDX11Device(SDL_Window* window, bool debugMode);
}
