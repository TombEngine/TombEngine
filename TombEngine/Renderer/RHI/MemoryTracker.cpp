#include "framework.h"
#include "Renderer/RHI/MemoryTracker.h"

#include "Specific/EngineMain.h"

namespace TEN::Renderer::RHI
{
    // Atomic, as resources are created and released on several threads.
    static std::array<std::atomic<unsigned long long>, (int)MemoryCategory::Count> TrackedBytes  = {};
    static std::array<std::atomic<int>, (int)MemoryCategory::Count>                TrackedCounts = {};

    unsigned long long MemoryStats::GetTotal() const
    {
        unsigned long long total = 0;
        for (auto bytes : Bytes)
            total += bytes;

        return total;
    }

    MemoryAllocation::MemoryAllocation(MemoryCategory category, unsigned long long bytes) :
        _category(category),
        _bytes(bytes)
    {
        TrackedBytes[(int)_category] += _bytes;
        TrackedCounts[(int)_category]++;
    }

    // Uncounts the current size and takes over the size of the other allocation, which stops counting it.
    MemoryAllocation& MemoryAllocation::operator=(MemoryAllocation&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_bytes != 0)
        {
            TrackedBytes[(int)_category] -= _bytes;
            TrackedCounts[(int)_category]--;
        }

        _category = other._category;
        _bytes = other._bytes;
        other._bytes = 0;
        return *this;
    }

    MemoryAllocation::~MemoryAllocation()
    {
        if (_bytes == 0)
            return;

        TrackedBytes[(int)_category] -= _bytes;
        TrackedCounts[(int)_category]--;
    }

    // Every mip of every layer, as the formats are stored; drivers add alignment and padding on top.
    unsigned long long GetTextureMemorySize(const TextureDesc& desc)
    {
        bool isVolume = (desc.Type == TextureType::Texture3D);
        unsigned long long bytes = 0;
        for (int mip = 0; mip < std::max(desc.MipLevels, 1); mip++)
        {
            int width = std::max(desc.Width >> mip, 1);
            int height = std::max(desc.Height >> mip, 1);
            int depth = isVolume ? std::max(desc.DepthOrLayers >> mip, 1) : std::max(desc.DepthOrLayers, 1);
            bytes += (unsigned long long)GetTextureDataSize(desc.Format, width, height, depth);
        }

        return (bytes * std::max(desc.SampleCount, 1));
    }

    MemoryCategory GetTextureMemoryCategory(const TextureDesc& desc)
    {
        bool isTarget = (desc.Usage & (TextureUsage::ColorTarget | TextureUsage::DepthStencilTarget)) != 0;
        return (isTarget ? MemoryCategory::RenderTarget : MemoryCategory::Texture);
    }

    MemoryStats GetMemoryStats(const std::string& deviceName)
    {
        auto stats = MemoryStats{};
        for (int i = 0; i < (int)MemoryCategory::Count; i++)
        {
            stats.Bytes[i] = TrackedBytes[i];
            stats.Counts[i] = TrackedCounts[i];
        }

        if (g_Platform != nullptr)
            stats.Adapter = g_Platform->GetVideoMemoryInfo(deviceName);
        return stats;
    }
}
