#include "framework.h"
#include "Renderer/RHI/MemoryTracker.h"

#ifdef _WIN32
#include <dxgi1_4.h>
#include <wrl/client.h>
#endif

#include "Specific/trutils.h"

namespace TEN::Renderer::RHI
{
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

#ifdef _WIN32
    // DXGI describes the adapters of both backends; the adapter is found by the name the device reports.
    static AdapterMemory GetAdapterMemory(const std::string& deviceName)
    {
        using Microsoft::WRL::ComPtr;

        auto memory = AdapterMemory{};
        auto factory = ComPtr<IDXGIFactory1>{};
        if (deviceName.empty() || FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
            return memory;

        auto name = TEN::Utils::ToWString(deviceName);
        auto adapter = ComPtr<IDXGIAdapter1>{};
        for (unsigned int i = 0; factory->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; i++)
        {
            auto desc = DXGI_ADAPTER_DESC1{};
            if (FAILED(adapter->GetDesc1(&desc)))
                continue;

            auto description = std::wstring(desc.Description);
            if (name.find(description) == std::wstring::npos && description.find(name) == std::wstring::npos)
                continue;

            memory.DedicatedVideoMemory = desc.DedicatedVideoMemory;
            memory.SharedSystemMemory = desc.SharedSystemMemory;

            auto adapter3 = ComPtr<IDXGIAdapter3>{};
            auto info = DXGI_QUERY_VIDEO_MEMORY_INFO{};
            if (SUCCEEDED(adapter.As(&adapter3)) && SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
            {
                memory.ProcessUsage = info.CurrentUsage;
                memory.ProcessBudget = info.Budget;
            }

            break;
        }

        return memory;
    }
#else
    static AdapterMemory GetAdapterMemory(const std::string& deviceName)
    {
        return AdapterMemory{};
    }
#endif

    MemoryStats GetMemoryStats(const std::string& deviceName)
    {
        auto stats = MemoryStats{};
        for (int i = 0; i < (int)MemoryCategory::Count; i++)
        {
            stats.Bytes[i] = TrackedBytes[i];
            stats.Counts[i] = TrackedCounts[i];
        }

        stats.Adapter = GetAdapterMemory(deviceName);
        return stats;
    }
}
