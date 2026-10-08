#include "framework.h"
#include "Renderer/RHI/TextureCapture.h"

namespace TEN::Renderer::RHI
{
    bool SaveTextureToPng(IDevice& device, ITexture* texture, const std::string& path)
    {
        const auto& desc = texture->GetDesc();
        int size = GetTextureDataSize(desc.Format, desc.Width, desc.Height, 1);
        auto transferBuffer = device.CreateTransferBuffer(TransferBufferUsage::Download, size);
        if (transferBuffer == nullptr)
            return false;

        auto* commandBuffer = device.AcquireCommandBuffer();
        auto* copyPass = commandBuffer->BeginCopyPass();

        auto region = TextureRegion{};
        region.Texture = texture;
        region.Width = desc.Width;
        region.Height = desc.Height;
        copyPass->DownloadFromTexture(region, TextureTransferInfo{ transferBuffer.get(), 0, 0, 0 });

        commandBuffer->EndCopyPass();
        commandBuffer->Submit();
        device.WaitIdle();

        auto* data = (unsigned char*)device.MapTransferBuffer(transferBuffer.get(), false);

        // Single channel targets are saved as grayscale.
        auto expanded = std::vector<unsigned char>{};
        if (desc.Format == PixelFormat::R8Unorm)
        {
            expanded.resize(desc.Width * desc.Height * 4);
            for (int i = 0; i < desc.Width * desc.Height; i++)
            {
                expanded[i * 4 + 0] = expanded[i * 4 + 1] = expanded[i * 4 + 2] = data[i];
                expanded[i * 4 + 3] = 255;
            }
        }

        auto* pixels = expanded.empty() ? data : expanded.data();
        auto pixelFormat = (desc.Format == PixelFormat::BGRA8Unorm) ? SDL_PIXELFORMAT_BGRA32 : SDL_PIXELFORMAT_RGBA32;
        auto* surface = SDL_CreateSurfaceFrom(desc.Width, desc.Height, pixelFormat, pixels, desc.Width * 4);
        bool result = (surface != nullptr) && SDL_SavePNG(surface, path.c_str());
        SDL_DestroySurface(surface);
        device.UnmapTransferBuffer(transferBuffer.get());

        return result;
    }
}
