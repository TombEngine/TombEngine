#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::Next
{
    struct DecodedImage
    {
        int                        Width  = 0;
        int                        Height = 0;
        RHI::PixelFormat           Format = RHI::PixelFormat::RGBA8Unorm;
        int                        MipLevels = 1;
        std::vector<unsigned char> Data   = {}; // All mip levels, tightly packed, largest first.
    };

    // Decodes an encoded level texture (PNG or DDS) into GPU-ready pixels. PNG yields a single RGBA8 level,
    // whose mips are generated on the GPU; DDS keeps its stored format and mip chain.
    bool DecodeImage(const std::vector<unsigned char>& encodedData, DecodedImage& image);

    int GetMipLevelCount(int width, int height);
}
