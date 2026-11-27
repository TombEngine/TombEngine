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
            int                       UnusedFrames = 0;
        };

        IDevice*           _device  = nullptr;
        std::vector<Entry> _entries = {};

    public:
        void Initialize(IDevice& device);

        // Destroys every target; used when sizes change and the pooled targets no longer match.
        void Clear();

        // Per-frame housekeeping that destroys targets nobody has acquired for a long time.
        void CollectUnused();

        // Pointers returned by Acquire stay valid until Clear or CollectUnused destroys the target; the contents
        // are undefined, so the first pass must clear the target.
        ITexture* Acquire(const TextureDesc& desc);
        void      Release(ITexture* texture);
        void      Reserve(const TextureDesc& desc); // Creates a free target now, if none matches, for a later Acquire.
        int       GetCount() const { return (int)_entries.size(); }
    };
}
