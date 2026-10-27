#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::RHI
{
    // Linear allocator over one cycled transfer buffer: every upload of a copy pass is written into the same
    // mapped buffer, and the copies are recorded when the pass ends. Cycling hands out a fresh buffer while
    // previous frames still read the old one. Destinations are cycled too, so each gets one upload per pass.
    class UploadRing
    {
    private:
        struct PendingBufferCopy
        {
            IBuffer* Destination       = nullptr;
            int      SourceOffset      = 0;
            int      DestinationOffset = 0;
            int      Size              = 0;
        };

        struct PendingTextureCopy
        {
            TextureRegion Destination  = {};
            int           SourceOffset = 0;
        };

        IDevice*                         _device          = nullptr;
        std::unique_ptr<ITransferBuffer> _buffer          = nullptr;
        unsigned char*                   _mapped          = nullptr;
        ICopyPass*                       _copyPass        = nullptr;
        int                              _capacity        = 0;
        int                              _offset          = 0;
        int                              _minimumCapacity = 0;
        int                              _underusedPasses  = 0;
        std::vector<PendingBufferCopy>   _bufferCopies   = {};
        std::vector<PendingTextureCopy>  _textureCopies  = {};
        std::vector<std::unique_ptr<ITransferBuffer>> _retiredBuffers = {}; // Outgrown buffers still read by recorded copies.

        int  Allocate(int size);
        void Grow(int size);
        void Flush();
        void Trim();

    public:
        void Initialize(IDevice& device, int capacity);
        void Release();

        // Uploads are recorded into the copy pass given to Begin, which must stay open until End.
        void Begin(ICopyPass& copyPass);
        void UploadToBuffer(IBuffer* destination, int destinationOffset, const void* data, int size);
        void UploadToTexture(const TextureRegion& destination, const void* data, int size);
        void UploadToTextures(const TextureRegion* destinations, int count, const void* data, int size);
        void End();

        int GetCapacity() const { return _capacity; }
    };
}
