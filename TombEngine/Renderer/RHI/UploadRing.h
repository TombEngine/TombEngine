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
        // Copies are queued while the staging data is written and recorded together by Flush.
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

        // Allocate reserves aligned staging space, Grow switches to a larger buffer when it is full, Flush records
        // the queued copies and Trim shrinks the buffer after a long run of mostly empty passes.
        int  Allocate(int size);
        void Grow(int size);
        void Flush();
        void Trim();

    public:
        // Capacity in bytes of the initial staging buffer; it is also the size the ring never shrinks below.
        void Initialize(IDevice& device, int capacity);
        void Release();

        // Uploads are recorded into the copy pass given to Begin, which must stay open until End.
        void Begin(ICopyPass& copyPass);
        // The data is copied into staging memory immediately, so the source may be freed on return. Each
        // destination may receive one upload per pass, as destinations are cycled when the copies are recorded.
        void UploadToBuffer(IBuffer* destination, int destinationOffset, const void* data, int size);
        void UploadToTexture(const TextureRegion& destination, const void* data, int size);
        // Writes the same texel data to several destinations (for example the faces of a cube map) using one allocation.
        void UploadToTextures(const TextureRegion* destinations, int count, const void* data, int size);

        // Records the pending copies into the copy pass; must be called before the pass ends.
        void End();

        int GetCapacity() const { return _capacity; }
    };
}
