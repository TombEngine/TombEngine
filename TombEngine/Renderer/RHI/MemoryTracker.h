#pragma once

#include "Renderer/RHI/RHITypes.h"
#include "Specific/Platform/ISubsystem.h"

// Estimated GPU memory of the live RHI resources, counted by backend resource objects through MemoryAllocation,
// plus the adapter memory the operating system reports, for the debug statistics.

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

    using AdapterMemory = TEN::Platform::VideoMemoryInfo;

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
    MemoryCategory     GetTextureMemoryCategory(const TextureDesc& desc); // RenderTarget for color or depth targets.

    // Totals of the live resources of every device, and the adapter memory of the named device.
    MemoryStats GetMemoryStats(const std::string& deviceName);
}
