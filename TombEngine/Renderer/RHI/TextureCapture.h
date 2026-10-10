#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::RHI
{
    // Reads back a layer of an RGBA8, BGRA8, R8 or R32 float texture and writes it as PNG; single channel formats
    // become grayscale. Waits for the GPU; debugging use only.
    bool SaveTextureToPng(IDevice& device, ITexture* texture, const std::string& path, int layer = 0);

    // Texture downloads without stalling: Record adds the download to a command buffer, Submit submits it with a fence,
    // and Poll writes the PNG files, or calls the callbacks, of the downloads the GPU has completed some frames later.
    class TextureReadback
    {
    public:
        using ReadCallback = std::function<void(const TextureDesc& desc, const unsigned char* data)>;

    private:
        struct Request
        {
            std::unique_ptr<ITransferBuffer> Buffer   = nullptr;
            TextureDesc                      Desc     = {};
            std::string                      Path     = {};
            ReadCallback                     Callback = nullptr; // Called instead of writing a PNG file.
            std::shared_ptr<IFence>          Fence    = nullptr; // Shared by the requests of one submission.
        };

        std::vector<Request> _requests = {};

    public:
        bool HasUnsubmittedRequests() const;

        void Record(IDevice& device, ICopyPass& copyPass, ITexture* texture, const std::string& path);
        void Record(IDevice& device, ICopyPass& copyPass, ITexture* texture, ReadCallback callback);
        void Submit(ICommandBuffer& commandBuffer);
        void Poll(IDevice& device, bool wait);
        void Clear(); // Releases the requests left, before the device is destroyed.
    };
}
