#include "framework.h"
#include "Renderer/RHI/UploadRing.h"

namespace TEN::Renderer::RHI
{
    // Covers the offset rules of buffer to texture copies, including block compressed formats.
    constexpr auto UPLOAD_ALIGNMENT = 64;

    void UploadRing::Initialize(IDevice& device, int capacity)
    {
        _device = &device;
        _capacity = capacity;
        _minimumCapacity = capacity;
        _underusedPasses = 0;
        _buffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, _capacity);
    }

    void UploadRing::Release()
    {
        _bufferCopies.clear();
        _textureCopies.clear();
        _retiredBuffers.clear();
        _buffer = nullptr;
        _mapped = nullptr;
        _copyPass = nullptr;
        _device = nullptr;
    }

    void UploadRing::Begin(ICopyPass& copyPass)
    {
        _retiredBuffers.clear();
        Trim();
        _copyPass = &copyPass;
        _offset = 0;
        _mapped = nullptr;
    }

    void UploadRing::Trim()
    {
        constexpr auto TRIM_DELAY = 120;
        if (_underusedPasses < TRIM_DELAY)
            return;

        _underusedPasses = 0;
        int capacity = std::max(_minimumCapacity, _capacity / 2);
        auto buffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, capacity);
        if (buffer == nullptr)
            return;

        _buffer = std::move(buffer);
        _capacity = capacity;
    }

    int UploadRing::Allocate(int size)
    {
        int offset = (_offset + UPLOAD_ALIGNMENT - 1) & ~(UPLOAD_ALIGNMENT - 1);
        if (offset + size > _capacity)
        {
            Grow(size);
            offset = 0;
        }

        _offset = offset + size;
        if (_mapped == nullptr)
            _mapped = (unsigned char*)_device->MapTransferBuffer(_buffer.get(), true);

        return offset;
    }

    // Records the copies of the full buffer and continues in a larger one; the old buffer stays alive until
    // the next Begin, after the copy pass that reads it has been recorded.
    void UploadRing::Grow(int size)
    {
        Flush();
        _retiredBuffers.push_back(std::move(_buffer));

        _capacity = std::max(_capacity * 2, size);
        _buffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, _capacity);
        _mapped = (unsigned char*)_device->MapTransferBuffer(_buffer.get(), true);
        _offset = 0;
        _underusedPasses = 0;
    }

    void UploadRing::UploadToBuffer(IBuffer* destination, int destinationOffset, const void* data, int size)
    {
        if (size <= 0)
            return;

        int offset = Allocate(size);
        memcpy(_mapped + offset, data, size);
        _bufferCopies.push_back(PendingBufferCopy{ destination, offset, destinationOffset, size });
    }

    void UploadRing::UploadToTexture(const TextureRegion& destination, const void* data, int size)
    {
        UploadToTextures(&destination, 1, data, size);
    }

    // Destinations share the same texel layout and staging bytes until the copy pass completes.
    void UploadRing::UploadToTextures(const TextureRegion* destinations, int count, const void* data, int size)
    {
        if (size <= 0 || count <= 0)
            return;

        int offset = Allocate(size);
        memcpy(_mapped + offset, data, size);
        for (int i = 0; i < count; i++)
            _textureCopies.push_back(PendingTextureCopy{ destinations[i], offset });
    }

    // Copies are recorded once the buffer is unmapped. Destinations are cycled, as they were read by previous frames.
    void UploadRing::Flush()
    {
        if (_mapped == nullptr)
            return;

        _device->UnmapTransferBuffer(_buffer.get());
        _mapped = nullptr;

        for (const auto& copy : _bufferCopies)
            _copyPass->UploadToBuffer(_buffer.get(), copy.SourceOffset, copy.Destination, copy.DestinationOffset, copy.Size, true);

        for (const auto& copy : _textureCopies)
            _copyPass->UploadToTexture(TextureTransferInfo{ _buffer.get(), copy.SourceOffset, 0, 0 }, copy.Destination, true);

        _bufferCopies.clear();
        _textureCopies.clear();
    }

    void UploadRing::End()
    {
        Flush();
        if (_retiredBuffers.empty() && _capacity > _minimumCapacity && _offset <= _capacity / 4)
            _underusedPasses++;
        else
            _underusedPasses = 0;

        _copyPass = nullptr;
    }
}
