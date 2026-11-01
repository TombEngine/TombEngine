#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::RHI::OpenGL
{
    // Sets the OpenGL 4.5 core context attributes and returns the window flag OpenGL needs at window creation.
    unsigned int PrepareGLWindow();

    // Creates an OpenGL 4.5 core device on a window created with the flags of PrepareGLWindow.
    // Returns nullptr when the window or the driver does not support it, so the caller can fall back to another backend.
    std::unique_ptr<IDevice> CreateGLDevice(SDL_Window* window, bool debugMode);
}
