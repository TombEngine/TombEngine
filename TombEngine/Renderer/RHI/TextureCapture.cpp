#include "framework.h"
#include "Renderer/RHI/TextureCapture.h"

namespace TEN::Renderer::RHI
{
    // Records the download of mip 0 of a layer into a new tightly packed transfer buffer.
    static std::unique_ptr<ITransferBuffer> RecordDownload(IDevice& device, ICopyPass& copyPass, ITexture* texture, int layer)
    {
        const auto& desc = texture->GetDesc();
        int size = GetTextureDataSize(desc.Format, desc.Width, desc.Height, 1);
        auto transferBuffer = device.CreateTransferBuffer(TransferBufferUsage::Download, size);
        if (transferBuffer == nullptr)
            return nullptr;

        auto region = TextureRegion{};
        region.Texture = texture;
        region.Layer = layer;
        region.Width = desc.Width;
        region.Height = desc.Height;
        copyPass.DownloadFromTexture(region, TextureTransferInfo{ transferBuffer.get(), 0, 0, 0 });
        return transferBuffer;
    }

    // IEEE half float to float, for the R16 float targets (accumulated ambient occlusion).
    static float GetHalfValue(unsigned short half)
    {
        unsigned int sign = (unsigned int)(half & 0x8000) << 16;
        unsigned int exponent = (half >> 10) & 0x1F;
        unsigned int mantissa = half & 0x3FF;

        unsigned int bits = 0;
        if (exponent == 0)
        {
            // Zero or subnormal: small enough to read as zero for an image.
            bits = sign;
        }
        else if (exponent == 31)
        {
            bits = sign | 0x7F800000 | (mantissa << 13);
        }
        else
        {
            bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
        }

        float value = 0.0f;
        memcpy(&value, &bits, sizeof(value));
        return value;
    }

    static bool WritePng(const TextureDesc& desc, const unsigned char* data, const std::string& path)
    {
        bool isSingleChannel = (desc.Format == PixelFormat::R8Unorm || desc.Format == PixelFormat::R16Float || desc.Format == PixelFormat::R32Float);
        if (!isSingleChannel && desc.Format != PixelFormat::RGBA8Unorm && desc.Format != PixelFormat::BGRA8Unorm)
        {
            TENLog("Texture capture: unsupported format of " + desc.Name + ".", LogLevel::Warning);
            return false;
        }

        // Single channel targets are saved as grayscale.
        auto expanded = std::vector<unsigned char>{};
        if (isSingleChannel)
        {
            expanded.resize(desc.Width * desc.Height * 4);
            for (int i = 0; i < desc.Width * desc.Height; i++)
            {
                // Depth values crowd near 1, so R32 float is stretched with a power curve.
                unsigned char value = data[i];
                if (desc.Format == PixelFormat::R16Float)
                    value = (unsigned char)(std::clamp(GetHalfValue(((const unsigned short*)data)[i]), 0.0f, 1.0f) * 255.0f);
                else if (desc.Format == PixelFormat::R32Float)
                    value = (unsigned char)(std::pow(std::clamp(((const float*)data)[i], 0.0f, 1.0f), 64.0f) * 255.0f);

                expanded[i * 4 + 0] = expanded[i * 4 + 1] = expanded[i * 4 + 2] = value;
                expanded[i * 4 + 3] = 255;
            }
        }

        auto* pixels = expanded.empty() ? data : expanded.data();
        auto pixelFormat = (desc.Format == PixelFormat::BGRA8Unorm) ? SDL_PIXELFORMAT_BGRA32 : SDL_PIXELFORMAT_RGBA32;
        auto* surface = SDL_CreateSurfaceFrom(desc.Width, desc.Height, pixelFormat, (void*)pixels, desc.Width * 4);
        bool result = (surface != nullptr) && SDL_SavePNG(surface, path.c_str());
        SDL_DestroySurface(surface);
        return result;
    }

    bool SaveTextureToPng(IDevice& device, ITexture* texture, const std::string& path, int layer)
    {
        auto* commandBuffer = device.AcquireCommandBuffer();
        auto transferBuffer = RecordDownload(device, *commandBuffer->BeginCopyPass(), texture, layer);
        commandBuffer->EndCopyPass();
        commandBuffer->Submit();
        if (transferBuffer == nullptr)
            return false;

        device.WaitIdle();

        bool result = WritePng(texture->GetDesc(), (const unsigned char*)device.MapTransferBuffer(transferBuffer.get(), false), path);
        device.UnmapTransferBuffer(transferBuffer.get());
        return result;
    }

    bool TextureReadback::HasUnsubmittedRequests() const
    {
        for (const auto& request : _requests)
        {
            if (request.Fence == nullptr)
                return true;
        }

        return false;
    }

    void TextureReadback::Record(IDevice& device, ICopyPass& copyPass, ITexture* texture, const std::string& path)
    {
        auto request = Request{};
        request.Buffer = RecordDownload(device, copyPass, texture, 0);
        request.Desc = texture->GetDesc();
        request.Path = path;
        if (request.Buffer != nullptr)
            _requests.push_back(std::move(request));
    }

    void TextureReadback::Record(IDevice& device, ICopyPass& copyPass, ITexture* texture, ReadCallback callback)
    {
        auto request = Request{};
        request.Buffer = RecordDownload(device, copyPass, texture, 0);
        request.Desc = texture->GetDesc();
        request.Callback = std::move(callback);
        if (request.Buffer != nullptr)
            _requests.push_back(std::move(request));
    }

    // Takes a fence only when downloads were recorded since the last submission.
    void TextureReadback::Submit(ICommandBuffer& commandBuffer)
    {
        if (!HasUnsubmittedRequests())
        {
            commandBuffer.Submit();
            return;
        }

        SubmitWithFence(commandBuffer);
    }

    std::shared_ptr<IFence> TextureReadback::SubmitWithFence(ICommandBuffer& commandBuffer)
    {
        auto fence = std::shared_ptr<IFence>(commandBuffer.SubmitWithFence());
        for (auto& request : _requests)
        {
            if (request.Fence == nullptr)
                request.Fence = fence;
        }

        return fence;
    }

    void TextureReadback::Clear()
    {
        _requests.clear();
    }

    // With wait set, every submitted download is completed first, as at shutdown.
    void TextureReadback::Poll(IDevice& device, bool wait)
    {
        if (_requests.empty())
            return;

        if (wait)
            device.WaitIdle();

        for (int i = 0; i < (int)_requests.size(); )
        {
            auto& request = _requests[i];
            if (request.Fence == nullptr || !device.IsFenceSignaled(request.Fence.get()))
            {
                i++;
                continue;
            }

            const auto* data = (const unsigned char*)device.MapTransferBuffer(request.Buffer.get(), false);
            if (request.Callback != nullptr)
            {
                request.Callback(request.Desc, data);
                device.UnmapTransferBuffer(request.Buffer.get());
                _requests.erase(_requests.begin() + i);
                continue;
            }

            bool saved = WritePng(request.Desc, data, request.Path);
            device.UnmapTransferBuffer(request.Buffer.get());
            TENLog("Screenshot " + request.Path + (saved ? " saved." : " failed."), saved ? LogLevel::Info : LogLevel::Warning);

            _requests.erase(_requests.begin() + i);
        }
    }
}
