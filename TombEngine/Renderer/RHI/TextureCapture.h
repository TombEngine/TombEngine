#pragma once

#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer::RHI
{
    // Reads back a layer of an RGBA8, BGRA8, R8 or R32 float texture and writes it as PNG; single channel formats
    // become grayscale. Waits for the GPU; debugging use only.
    bool SaveTextureToPng(IDevice& device, ITexture* texture, const std::string& path, int layer = 0);

    // PNG captures without stalling: Record adds the download to a command buffer, Submit submits it with a fence,
    // and Poll writes the files of the downloads the GPU has completed, some frames later.
    class TextureReadback
    {
    private:
        struct Request
        {
            std::unique_ptr<ITransferBuffer> Buffer = nullptr;
            TextureDesc                      Desc   = {};
            std::string                      Path   = {};
            std::shared_ptr<IFence>          Fence  = nullptr; // Shared by the requests of one submission.
        };

        std::vector<Request> _requests = {};

    public:
        bool HasUnsubmittedRequests() const;

        void Record(IDevice& device, ICopyPass& copyPass, ITexture* texture, const std::string& path);
        void Submit(ICommandBuffer& commandBuffer);
        void Poll(IDevice& device, bool wait);
    };
}
