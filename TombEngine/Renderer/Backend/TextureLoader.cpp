#include "framework.h"
#include "Renderer/Backend/TextureLoader.h"

// JPEG decoding only: PNG goes through SDL and DDS is read directly.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include <stb/stb_image.h>

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    constexpr auto DDS_MAGIC_SIZE  = 4;
    constexpr auto DDS_HEADER_SIZE = 124;
    constexpr auto DDS_DX10_SIZE   = 20;

    constexpr unsigned int MakeFourCC(char a, char b, char c, char d)
    {
        return (unsigned int)(unsigned char)a | ((unsigned int)(unsigned char)b << 8) |
               ((unsigned int)(unsigned char)c << 16) | ((unsigned int)(unsigned char)d << 24);
    }

    static unsigned int ReadUInt(const unsigned char* data, int offset)
    {
        unsigned int value = 0;
        memcpy(&value, data + offset, sizeof(value));
        return value;
    }

    static PixelFormat GetDXGIPixelFormat(unsigned int dxgiFormat)
    {
        switch (dxgiFormat)
        {
        case 28: return PixelFormat::RGBA8Unorm;
        case 87: return PixelFormat::BGRA8Unorm;
        case 71: return PixelFormat::BC1Unorm;
        case 74: return PixelFormat::BC2Unorm;
        case 77: return PixelFormat::BC3Unorm;
        case 80: return PixelFormat::BC4Unorm;
        case 83: return PixelFormat::BC5Unorm;
        case 98: return PixelFormat::BC7Unorm;
        default: return PixelFormat::Unknown;
        }
    }

    static PixelFormat GetDDSPixelFormat(const unsigned char* header, bool& hasDX10Header)
    {
        constexpr auto PIXEL_FORMAT_OFFSET = 72;
        constexpr auto DDPF_FOURCC = 0x4;
        constexpr auto DDPF_RGB    = 0x40;

        unsigned int flags = ReadUInt(header, PIXEL_FORMAT_OFFSET + 4);
        unsigned int fourCC = ReadUInt(header, PIXEL_FORMAT_OFFSET + 8);
        unsigned int bitCount = ReadUInt(header, PIXEL_FORMAT_OFFSET + 12);
        unsigned int redMask = ReadUInt(header, PIXEL_FORMAT_OFFSET + 16);

        hasDX10Header = false;

        if (flags & DDPF_FOURCC)
        {
            if (fourCC == MakeFourCC('D', 'X', 'T', '1'))
                return PixelFormat::BC1Unorm;

            if (fourCC == MakeFourCC('D', 'X', 'T', '3'))
                return PixelFormat::BC2Unorm;

            if (fourCC == MakeFourCC('D', 'X', 'T', '5'))
                return PixelFormat::BC3Unorm;

            if (fourCC == MakeFourCC('A', 'T', 'I', '1') || fourCC == MakeFourCC('B', 'C', '4', 'U'))
                return PixelFormat::BC4Unorm;

            if (fourCC == MakeFourCC('A', 'T', 'I', '2') || fourCC == MakeFourCC('B', 'C', '5', 'U'))
                return PixelFormat::BC5Unorm;

            if (fourCC == MakeFourCC('D', 'X', '1', '0'))
            {
                hasDX10Header = true;
                return PixelFormat::Unknown;
            }

            return PixelFormat::Unknown;
        }

        if ((flags & DDPF_RGB) && bitCount == 32)
            return (redMask == 0x000000FF) ? PixelFormat::RGBA8Unorm : PixelFormat::BGRA8Unorm;

        return PixelFormat::Unknown;
    }

    static int GetMipChainSize(const ImageInfo& info)
    {
        int size = 0;
        for (int mip = 0; mip < info.MipLevels; mip++)
            size += GetTextureDataSize(info.Format, std::max(1, info.Width >> mip), std::max(1, info.Height >> mip), 1);

        return size;
    }

    // BC3 alpha block with alpha 255 for every texel. Equal endpoints select the six value mode, where only index 6 is
    // transparent; other endpoints are left as not opaque, which keeps the check to a few bit operations.
    static bool IsOpaqueAlphaBlock(const unsigned char* block)
    {
        constexpr unsigned long long INDEX_LOW_BITS = 0x249249249249ull;

        if (block[0] != 255 || block[1] != 255)
            return false;

        unsigned long long indices = 0;
        memcpy(&indices, block + 2, 6);
        auto bit0 = indices & INDEX_LOW_BITS;
        auto bit1 = (indices >> 1) & INDEX_LOW_BITS;
        auto bit2 = (indices >> 2) & INDEX_LOW_BITS;
        return ((bit2 & bit1 & ~bit0) == 0);
    }

    static bool IsOpaqueBC3(const unsigned char* data, int size)
    {
        for (int offset = 0; (offset + 16) <= size; offset += 16)
        {
            if (!IsOpaqueAlphaBlock(data + offset))
                return false;
        }

        return true;
    }

    // The color half of a BC3 block is a BC1 block always decoded with four colors. BC1 uses four colors only when the
    // first endpoint is greater, so reversed endpoints are swapped with their indices, and equal ones keep index 0,
    // as the other indices would select the transparent color of BC1. The result decodes to the same colors.
    static void ConvertOpaqueBC3ToBC1(const unsigned char* source, int size, std::vector<unsigned char>& destination)
    {
        destination.resize(size / 2);
        for (int block = 0; (block * 16 + 16) <= size; block++)
        {
            const auto* color = source + (block * 16) + 8;
            unsigned short color0 = 0;
            unsigned short color1 = 0;
            unsigned int indices = 0;
            memcpy(&color0, color, 2);
            memcpy(&color1, color + 2, 2);
            memcpy(&indices, color + 4, 4);

            if (color0 < color1)
            {
                std::swap(color0, color1);
                indices ^= 0x55555555;
            }
            else if (color0 == color1)
            {
                indices = 0;
            }

            auto* output = destination.data() + (block * 8);
            memcpy(output, &color0, 2);
            memcpy(output + 2, &color1, 2);
            memcpy(output + 4, &indices, 4);
        }
    }

    // Returns the offset of the pixel data, or NO_VALUE for unsupported files.
    static int ReadDDSHeader(const std::vector<unsigned char>& encodedData, ImageInfo& info)
    {
        if (encodedData.size() < DDS_MAGIC_SIZE + DDS_HEADER_SIZE)
            return NO_VALUE;

        const auto* header = encodedData.data() + DDS_MAGIC_SIZE;
        info.Height = (int)ReadUInt(header, 8);
        info.Width = (int)ReadUInt(header, 12);
        info.MipLevels = std::max(1, (int)ReadUInt(header, 24));

        bool hasDX10Header = false;
        info.Format = GetDDSPixelFormat(header, hasDX10Header);

        int dataOffset = DDS_MAGIC_SIZE + DDS_HEADER_SIZE;
        if (hasDX10Header)
        {
            if ((int)encodedData.size() < dataOffset + DDS_DX10_SIZE)
                return NO_VALUE;

            info.Format = GetDXGIPixelFormat(ReadUInt(encodedData.data(), dataOffset));
            dataOffset += DDS_DX10_SIZE;
        }

        if (info.Format == PixelFormat::Unknown)
        {
            TENLog("Unsupported DDS pixel format in level texture.", LogLevel::Warning);
            return NO_VALUE;
        }

        return dataOffset;
    }

    // Level textures were compiled as BC3 also without transparency; those are loaded as BC1.
    static bool IsOpaqueBC3Image(const std::vector<unsigned char>& encodedData, int dataOffset, const ImageInfo& info)
    {
        int size = GetMipChainSize(info);
        return (info.Format == PixelFormat::BC3Unorm && (int)encodedData.size() >= (dataOffset + size) &&
                IsOpaqueBC3(encodedData.data() + dataOffset, size));
    }

    static bool DecodeDDS(const std::vector<unsigned char>& encodedData, DecodedImage& image)
    {
        auto info = ImageInfo{};
        int dataOffset = ReadDDSHeader(encodedData, info);
        if (dataOffset == NO_VALUE)
            return false;

        int totalSize = GetMipChainSize(info);
        if ((int)encodedData.size() < dataOffset + totalSize)
            return false;

        image.Width = info.Width;
        image.Height = info.Height;
        image.Format = info.Format;
        image.MipLevels = info.MipLevels;

        if (IsOpaqueBC3Image(encodedData, dataOffset, info))
        {
            ConvertOpaqueBC3ToBC1(encodedData.data() + dataOffset, totalSize, image.Data);
            image.Format = PixelFormat::BC1Unorm;
            return true;
        }

        image.Data.assign(encodedData.begin() + dataOffset, encodedData.begin() + dataOffset + totalSize);
        return true;
    }

    static bool DecodePNG(const std::vector<unsigned char>& encodedData, DecodedImage& image)
    {
        auto* stream = SDL_IOFromConstMem(encodedData.data(), encodedData.size());
        auto* surface = SDL_LoadPNG_IO(stream, true);
        if (surface == nullptr)
        {
            TENLog(std::string("Cannot decode level texture: ") + SDL_GetError(), LogLevel::Warning);
            return false;
        }

        auto* rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);
        if (rgbaSurface == nullptr)
            return false;

        image.Width = rgbaSurface->w;
        image.Height = rgbaSurface->h;
        image.Format = PixelFormat::RGBA8Unorm;
        image.MipLevels = 1;
        image.Data.resize(image.Width * image.Height * 4);

        for (int y = 0; y < image.Height; y++)
            memcpy(image.Data.data() + (y * image.Width * 4), (unsigned char*)rgbaSurface->pixels + (y * rgbaSurface->pitch), image.Width * 4);

        SDL_DestroySurface(rgbaSurface);
        return true;
    }

    static bool IsDDS(const std::vector<unsigned char>& encodedData)
    {
        return (encodedData.size() >= DDS_MAGIC_SIZE && memcmp(encodedData.data(), "DDS ", DDS_MAGIC_SIZE) == 0);
    }

    // JPEG files start with the start of image marker followed by another marker.
    static bool IsJPEG(const std::vector<unsigned char>& encodedData)
    {
        return (encodedData.size() >= 3 && encodedData[0] == 0xFF && encodedData[1] == 0xD8 && encodedData[2] == 0xFF);
    }

    static bool DecodeJPEG(const std::vector<unsigned char>& encodedData, DecodedImage& image)
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        auto* pixels = stbi_load_from_memory(encodedData.data(), (int)encodedData.size(), &width, &height, &channels, 4);
        if (pixels == nullptr)
        {
            TENLog(std::string("Cannot decode JPEG image: ") + stbi_failure_reason(), LogLevel::Warning);
            return false;
        }

        image.Width = width;
        image.Height = height;
        image.Format = PixelFormat::RGBA8Unorm;
        image.MipLevels = 1;
        image.Data.assign(pixels, pixels + (width * height * 4));
        stbi_image_free(pixels);
        return true;
    }

    // PNG: 8 byte signature, then the IHDR chunk with big endian width and height.
    bool ReadImageInfo(const std::vector<unsigned char>& encodedData, ImageInfo& info)
    {
        constexpr auto PNG_WIDTH_OFFSET = 16;
        constexpr auto PNG_HEADER_SIZE  = 24;

        if (IsDDS(encodedData))
        {
            int dataOffset = ReadDDSHeader(encodedData, info);
            if (dataOffset == NO_VALUE)
                return false;

            if (IsOpaqueBC3Image(encodedData, dataOffset, info))
                info.Format = PixelFormat::BC1Unorm;

            return true;
        }

        if (IsJPEG(encodedData))
        {
            int channels = 0;
            info.Format = PixelFormat::RGBA8Unorm;
            info.MipLevels = 1;
            return (stbi_info_from_memory(encodedData.data(), (int)encodedData.size(), &info.Width, &info.Height, &channels) != 0);
        }

        if (encodedData.size() < PNG_HEADER_SIZE)
            return false;

        auto readBigEndian = [&](int offset)
        {
            const auto* bytes = encodedData.data() + offset;
            return (int)(((unsigned int)bytes[0] << 24) | ((unsigned int)bytes[1] << 16) | ((unsigned int)bytes[2] << 8) | bytes[3]);
        };

        info.Width = readBigEndian(PNG_WIDTH_OFFSET);
        info.Height = readBigEndian(PNG_WIDTH_OFFSET + 4);
        info.Format = PixelFormat::RGBA8Unorm;
        info.MipLevels = 1;
        return (info.Width > 0 && info.Height > 0);
    }

    bool DecodeImage(const std::vector<unsigned char>& encodedData, DecodedImage& image)
    {
        if (IsDDS(encodedData))
            return DecodeDDS(encodedData, image);

        if (IsJPEG(encodedData))
            return DecodeJPEG(encodedData, image);

        return DecodePNG(encodedData, image);
    }

    int GetMipLevelCount(int width, int height)
    {
        int levels = 1;
        int size = std::max(width, height);
        while (size > 1)
        {
            size >>= 1;
            levels++;
        }

        return levels;
    }
}
