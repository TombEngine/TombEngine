#include "framework.h"
#include "Renderer/Backend/TextureCompressor.h"

#define STB_DXT_IMPLEMENTATION
#include <stb/stb_dxt.h>

#include "Renderer/Backend/TextureLoader.h"
#include "Specific/level.h"
#include "Specific/Parallel.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    // Part of the cache file names: changing the encoder or its settings must change it, so stale files are not read.
    constexpr auto CACHE_VERSION = "bc1";

    constexpr auto BLOCK_SIZE      = 4;
    constexpr auto DDS_HEADER_SIZE = 124;

    struct CompressionJob
    {
        std::vector<unsigned char>* Data       = nullptr;
        bool                        IsNormal   = false;
        bool                        IsFromCache = false;
    };

    static bool IsBlockCompressed(const std::vector<unsigned char>& data)
    {
        return (data.size() >= 4 && memcmp(data.data(), "DDS ", 4) == 0);
    }

    // FNV-1a over the source image, so the cache follows changes of the level textures.
    static unsigned long long GetDataHash(const std::vector<unsigned char>& data)
    {
        unsigned long long hash = 14695981039346656037ull;
        for (auto byte : data)
        {
            hash ^= byte;
            hash *= 1099511628211ull;
        }

        return hash;
    }

    // 2x2 box filter; an odd edge repeats its last texel.
    static std::vector<unsigned char> GetNextMip(const std::vector<unsigned char>& source, int width, int height, int nextWidth, int nextHeight)
    {
        auto result = std::vector<unsigned char>(nextWidth * nextHeight * 4);
        for (int y = 0; y < nextHeight; y++)
        {
            int y0 = std::min(y * 2, height - 1);
            int y1 = std::min((y * 2) + 1, height - 1);
            for (int x = 0; x < nextWidth; x++)
            {
                int x0 = std::min(x * 2, width - 1);
                int x1 = std::min((x * 2) + 1, width - 1);
                for (int channel = 0; channel < 4; channel++)
                {
                    int sum = source[((y0 * width) + x0) * 4 + channel] + source[((y0 * width) + x1) * 4 + channel] +
                              source[((y1 * width) + x0) * 4 + channel] + source[((y1 * width) + x1) * 4 + channel];
                    result[((y * nextWidth) + x) * 4 + channel] = (unsigned char)((sum + 2) / 4);
                }
            }
        }

        return result;
    }

    static bool IsOpaque(const std::vector<unsigned char>& pixels)
    {
        for (int i = 3; i < (int)pixels.size(); i += 4)
        {
            if (pixels[i] != 255)
                return false;
        }

        return true;
    }

    // Blocks past the image edge repeat its last row and column.
    static void CompressMip(const std::vector<unsigned char>& pixels, int width, int height, PixelFormat format, std::vector<unsigned char>& output)
    {
        int blocksX = (width + BLOCK_SIZE - 1) / BLOCK_SIZE;
        int blocksY = (height + BLOCK_SIZE - 1) / BLOCK_SIZE;
        int blockBytes = GetFormatBlockSize(format);

        for (int blockY = 0; blockY < blocksY; blockY++)
        {
            for (int blockX = 0; blockX < blocksX; blockX++)
            {
                unsigned char rgba[BLOCK_SIZE * BLOCK_SIZE * 4] = {};
                unsigned char rg[BLOCK_SIZE * BLOCK_SIZE * 2] = {};
                for (int y = 0; y < BLOCK_SIZE; y++)
                {
                    for (int x = 0; x < BLOCK_SIZE; x++)
                    {
                        int sourceX = std::min((blockX * BLOCK_SIZE) + x, width - 1);
                        int sourceY = std::min((blockY * BLOCK_SIZE) + y, height - 1);
                        const auto* texel = &pixels[((sourceY * width) + sourceX) * 4];
                        int index = (y * BLOCK_SIZE) + x;
                        memcpy(&rgba[index * 4], texel, 4);
                        rg[index * 2] = texel[0];
                        rg[(index * 2) + 1] = texel[1];
                    }
                }

                unsigned char block[16] = {};
                if (format == PixelFormat::BC5Unorm)
                    stb_compress_bc5_block(block, rg);
                else
                    stb_compress_dxt_block(block, rgba, (format == PixelFormat::BC3Unorm) ? 1 : 0, STB_DXT_HIGHQUAL);

                output.insert(output.end(), block, block + blockBytes);
            }
        }
    }

    static void WriteUInt(std::vector<unsigned char>& data, int offset, unsigned int value)
    {
        memcpy(data.data() + offset, &value, sizeof(value));
    }

    // Legacy DDS header with a FourCC code, as read by TextureLoader.
    static std::vector<unsigned char> GetDDSHeader(int width, int height, int mipLevels, PixelFormat format)
    {
        constexpr auto FLAGS = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000; // Caps, height, width, pixel format, mip count, linear size.
        constexpr auto CAPS  = 0x8 | 0x1000 | 0x400000;                       // Complex, texture, mipmap.
        constexpr auto PIXEL_FORMAT_FOURCC = 0x4;

        const char* fourCC = (format == PixelFormat::BC1Unorm) ? "DXT1" : ((format == PixelFormat::BC3Unorm) ? "DXT5" : "ATI2");

        auto data = std::vector<unsigned char>(4 + DDS_HEADER_SIZE);
        memcpy(data.data(), "DDS ", 4);
        WriteUInt(data, 4, DDS_HEADER_SIZE);
        WriteUInt(data, 8, FLAGS);
        WriteUInt(data, 12, height);
        WriteUInt(data, 16, width);
        WriteUInt(data, 20, GetTextureDataSize(format, width, height, 1));
        WriteUInt(data, 28, mipLevels);
        WriteUInt(data, 76, 32);
        WriteUInt(data, 80, PIXEL_FORMAT_FOURCC);
        memcpy(data.data() + 84, fourCC, 4);
        WriteUInt(data, 108, CAPS);
        return data;
    }

    static bool EncodeImage(const std::vector<unsigned char>& source, bool isNormal, std::vector<unsigned char>& result)
    {
        auto image = DecodedImage{};
        if (!DecodeImage(source, image) || image.Format != PixelFormat::RGBA8Unorm)
            return false;

        auto format = isNormal ? PixelFormat::BC5Unorm : (IsOpaque(image.Data) ? PixelFormat::BC1Unorm : PixelFormat::BC3Unorm);
        int mipLevels = GetMipLevelCount(image.Width, image.Height);
        result = GetDDSHeader(image.Width, image.Height, mipLevels, format);

        int width = image.Width;
        int height = image.Height;
        auto pixels = std::move(image.Data);
        for (int mip = 0; mip < mipLevels; mip++)
        {
            CompressMip(pixels, width, height, format, result);
            if (mip == (mipLevels - 1))
                break;

            int nextWidth = std::max(width / 2, 1);
            int nextHeight = std::max(height / 2, 1);
            pixels = GetNextMip(pixels, width, height, nextWidth, nextHeight);
            width = nextWidth;
            height = nextHeight;
        }

        return true;
    }

    static bool ReadCacheFile(const std::filesystem::path& path, std::vector<unsigned char>& data)
    {
        auto file = std::ifstream(path, std::ios::binary | std::ios::ate);
        if (!file.is_open())
            return false;

        auto size = (int)file.tellg();
        if (size <= (4 + DDS_HEADER_SIZE))
            return false;

        data.resize(size);
        file.seekg(0);
        file.read((char*)data.data(), size);
        return file.good();
    }

    // Written to a temporary name first, so an interrupted write never leaves a truncated file under the final name.
    static void WriteCacheFile(const std::filesystem::path& path, const std::vector<unsigned char>& data)
    {
        auto temporaryPath = path;
        temporaryPath += ".tmp";

        {
            auto file = std::ofstream(temporaryPath, std::ios::binary | std::ios::trunc);
            if (!file.is_open())
                return;

            file.write((const char*)data.data(), data.size());
            if (!file.good())
                return;
        }

        auto error = std::error_code{};
        std::filesystem::rename(temporaryPath, path, error);
        if (error)
            std::filesystem::remove(temporaryPath, error);
    }

    static void CompressJob(CompressionJob& job, const std::filesystem::path& directory)
    {
        auto& data = *job.Data;
        auto name = fmt::format("{:016x}-{}-{}{}.dds", GetDataHash(data), data.size(), CACHE_VERSION, job.IsNormal ? "-n" : "");
        auto path = directory / name;

        auto result = std::vector<unsigned char>{};
        if (ReadCacheFile(path, result))
        {
            job.IsFromCache = true;
            data.swap(result);
            return;
        }

        if (!EncodeImage(data, job.IsNormal, result))
            return;

        WriteCacheFile(path, result);
        data.swap(result);
    }

    static void AddJob(std::vector<CompressionJob>& jobs, std::vector<unsigned char>& data, bool isNormal)
    {
        if (!data.empty() && !IsBlockCompressed(data))
            jobs.push_back(CompressionJob{ &data, isNormal });
    }

    void CompressLevelTextures(const std::vector<TEXTURE*>& textures, const std::string& cacheDirectory)
    {
        auto jobs = std::vector<CompressionJob>{};
        for (auto* texture : textures)
        {
            AddJob(jobs, texture->colorMapData, false);
            AddJob(jobs, texture->normalMapData, true);
            AddJob(jobs, texture->ORSHMapData, false);
            AddJob(jobs, texture->emissiveMapData, false);
        }

        if (jobs.empty())
            return;

        auto directory = std::filesystem::path(cacheDirectory);
        auto error = std::error_code{};
        std::filesystem::create_directories(directory, error);

        auto startTime = std::chrono::steady_clock::now();
        g_Parallel.AddTasks((int)jobs.size(), [&](int start, int end)
        {
            for (int i = start; i < end; i++)
                CompressJob(jobs[i], directory);
        }).wait();

        int cachedCount = (int)std::count_if(jobs.begin(), jobs.end(), [](const CompressionJob& job) { return job.IsFromCache; });
        float elapsed = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();
        TENLog(fmt::format("Scene renderer: {} PNG or JPEG texture maps block compressed in {:.1f} ms ({} from the cache in {}).",
            jobs.size(), elapsed, cachedCount, cacheDirectory), LogLevel::Info);
    }
}
