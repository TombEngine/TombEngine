#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::RHI
{
    // Reads back a layer of an RGBA8, BGRA8, R8 or R32 float texture and writes it as PNG; single channel formats
    // become grayscale. Waits for the GPU; debugging use only.
    bool SaveTextureToPng(IDevice& device, ITexture* texture, const std::string& path, int layer = 0);
}
