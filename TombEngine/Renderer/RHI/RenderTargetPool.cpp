#include "framework.h"
#include "Renderer/RHI/RenderTargetPool.h"

namespace TEN::Renderer::RHI
{
    static bool IsSameTarget(const TextureDesc& first, const TextureDesc& second)
    {
        return (first.Type == second.Type && first.Format == second.Format && first.Width == second.Width && first.Height == second.Height &&
                first.DepthOrLayers == second.DepthOrLayers && first.MipLevels == second.MipLevels && first.SampleCount == second.SampleCount &&
                first.Usage == second.Usage);
    }

    void RenderTargetPool::Initialize(IDevice& device)
    {
        _device = &device;
    }

    // Called when the frame size changes; every target must be released.
    void RenderTargetPool::Clear()
    {
        _entries.clear();
    }

    ITexture* RenderTargetPool::Acquire(const TextureDesc& desc)
    {
        for (auto& entry : _entries)
        {
            if (!entry.IsInUse && IsSameTarget(entry.Texture->GetDesc(), desc))
            {
                entry.IsInUse = true;
                entry.UnusedFrames = 0;
                return entry.Texture.get();
            }
        }

        auto poolDesc = desc;
        poolDesc.Name = "Transient[" + std::to_string(_entries.size()) + "]";

        auto texture = _device->CreateTexture(poolDesc);
        if (texture == nullptr)
            return nullptr;

        _entries.push_back(Entry{ std::move(texture), true });
        return _entries.back().Texture.get();
    }

    void RenderTargetPool::Reserve(const TextureDesc& desc)
    {
        auto* texture = Acquire(desc);
        if (texture != nullptr)
            Release(texture);
    }

    void RenderTargetPool::CollectUnused()
    {
        constexpr auto MAX_UNUSED_FRAMES = 300;
        for (auto entry = _entries.begin(); entry != _entries.end();)
        {
            if (entry->IsInUse || ++entry->UnusedFrames < MAX_UNUSED_FRAMES)
            {
                ++entry;
                continue;
            }

            entry = _entries.erase(entry);
        }
    }

    void RenderTargetPool::Release(ITexture* texture)
    {
        for (auto& entry : _entries)
        {
            if (entry.Texture.get() == texture)
            {
                entry.IsInUse = false;
                return;
            }
        }
    }
}
