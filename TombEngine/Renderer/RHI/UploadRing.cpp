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
        _copyPass = &copyPass;
        _offset = 0;
        _mapped = (unsigned char*)_device->MapTransferBuffer(_buffer.get(), true);
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
        if (size <= 0)
            return;

        int offset = Allocate(size);
        memcpy(_mapped + offset, data, size);
        _textureCopies.push_back(PendingTextureCopy{ destination, offset });
    }

    // Copies are recorded once the buffer is unmapped. Destinations are cycled, as they were read by previous frames.
    void UploadRing::Flush()
    {
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
        _copyPass = nullptr;
    }
}
