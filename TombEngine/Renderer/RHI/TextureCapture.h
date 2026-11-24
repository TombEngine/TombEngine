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
        // True when downloads were recorded that no submitted command buffer carries yet.
        bool HasUnsubmittedRequests() const;

        // Record must be called inside an open copy pass, and the texture must be left unchanged until the command
        // buffer is submitted. Only mip 0 of layer 0 is read. The callback data is valid only during the call.
        void Record(IDevice& device, ICopyPass& copyPass, ITexture* texture, const std::string& path);
        void Record(IDevice& device, ICopyPass& copyPass, ITexture* texture, ReadCallback callback);

        // Replaces the plain Submit of the frame so pending downloads are tied to a fence.
        void Submit(ICommandBuffer& commandBuffer);
        std::shared_ptr<IFence> SubmitWithFence(ICommandBuffer& commandBuffer); // Always fenced, for frame pacing.
        // Call once per frame; completed requests are removed, pending ones stay for a later call.
        void Poll(IDevice& device, bool wait);
        void Clear(); // Releases the requests left, before the device is destroyed.
    };
}
