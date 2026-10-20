#pragma once

#include "Renderer/RHI/RHIDevice.h"

// Decoding of level and overlay images at load. Level textures arrive from Tomb Editor as DDS (BC1, BC3 or BC5 with
// mips) or, with compression off in Tomb Editor, as PNG; the engine never compresses them itself.

namespace TEN::Renderer::Backend
{
    // Pixels of a decoded image, ready for a texture upload.
    struct DecodedImage
    {
        int                        Width  = 0;
        int                        Height = 0;
        RHI::PixelFormat           Format = RHI::PixelFormat::RGBA8Unorm;
        int                        MipLevels = 1;
        std::vector<unsigned char> Data   = {}; // All mip levels, tightly packed, largest first.
    };

    // Size and format of an encoded image, read from its header. PNG images report one level: their mips are generated.
    struct ImageInfo
    {
        int              Width     = 0;
        int              Height    = 0;
        RHI::PixelFormat Format    = RHI::PixelFormat::Unknown;
        int              MipLevels = 1;
    };

    // Reads only the header, so texture array groups can be planned before any image is decoded.
    bool ReadImageInfo(const std::vector<unsigned char>& encodedData, ImageInfo& info);

    // Decodes an encoded level texture (PNG, JPEG or DDS) into GPU-ready pixels. PNG and JPEG yield a single RGBA8 level,
    // whose mips are generated on the GPU; DDS keeps its stored format and mip chain.
    bool DecodeImage(const std::vector<unsigned char>& encodedData, DecodedImage& image);

    // Length of the full mip chain down to 1x1.
    int GetMipLevelCount(int width, int height);
}
