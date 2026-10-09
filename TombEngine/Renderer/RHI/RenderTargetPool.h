#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::RHI
{
    // Render targets for intermediate results that live within a single pass sequence. Acquire returns a free
    // target matching the description, creating it on first use; Release makes it available to later passes.
    class RenderTargetPool
    {
    private:
        struct Entry
        {
            std::unique_ptr<ITexture> Texture = nullptr;
            bool                      IsInUse = false;
        };

        IDevice*           _device  = nullptr;
        std::vector<Entry> _entries = {};

    public:
        void Initialize(IDevice& device);
        void Clear();

        ITexture* Acquire(const TextureDesc& desc);
        void      Release(ITexture* texture);
    };
}
