#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::RHI
{
    // Reads back an RGBA8 or BGRA8 texture and writes it as PNG. Waits for the GPU; debugging use only.
    bool SaveTextureToPng(IDevice& device, ITexture* texture, const std::string& path);
}
