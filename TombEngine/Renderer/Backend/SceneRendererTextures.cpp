#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Backend/TextureLoader.h"
#include "Renderer/RHI/MemoryTracker.h"
#include "Specific/level.h"
#include "Specific/Parallel.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Utils;

namespace TEN::Renderer::Backend
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
        int         SkippedMips = 0;     // Top mips left out to fit the video memory; not part of the grouping.
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

    // With texture compression on, Tomb Editor writes level textures as DDS files with full mip chains: BC1 for opaque
    // maps, BC3 for maps with alpha, BC5 for normal maps. Without it, maps are PNG files, loaded as RGBA8 with mips
    // generated on the GPU, at four to eight times the video memory. The engine never compresses or converts them.
    static void LogUncompressedPages(const std::vector<ArrayGroupKey>& pageKeys)
    {
        int count = 0;
        for (const auto& key : pageKeys)
        {
            for (const auto& map : key.Maps)
            {
                if (map.Width > 0 && (map.Format == PixelFormat::RGBA8Unorm || map.Format == PixelFormat::BGRA8Unorm))
                    count++;
            }
        }

        if (count == 0)
            return;

        TENLog(std::to_string(count) + " level texture maps are not block compressed (texture compression is off in Tomb Editor).", LogLevel::Info);
    }

    static TextureDesc GetArrayMapDesc(const ArrayMapKey& key, int layerCount)
    {
        auto desc = TextureDesc{};
        desc.Type = TextureType::Texture2DArray;
        desc.Width = std::max(key.Width >> key.SkippedMips, 1);
        desc.Height = std::max(key.Height >> key.SkippedMips, 1);
        desc.DepthOrLayers = layerCount;
        desc.MipLevels = key.MipLevels - key.SkippedMips;
        desc.Format = key.Format;
        desc.Usage = TextureUsage::Sampler | (key.IsGenerated ? TextureUsage::ColorTarget : 0);
        return desc;
    }

    // Level textures larger than the free video memory of a discrete GPU are placed by Vulkan in system memory, and
    // paged by DX11, both costly to sample. Top mips are left out instead, emissive maps first, then ORSH, normal and
    // color maps, one level per map type in turn. Integrated GPUs share system memory and keep every mip.
    static void FitArrayGroupsToBudget(std::vector<ArrayGroupKey>& groupKeys, const std::vector<int>& layerCounts, const MemoryStats& stats)
    {
        constexpr auto DISCRETE_MEMORY_MIN = 1024ull * 1024 * 1024;
        constexpr auto RESERVED_MEMORY     = 512ull * 1024 * 1024; // Frame buffers, pooled targets and later allocations.
        constexpr auto SKIPPED_MIPS_MAX    = 2;
        constexpr int  MAP_ORDER[MATERIAL_MAP_COUNT] = { 3, 2, 1, 0 };

        const auto& adapter = stats.Adapter;
        if (adapter.DedicatedVideoMemory < DISCRETE_MEMORY_MIN)
            return;

        auto limit = (adapter.ProcessBudget > 0) ? std::min(adapter.ProcessBudget, adapter.DedicatedVideoMemory) : adapter.DedicatedVideoMemory;

        // Memory held by other processes, such as browsers and the compositor, is rarely evicted for the game.
        auto otherUsage = (adapter.AdapterUsage > adapter.ProcessUsage) ? (adapter.AdapterUsage - adapter.ProcessUsage) : 0ull;
        if (otherUsage < adapter.DedicatedVideoMemory)
            limit = std::min(limit, adapter.DedicatedVideoMemory - otherUsage);

        // The usage the system reports includes the driver and backend allocations the tracked totals miss.
        auto used = std::max(adapter.ProcessUsage, stats.GetTotal()) + std::max(RESERVED_MEMORY, limit / 10);
        auto available = (limit > used) ? (limit - used) : 0ull;

        auto getTotalSize = [&]()
        {
            auto total = 0ull;
            for (int group = 0; group < (int)groupKeys.size(); group++)
            {
                for (const auto& map : groupKeys[group].Maps)
                {
                    if (map.Width > 0)
                        total += GetTextureMemorySize(GetArrayMapDesc(map, layerCounts[group]));
                }
            }

            return total;
        };

        auto originalSize = getTotalSize();
        auto size = originalSize;
        for (int skip = 1; skip <= SKIPPED_MIPS_MAX && size > available; skip++)
        {
            for (int i = 0; i < MATERIAL_MAP_COUNT && size > available; i++)
            {
                for (auto& key : groupKeys)
                {
                    auto& map = key.Maps[MAP_ORDER[i]];
                    if (map.Width > 0 && !map.IsGenerated && map.MipLevels > skip)
                        map.SkippedMips = skip;
                }

                size = getTotalSize();
            }
        }

        if (size != originalSize)
        {
            TENLog(fmt::format("Scene renderer: level textures reduced from {:.1f} MB to {:.1f} MB to fit {:.1f} MB of free video memory.",
                originalSize / (1024.0 * 1024.0), size / (1024.0 * 1024.0), available / (1024.0 * 1024.0)), LogLevel::Warning);
        }
    }

    void SceneRenderer::AddTextureRequest(std::vector<LevelTextureRequest>& requests, const std::vector<unsigned char>& encodedData,
                                         const std::string& name, std::unique_ptr<ITexture>& target)
    {
        if (!encodedData.empty())
            requests.push_back(LevelTextureRequest{ &encodedData, name, &target });
    }

    // Uploads every mip of a decoded image, but the skipped top ones, into one layer of a texture or texture array.
    void SceneRenderer::RecordImageUpload(ICopyPass& copyPass, const DecodedImage& image, ITexture* texture, int layer,
                                         std::vector<std::unique_ptr<ITransferBuffer>>& transferBuffers, int skippedMips)
    {
        int skippedSize = 0;
        for (int mip = 0; mip < skippedMips; mip++)
            skippedSize += GetTextureDataSize(image.Format, std::max(1, image.Width >> mip), std::max(1, image.Height >> mip), 1);

        int size = (int)image.Data.size() - skippedSize;
        if (size <= 0)
            return;

        auto transferBuffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, size);
        if (transferBuffer == nullptr)
            return;

        memcpy(_device->MapTransferBuffer(transferBuffer.get(), false), image.Data.data() + skippedSize, size);
        _device->UnmapTransferBuffer(transferBuffer.get());

        int offset = 0;
        for (int mip = skippedMips; mip < image.MipLevels; mip++)
        {
            int mipWidth = std::max(1, image.Width >> mip);
            int mipHeight = std::max(1, image.Height >> mip);

            auto region = TextureRegion{};
            region.Texture = texture;
            region.MipLevel = mip - skippedMips;
            region.Layer = layer;
            region.Width = mipWidth;
            region.Height = mipHeight;
            copyPass.UploadToTexture(TextureTransferInfo{ transferBuffer.get(), offset, 0, 0 }, region, false);

            offset += GetTextureDataSize(image.Format, mipWidth, mipHeight, 1);
        }

        transferBuffers.push_back(std::move(transferBuffer));
    }

    // Single-level images get their mip chain generated on the GPU after the copy pass, which needs a color target.
    std::unique_ptr<ITexture> SceneRenderer::RecordTextureUpload(ICopyPass& copyPass, const DecodedImage& image, const std::string& name, bool allowMips,
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
    void SceneRenderer::CreateLevelTextures(const std::vector<LevelTextureRequest>& requests, bool allowMips)
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

    std::unique_ptr<ITexture> SceneRenderer::CreateLevelTexture(const std::vector<unsigned char>& encodedData, const std::string& name, bool allowMips)
    {
        auto texture = std::unique_ptr<ITexture>{};
        auto requests = std::vector<LevelTextureRequest>{};
        AddTextureRequest(requests, encodedData, name, texture);
        CreateLevelTextures(requests, allowMips);
        return texture;
    }

    void SceneRenderer::AddArrayPages(const std::vector<::TEXTURE>& source, std::vector<TexturePage>& destination, bool isColorOnly,
                                     std::vector<ArrayPageSource>& pages)
    {
        destination.assign(source.size(), TexturePage{});
        for (int i = 0; i < (int)source.size(); i++)
            pages.push_back(ArrayPageSource{ &source[i], isColorOnly, &destination[i] });
    }

    std::unique_ptr<SceneRenderer::TextureArrayGroup> SceneRenderer::CreateTextureArrayGroup(const ArrayGroupKey& key, int layerCount, int groupIndex)
    {
        constexpr const char* MAP_NAMES[MATERIAL_MAP_COUNT] = { "Color", "Normal", "ORSH", "Emissive" };

        auto group = std::make_unique<TextureArrayGroup>();
        for (int map = 0; map < MATERIAL_MAP_COUNT; map++)
        {
            const auto& mapKey = key.Maps[map];
            if (mapKey.Width == 0)
                continue;

            auto desc = GetArrayMapDesc(mapKey, layerCount);
            desc.Name = "LevelPages[" + std::to_string(groupIndex) + "]." + MAP_NAMES[map];
            group->Maps[map] = _device->CreateTexture(desc);
            group->SkippedMips[map] = mapKey.SkippedMips;

            TENLog(fmt::format("Scene renderer: {} {}x{}, format {}, {} mips{}, {} layers, {:.1f} MB.", desc.Name, desc.Width, desc.Height,
                (int)desc.Format, desc.MipLevels, mapKey.IsGenerated ? " (generated)" : (mapKey.SkippedMips > 0 ? " (reduced)" : ""), layerCount,
                GetTextureMemorySize(desc) / (1024.0 * 1024.0)), LogLevel::Info);
        }

        group->Index = groupIndex;

        return group;
    }

    // Level pages become layers of texture arrays: pages whose maps share size and format share one array per map,
    // whatever their category, so draws switching between them only change the layer uniform.
    void SceneRenderer::LoadTextureArrays(const std::vector<ArrayPageSource>& pages)
    {
        _textureArrayGroups.clear();

        auto groupKeys = std::vector<ArrayGroupKey>{};
        auto layerCounts = std::vector<int>{};
        auto pageKeys = std::vector<ArrayGroupKey>(pages.size());
        auto pageGroups = std::vector<int>(pages.size());

        for (int i = 0; i < (int)pages.size(); i++)
            pageKeys[i] = GetArrayGroupKey(*pages[i].Texture, pages[i].IsColorOnly);

        LogUncompressedPages(pageKeys);

        for (int i = 0; i < (int)pages.size(); i++)
        {
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

        // Waiting completes the release of the previous level textures, which the measured usage must not include.
        _device->WaitIdle();
        FitArrayGroupsToBudget(groupKeys, layerCounts, GetMemoryStats(_device->GetDeviceName()));

        for (int group = 0; group < (int)groupKeys.size(); group++)
            _textureArrayGroups.push_back(CreateTextureArrayGroup(groupKeys[group], layerCounts[group], group));

        for (int i = 0; i < (int)pages.size(); i++)
            pages[i].Target->Group = _textureArrayGroups[pageGroups[i]].get();

        UploadTextureArrayLayers(pages, pageKeys);

        TENLog(fmt::format("Scene renderer: {} texture pages in {} texture array groups.", pages.size(), groupKeys.size()), LogLevel::Info);
    }

    // Maps are decoded in parallel chunks and uploaded to their layer; generated mips follow once all layers are filled.
    void SceneRenderer::UploadTextureArrayLayers(const std::vector<ArrayPageSource>& pages, const std::vector<ArrayGroupKey>& pageKeys)
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
                const auto& key = pageKeys[upload.Page].Maps[upload.Map];
                if (!isDecoded[i] || images[i].Width != key.Width || images[i].Height != key.Height || images[i].Format != key.Format)
                {
                    TENLog("Scene renderer: texture page " + std::to_string(upload.Page) + " does not match its header.", LogLevel::Warning);
                    continue;
                }

                RecordImageUpload(*copyPass, images[i], texture, page.Target->Layer, transferBuffers, page.Target->Group->SkippedMips[upload.Map]);
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

    // Animated texture pages and the sky use their color map only. Sprite pages stay
    // single textures: the 2D layer samples them too.
    void SceneRenderer::LoadLevelTextures()
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
        FreeEncodedLevelTextures();
    }

    // The encoded maps are not read after the upload; releasing them saves system memory of the size of the textures.
    void SceneRenderer::FreeEncodedLevelTextures()
    {
        auto freeMaps = [](::TEXTURE& texture)
        {
            std::vector<unsigned char>().swap(texture.colorMapData);
            std::vector<unsigned char>().swap(texture.normalMapData);
            std::vector<unsigned char>().swap(texture.ORSHMapData);
            std::vector<unsigned char>().swap(texture.emissiveMapData);
        };

        for (auto* textures : { &g_Level.RoomTextures, &g_Level.StaticsTextures, &g_Level.MoveablesTextures, &g_Level.AnimatedTextures, &g_Level.SpritesTextures })
        {
            for (auto& texture : *textures)
                freeMaps(texture);
        }

        freeMaps(g_Level.SkyTexture);
    }
}
