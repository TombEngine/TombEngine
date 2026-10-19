#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::Backend
{
    struct DecodedImage
    {
        int                        Width  = 0;
        int                        Height = 0;
        RHI::PixelFormat           Format = RHI::PixelFormat::RGBA8Unorm;
        int                        MipLevels = 1;
        std::vector<unsigned char> Data   = {}; // All mip levels, tightly packed, largest first.
    };

    // Size and format of an encoded image, read from its header. PNG images report one level: their mips are generated.
    // Opaque BC3 images report BC1, the format DecodeImage converts them to.
    struct ImageInfo
    {
        int              Width     = 0;
        int              Height    = 0;
        RHI::PixelFormat Format    = RHI::PixelFormat::Unknown;
        int              MipLevels = 1;
    };

    bool ReadImageInfo(const std::vector<unsigned char>& encodedData, ImageInfo& info);

    // Decodes an encoded level texture (PNG, JPEG or DDS) into GPU-ready pixels. PNG and JPEG yield a single RGBA8 level,
    // whose mips are generated on the GPU; DDS keeps its stored format and mip chain.
    bool DecodeImage(const std::vector<unsigned char>& encodedData, DecodedImage& image);

    int GetMipLevelCount(int width, int height);
}
