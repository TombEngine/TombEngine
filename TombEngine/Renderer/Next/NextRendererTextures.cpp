#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Next/TextureLoader.h"
#include "Specific/level.h"
#include "Specific/Parallel.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Utils;

namespace TEN::Renderer::Next
{
    // Textures decoded and uploaded together. Chunks bound the memory of decoded pixels and transfer buffers.
    constexpr auto TEXTURE_CHUNK_SIZE = 8;

    void NextRenderer::AddTextureRequest(std::vector<LevelTextureRequest>& requests, const std::vector<unsigned char>& encodedData,
                                         const std::string& name, std::unique_ptr<ITexture>& target)
    {
        if (!encodedData.empty())
            requests.push_back(LevelTextureRequest{ &encodedData, name, &target });
    }

    // Single-level images get their mip chain generated on the GPU after the copy pass, which needs a color target.
    std::unique_ptr<ITexture> NextRenderer::RecordTextureUpload(ICopyPass& copyPass, const DecodedImage& image, const std::string& name, bool allowMips,
                                                                std::vector<std::unique_ptr<ITransferBuffer>>& transferBuffers)
    {
        bool generateMips = (image.MipLevels == 1 && allowMips);

        auto desc = TextureDesc{};
        desc.Width = image.Width;
        desc.Height = image.Height;
        desc.Format = image.Format;
        desc.MipLevels = generateMips ? GetMipLevelCount(image.Width, image.Height) : image.MipLevels;
        desc.Usage = TextureUsage::Sampler | (generateMips ? TextureUsage::ColorTarget : 0);
        desc.Name = name;

        auto texture = _device->CreateTexture(desc);
        auto transferBuffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, (int)image.Data.size());
        if (texture == nullptr || transferBuffer == nullptr)
            return nullptr;

        memcpy(_device->MapTransferBuffer(transferBuffer.get(), false), image.Data.data(), image.Data.size());
        _device->UnmapTransferBuffer(transferBuffer.get());

        int offset = 0;
        for (int mip = 0; mip < image.MipLevels; mip++)
        {
            int mipWidth = std::max(1, image.Width >> mip);
            int mipHeight = std::max(1, image.Height >> mip);

            auto region = TextureRegion{};
            region.Texture = texture.get();
            region.MipLevel = mip;
            region.Width = mipWidth;
            region.Height = mipHeight;
            copyPass.UploadToTexture(TextureTransferInfo{ transferBuffer.get(), offset, 0, 0 }, region, false);

            offset += GetTextureDataSize(image.Format, mipWidth, mipHeight, 1);
        }

        transferBuffers.push_back(std::move(transferBuffer));
        return texture;
    }

    // Each chunk is decoded in parallel, then uploaded in one command buffer with a single wait.
    void NextRenderer::CreateLevelTextures(const std::vector<LevelTextureRequest>& requests, bool allowMips)
    {
        for (int first = 0; first < (int)requests.size(); first += TEXTURE_CHUNK_SIZE)
        {
            int count = std::min(TEXTURE_CHUNK_SIZE, (int)requests.size() - first);

            DecodedImage images[TEXTURE_CHUNK_SIZE] = {};
            bool isDecoded[TEXTURE_CHUNK_SIZE] = {};
            g_Parallel.AddTasks(count, [&](int start, int end)
            {
                for (int i = start; i < end; i++)
                    isDecoded[i] = DecodeImage(*requests[first + i].EncodedData, images[i]);
            }).wait();

            auto transferBuffers = std::vector<std::unique_ptr<ITransferBuffer>>{};
            auto* commandBuffer = _device->AcquireCommandBuffer();
            auto* copyPass = commandBuffer->BeginCopyPass();
            for (int i = 0; i < count; i++)
            {
                if (isDecoded[i])
                    *requests[first + i].Target = RecordTextureUpload(*copyPass, images[i], requests[first + i].Name, allowMips, transferBuffers);
            }

            commandBuffer->EndCopyPass();

            for (int i = 0; i < count; i++)
            {
                auto* texture = requests[first + i].Target->get();
                if (texture != nullptr && texture->GetDesc().MipLevels > images[i].MipLevels)
                    _device->GenerateMipmaps(commandBuffer, texture);
            }

            commandBuffer->Submit();
            _device->WaitIdle();
        }
    }

    std::unique_ptr<ITexture> NextRenderer::CreateLevelTexture(const std::vector<unsigned char>& encodedData, const std::string& name, bool allowMips)
    {
        auto texture = std::unique_ptr<ITexture>{};
        auto requests = std::vector<LevelTextureRequest>{};
        AddTextureRequest(requests, encodedData, name, texture);
        CreateLevelTextures(requests, allowMips);
        return texture;
    }

    // Animated texture pages use their color map only, as in the legacy renderer.
    void NextRenderer::AddTextureSetRequests(const std::vector<::TEXTURE>& source, std::vector<RoomTexture>& destination, const std::string& name,
                                             bool isColorOnly, std::vector<LevelTextureRequest>& requests)
    {
        destination.clear();
        destination.resize(source.size());
        for (int i = 0; i < (int)source.size(); i++)
        {
            auto pageName = name + "[" + std::to_string(i) + "]";
            AddTextureRequest(requests, source[i].colorMapData, pageName + ".Color", destination[i].ColorMap);
            if (isColorOnly)
                continue;

            AddTextureRequest(requests, source[i].normalMapData, pageName + ".Normal", destination[i].NormalMap);
            AddTextureRequest(requests, source[i].ORSHMapData, pageName + ".ORSH", destination[i].ORSHMap);
            AddTextureRequest(requests, source[i].emissiveMapData, pageName + ".Emissive", destination[i].EmissiveMap);

            if (!source[i].emissiveMapData.empty())
                _hasEmissiveMaps = true;
        }
    }

    // Every level texture page, decoded and uploaded in parallel chunks.
    void NextRenderer::LoadLevelTextures()
    {
        _skyTexture = nullptr;

        auto requests = std::vector<LevelTextureRequest>{};
        AddTextureSetRequests(g_Level.RoomTextures, _roomTextures, "RoomAtlas", false, requests);
        AddTextureSetRequests(g_Level.StaticsTextures, _staticTextures, "StaticAtlas", false, requests);
        AddTextureSetRequests(g_Level.MoveablesTextures, _moveableTextures, "MoveableAtlas", false, requests);
        AddTextureSetRequests(g_Level.SpritesTextures, _spriteTextures, "SpriteAtlas", false, requests);
        AddTextureSetRequests(g_Level.AnimatedTextures, _animatedTextures, "AnimatedAtlas", true, requests);
        AddTextureRequest(requests, g_Level.SkyTexture.colorMapData, "Sky", _skyTexture);
        CreateLevelTextures(requests, true);
    }
}
