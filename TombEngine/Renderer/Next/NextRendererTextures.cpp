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
    constexpr auto MATERIAL_MAP_COUNT = 4;

    // Size, format and final mip count of one map of a level page; absent maps have zero width.
    struct ArrayMapKey
    {
        int         Width       = 0;
        int         Height      = 0;
        int         MipLevels   = 0;
        PixelFormat Format      = PixelFormat::Unknown;
        bool        IsGenerated = false; // Single level images get their mips generated on the GPU.
    };

    // Pages with the same key share the texture arrays of a group.
    struct ArrayGroupKey
    {
        ArrayMapKey Maps[MATERIAL_MAP_COUNT] = {};
    };

    static bool IsSameMapKey(const ArrayMapKey& first, const ArrayMapKey& second)
    {
        return (first.Width == second.Width && first.Height == second.Height && first.MipLevels == second.MipLevels &&
                first.Format == second.Format && first.IsGenerated == second.IsGenerated);
    }

    static bool IsSameGroupKey(const ArrayGroupKey& first, const ArrayGroupKey& second)
    {
        for (int i = 0; i < MATERIAL_MAP_COUNT; i++)
        {
            if (!IsSameMapKey(first.Maps[i], second.Maps[i]))
                return false;
        }

        return true;
    }

    static const std::vector<unsigned char>& GetMapData(const ::TEXTURE& texture, int map)
    {
        switch (map)
        {
        case 1:
            return texture.normalMapData;

        case 2:
            return texture.ORSHMapData;

        case 3:
            return texture.emissiveMapData;

        default:
            return texture.colorMapData;
        }
    }

    static ArrayGroupKey GetArrayGroupKey(const ::TEXTURE& texture, bool isColorOnly)
    {
        auto key = ArrayGroupKey{};
        for (int map = 0; map < (isColorOnly ? 1 : MATERIAL_MAP_COUNT); map++)
        {
            auto info = ImageInfo{};
            const auto& data = GetMapData(texture, map);
            if (data.empty() || !ReadImageInfo(data, info))
                continue;

            auto& mapKey = key.Maps[map];
            mapKey.Width = info.Width;
            mapKey.Height = info.Height;
            mapKey.Format = info.Format;
            mapKey.IsGenerated = (info.MipLevels == 1);
            mapKey.MipLevels = mapKey.IsGenerated ? GetMipLevelCount(info.Width, info.Height) : info.MipLevels;
        }

        return key;
    }

    void NextRenderer::AddTextureRequest(std::vector<LevelTextureRequest>& requests, const std::vector<unsigned char>& encodedData,
                                         const std::string& name, std::unique_ptr<ITexture>& target)
    {
        if (!encodedData.empty())
            requests.push_back(LevelTextureRequest{ &encodedData, name, &target });
    }

    // Uploads every mip of a decoded image into one layer of a texture or texture array.
    void NextRenderer::RecordImageUpload(ICopyPass& copyPass, const DecodedImage& image, ITexture* texture, int layer,
                                         std::vector<std::unique_ptr<ITransferBuffer>>& transferBuffers)
    {
        auto transferBuffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, (int)image.Data.size());
        if (transferBuffer == nullptr)
            return;

        memcpy(_device->MapTransferBuffer(transferBuffer.get(), false), image.Data.data(), image.Data.size());
        _device->UnmapTransferBuffer(transferBuffer.get());

        int offset = 0;
        for (int mip = 0; mip < image.MipLevels; mip++)
        {
            int mipWidth = std::max(1, image.Width >> mip);
            int mipHeight = std::max(1, image.Height >> mip);

            auto region = TextureRegion{};
            region.Texture = texture;
            region.MipLevel = mip;
            region.Layer = layer;
            region.Width = mipWidth;
            region.Height = mipHeight;
            copyPass.UploadToTexture(TextureTransferInfo{ transferBuffer.get(), offset, 0, 0 }, region, false);

            offset += GetTextureDataSize(image.Format, mipWidth, mipHeight, 1);
        }

        transferBuffers.push_back(std::move(transferBuffer));
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
        if (texture != nullptr)
            RecordImageUpload(copyPass, image, texture.get(), 0, transferBuffers);

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

    void NextRenderer::AddArrayPages(const std::vector<::TEXTURE>& source, std::vector<TexturePage>& destination, bool isColorOnly,
                                     std::vector<ArrayPageSource>& pages)
    {
        destination.assign(source.size(), TexturePage{});
        for (int i = 0; i < (int)source.size(); i++)
            pages.push_back(ArrayPageSource{ &source[i], isColorOnly, &destination[i] });
    }

    std::unique_ptr<NextRenderer::TextureArrayGroup> NextRenderer::CreateTextureArrayGroup(const ArrayGroupKey& key, int layerCount, int groupIndex)
    {
        constexpr const char* MAP_NAMES[MATERIAL_MAP_COUNT] = { "Color", "Normal", "ORSH", "Emissive" };

        auto group = std::make_unique<TextureArrayGroup>();
        for (int map = 0; map < MATERIAL_MAP_COUNT; map++)
        {
            const auto& mapKey = key.Maps[map];
            if (mapKey.Width == 0)
                continue;

            auto desc = TextureDesc{};
            desc.Type = TextureType::Texture2DArray;
            desc.Width = mapKey.Width;
            desc.Height = mapKey.Height;
            desc.DepthOrLayers = layerCount;
            desc.MipLevels = mapKey.MipLevels;
            desc.Format = mapKey.Format;
            desc.Usage = TextureUsage::Sampler | (mapKey.IsGenerated ? TextureUsage::ColorTarget : 0);
            desc.Name = "LevelPages[" + std::to_string(groupIndex) + "]." + MAP_NAMES[map];
            group->Maps[map] = _device->CreateTexture(desc);
        }

        return group;
    }

    // Level pages become layers of texture arrays: pages whose maps share size and format share one array per map,
    // whatever their category, so draws switching between them only change the layer uniform.
    void NextRenderer::LoadTextureArrays(const std::vector<ArrayPageSource>& pages)
    {
        _textureArrayGroups.clear();

        auto groupKeys = std::vector<ArrayGroupKey>{};
        auto layerCounts = std::vector<int>{};
        auto pageKeys = std::vector<ArrayGroupKey>(pages.size());
        auto pageGroups = std::vector<int>(pages.size());
        for (int i = 0; i < (int)pages.size(); i++)
        {
            pageKeys[i] = GetArrayGroupKey(*pages[i].Texture, pages[i].IsColorOnly);

            int group = 0;
            while (group < (int)groupKeys.size() && !IsSameGroupKey(groupKeys[group], pageKeys[i]))
                group++;

            if (group == (int)groupKeys.size())
            {
                groupKeys.push_back(pageKeys[i]);
                layerCounts.push_back(0);
            }

            pageGroups[i] = group;
            pages[i].Target->Layer = layerCounts[group]++;
        }

        for (int group = 0; group < (int)groupKeys.size(); group++)
            _textureArrayGroups.push_back(CreateTextureArrayGroup(groupKeys[group], layerCounts[group], group));

        for (int i = 0; i < (int)pages.size(); i++)
            pages[i].Target->Group = _textureArrayGroups[pageGroups[i]].get();

        UploadTextureArrayLayers(pages, pageKeys);

        TENLog(fmt::format("Next renderer: {} texture pages in {} texture array groups.", pages.size(), groupKeys.size()), LogLevel::Info);
    }

    // Maps are decoded in parallel chunks and uploaded to their layer; generated mips follow once all layers are filled.
    void NextRenderer::UploadTextureArrayLayers(const std::vector<ArrayPageSource>& pages, const std::vector<ArrayGroupKey>& pageKeys)
    {
        struct LayerUpload
        {
            int Page = 0;
            int Map  = 0;
        };

        auto uploads = std::vector<LayerUpload>{};
        for (int i = 0; i < (int)pages.size(); i++)
        {
            for (int map = 0; map < MATERIAL_MAP_COUNT; map++)
            {
                if (pageKeys[i].Maps[map].Width > 0 && pages[i].Target->Group->Maps[map] != nullptr)
                    uploads.push_back(LayerUpload{ i, map });
            }
        }

        for (int first = 0; first < (int)uploads.size(); first += TEXTURE_CHUNK_SIZE)
        {
            int count = std::min(TEXTURE_CHUNK_SIZE, (int)uploads.size() - first);

            DecodedImage images[TEXTURE_CHUNK_SIZE] = {};
            bool isDecoded[TEXTURE_CHUNK_SIZE] = {};
            g_Parallel.AddTasks(count, [&](int start, int end)
            {
                for (int i = start; i < end; i++)
                    isDecoded[i] = DecodeImage(GetMapData(*pages[uploads[first + i].Page].Texture, uploads[first + i].Map), images[i]);
            }).wait();

            auto transferBuffers = std::vector<std::unique_ptr<ITransferBuffer>>{};
            auto* commandBuffer = _device->AcquireCommandBuffer();
            auto* copyPass = commandBuffer->BeginCopyPass();
            for (int i = 0; i < count; i++)
            {
                const auto& upload = uploads[first + i];
                const auto& page = pages[upload.Page];
                auto* texture = page.Target->Group->Maps[upload.Map].get();
                const auto& desc = texture->GetDesc();
                if (!isDecoded[i] || images[i].Width != desc.Width || images[i].Height != desc.Height || images[i].Format != desc.Format)
                {
                    TENLog("Next renderer: texture page " + std::to_string(upload.Page) + " does not match its header.", LogLevel::Warning);
                    continue;
                }

                RecordImageUpload(*copyPass, images[i], texture, page.Target->Layer, transferBuffers);
            }

            commandBuffer->EndCopyPass();
            commandBuffer->Submit();
            _device->WaitIdle();
        }

        auto* commandBuffer = _device->AcquireCommandBuffer();
        for (const auto& group : _textureArrayGroups)
        {
            for (const auto& map : group->Maps)
            {
                if (map != nullptr && (map->GetDesc().Usage & TextureUsage::ColorTarget))
                    _device->GenerateMipmaps(commandBuffer, map.get());
            }
        }

        commandBuffer->Submit();
        _device->WaitIdle();
    }

    // Animated texture pages and the sky use their color map only, as in the legacy renderer. Sprite pages stay
    // single textures: the 2D layer samples them too.
    void NextRenderer::LoadLevelTextures()
    {
        auto pages = std::vector<ArrayPageSource>{};
        AddArrayPages(g_Level.RoomTextures, _roomTextures, false, pages);
        AddArrayPages(g_Level.StaticsTextures, _staticTextures, false, pages);
        AddArrayPages(g_Level.MoveablesTextures, _moveableTextures, false, pages);
        AddArrayPages(g_Level.AnimatedTextures, _animatedTextures, true, pages);

        _skyPage = {};
        if (!g_Level.SkyTexture.colorMapData.empty())
            pages.push_back(ArrayPageSource{ &g_Level.SkyTexture, true, &_skyPage });

        for (const auto& page : pages)
        {
            if (!page.IsColorOnly && !page.Texture->emissiveMapData.empty())
                _hasEmissiveMaps = true;
        }

        LoadTextureArrays(pages);

        auto requests = std::vector<LevelTextureRequest>{};
        _spriteTextures.clear();
        _spriteTextures.resize(g_Level.SpritesTextures.size());
        for (int i = 0; i < (int)g_Level.SpritesTextures.size(); i++)
            AddTextureRequest(requests, g_Level.SpritesTextures[i].colorMapData, "SpriteAtlas[" + std::to_string(i) + "].Color", _spriteTextures[i].ColorMap);

        CreateLevelTextures(requests, true);
    }
}
