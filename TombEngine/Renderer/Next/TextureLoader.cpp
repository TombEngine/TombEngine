#include "framework.h"
#include "Renderer/Next/TextureLoader.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Next
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

    static bool DecodeDDS(const std::vector<unsigned char>& encodedData, DecodedImage& image)
    {
        if (encodedData.size() < DDS_MAGIC_SIZE + DDS_HEADER_SIZE)
            return false;

        const auto* header = encodedData.data() + DDS_MAGIC_SIZE;
        image.Height = (int)ReadUInt(header, 8);
        image.Width = (int)ReadUInt(header, 12);
        image.MipLevels = std::max(1, (int)ReadUInt(header, 24));

        bool hasDX10Header = false;
        image.Format = GetDDSPixelFormat(header, hasDX10Header);

        int dataOffset = DDS_MAGIC_SIZE + DDS_HEADER_SIZE;
        if (hasDX10Header)
        {
            if ((int)encodedData.size() < dataOffset + DDS_DX10_SIZE)
                return false;

            image.Format = GetDXGIPixelFormat(ReadUInt(encodedData.data(), dataOffset));
            dataOffset += DDS_DX10_SIZE;
        }

        if (image.Format == PixelFormat::Unknown)
        {
            TENLog("Unsupported DDS pixel format in level texture.", LogLevel::Warning);
            return false;
        }

        int totalSize = 0;
        for (int mip = 0; mip < image.MipLevels; mip++)
            totalSize += GetTextureDataSize(image.Format, std::max(1, image.Width >> mip), std::max(1, image.Height >> mip), 1);

        if ((int)encodedData.size() < dataOffset + totalSize)
            return false;

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

    bool DecodeImage(const std::vector<unsigned char>& encodedData, DecodedImage& image)
    {
        if (encodedData.size() >= DDS_MAGIC_SIZE && memcmp(encodedData.data(), "DDS ", DDS_MAGIC_SIZE) == 0)
            return DecodeDDS(encodedData, image);

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
