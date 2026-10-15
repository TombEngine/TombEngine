#pragma once

#include "Renderer/RHI/RHITypes.h"

namespace TEN::Renderer::RHI
{
    enum class MemoryCategory
    {
        Texture,
        RenderTarget,
        Buffer,
        TransferBuffer,

        Count
    };

    // Memory of the adapter reported by the operating system; zero where it is not available.
    struct AdapterMemory
    {
        unsigned long long DedicatedVideoMemory = 0;
        unsigned long long SharedSystemMemory   = 0;
        unsigned long long ProcessUsage         = 0; // Video memory used by the process, all allocations included.
        unsigned long long ProcessBudget        = 0; // Video memory the process can use before the system evicts it.
        unsigned long long AdapterUsage         = 0; // Dedicated video memory used by every process.
    };

    struct MemoryStats
    {
        std::array<unsigned long long, (int)MemoryCategory::Count> Bytes  = {};
        std::array<int, (int)MemoryCategory::Count>                Counts = {};
        AdapterMemory                                              Adapter = {};

        unsigned long long GetTotal() const;
    };

    // Size of the resource, owned by the backend resource object: the size is counted while the object lives.
    class MemoryAllocation
    {
    private:
        MemoryCategory     _category = MemoryCategory::Buffer;
        unsigned long long _bytes    = 0;

    public:
        MemoryAllocation() = default;
        MemoryAllocation(MemoryCategory category, unsigned long long bytes);
        MemoryAllocation(const MemoryAllocation&) = delete;
        MemoryAllocation& operator=(const MemoryAllocation&) = delete;
        MemoryAllocation& operator=(MemoryAllocation&& other) noexcept;
        ~MemoryAllocation();
    };

    unsigned long long GetTextureMemorySize(const TextureDesc& desc);
    MemoryCategory     GetTextureMemoryCategory(const TextureDesc& desc);

    // Totals of the live resources of every device, and the adapter memory of the named device.
    MemoryStats GetMemoryStats(const std::string& deviceName);
}
