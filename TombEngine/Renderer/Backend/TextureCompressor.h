#pragma once

struct TEXTURE;

namespace TEN::Renderer::Backend
{
    // Replaces the PNG and JPEG maps of level textures with block compressed DDS images: BC1 for opaque maps, BC3 with
    // transparency and BC5 for normal maps, with their mip chains. Results are cached on disk by the hash of the source
    // image, so a level is compressed only on its first load.
    void CompressLevelTextures(const std::vector<TEXTURE*>& textures, const std::string& cacheDirectory);
}
