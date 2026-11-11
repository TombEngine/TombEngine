#include "framework.h"
#include "Renderer/Graphics/ImageDecoding.h"

#include <stb/stb_image.h>

namespace TEN::Renderer::Graphics
{
	constexpr auto DDS_HEADER_SIZE	  = 128;
	constexpr auto DDS_FOURCC_OFFSET  = 84;
	constexpr auto PNG_GAMMA_SRGB	  = 45455; // gAMA chunk value of 1/2.2.
	constexpr auto EXIF_COLOR_SPACE	  = 0xA001;
	constexpr auto EXIF_IFD_POINTER	  = 0x8769;
	constexpr auto EXIF_SRGB		  = 1;

	static unsigned int ReadBigEndian32(const unsigned char* data)
	{
		return ((unsigned int)data[0] << 24) | ((unsigned int)data[1] << 16) | ((unsigned int)data[2] << 8) | (unsigned int)data[3];
	}

	static unsigned int ReadTiff(const unsigned char* data, int byteCount, bool isBigEndian)
	{
		unsigned int value = 0;
		for (int i = 0; i < byteCount; i++)
			value |= (unsigned int)data[isBigEndian ? i : (byteCount - 1 - i)] << ((byteCount - 1 - i) * 8);

		return value;
	}

	static unsigned int MakeFourCC(char a, char b, char c, char d)
	{
		return ((unsigned int)(unsigned char)a | ((unsigned int)(unsigned char)b << 8) |
				((unsigned int)(unsigned char)c << 16) | ((unsigned int)(unsigned char)d << 24));
	}

	// Value of a tag in a TIFF directory, or -1 if the tag is missing or the directory is out of bounds.
	static int FindTiffTag(const unsigned char* tiff, int tiffSize, unsigned int dirOffset, unsigned int tag, bool isBigEndian)
	{
		if (dirOffset + 2 > (unsigned int)tiffSize)
			return -1;

		unsigned int entryCount = ReadTiff(&tiff[dirOffset], 2, isBigEndian);
		for (unsigned int i = 0; i < entryCount; i++)
		{
			unsigned int entry = dirOffset + 2 + (i * 12);
			if (entry + 12 > (unsigned int)tiffSize)
				return -1;

			if (ReadTiff(&tiff[entry], 2, isBigEndian) != tag)
				continue;

			// SHORT values sit in the first two bytes of the value field, LONG values take all four.
			unsigned int type = ReadTiff(&tiff[entry + 2], 2, isBigEndian);
			return (int)ReadTiff(&tiff[entry + 8], (type == 3) ? 2 : 4, isBigEndian);
		}

		return -1;
	}

	// PNG files are sRGB with an sRGB chunk, or else with a gAMA chunk of 1/2.2.
	static bool IsSrgbPng(const unsigned char* data, int dataSize)
	{
		bool hasGamma = false;
		unsigned int gamma = 0;

		for (unsigned int offset = 8; offset + 8 <= (unsigned int)dataSize;)
		{
			unsigned int length = ReadBigEndian32(&data[offset]);
			const auto* type = &data[offset + 4];

			if (memcmp(type, "IDAT", 4) == 0 || memcmp(type, "IEND", 4) == 0)
				break;

			if (memcmp(type, "sRGB", 4) == 0)
				return true;

			if (memcmp(type, "gAMA", 4) == 0 && length == 4 && offset + 12 <= (unsigned int)dataSize)
			{
				hasGamma = true;
				gamma = ReadBigEndian32(&data[offset + 8]);
			}

			if (length > (unsigned int)dataSize)
				break;

			offset += 12 + length;
		}

		return (hasGamma && gamma == PNG_GAMMA_SRGB);
	}

	// JPEG files are sRGB when the color space tag of their EXIF data says so.
	static bool IsSrgbJpeg(const unsigned char* data, int dataSize)
	{
		for (int offset = 2; offset + 4 <= dataSize;)
		{
			if (data[offset] != 0xFF)
				return false;

			unsigned char marker = data[offset + 1];
			if (marker == 0xDA || marker == 0xD9) // Start of scan or end of image.
				return false;

			int length = (data[offset + 2] << 8) | data[offset + 3];
			const auto* segment = &data[offset + 4];
			int segmentSize = std::min(length - 2, dataSize - (offset + 4));

			if (marker == 0xE1 && segmentSize > 14 && memcmp(segment, "Exif\0\0", 6) == 0)
			{
				const auto* tiff = segment + 6;
				int tiffSize = segmentSize - 6;
				bool isBigEndian = (tiff[0] == 'M');

				unsigned int ifdOffset = ReadTiff(&tiff[4], 4, isBigEndian);
				int exifOffset = FindTiffTag(tiff, tiffSize, ifdOffset, EXIF_IFD_POINTER, isBigEndian);
				if (exifOffset < 0)
					return false;

				return (FindTiffTag(tiff, tiffSize, (unsigned int)exifOffset, EXIF_COLOR_SPACE, isBigEndian) == EXIF_SRGB);
			}

			offset += 2 + length;
		}

		return false;
	}

	static void DecodeColorBlock(const unsigned char* block, unsigned char colors[4][4], bool allowTransparency)
	{
		unsigned int color0 = block[0] | (block[1] << 8);
		unsigned int color1 = block[2] | (block[3] << 8);

		auto expand = [](unsigned int color, unsigned char* target)
		{
			unsigned int r = (color >> 11) & 31;
			unsigned int g = (color >> 5) & 63;
			unsigned int b = color & 31;
			target[0] = (unsigned char)((r << 3) | (r >> 2));
			target[1] = (unsigned char)((g << 2) | (g >> 4));
			target[2] = (unsigned char)((b << 3) | (b >> 2));
			target[3] = 255;
		};

		expand(color0, colors[0]);
		expand(color1, colors[1]);

		for (int channel = 0; channel < 3; channel++)
		{
			if (color0 > color1 || !allowTransparency)
			{
				colors[2][channel] = (unsigned char)(((2 * colors[0][channel]) + colors[1][channel] + 1) / 3);
				colors[3][channel] = (unsigned char)((colors[0][channel] + (2 * colors[1][channel]) + 1) / 3);
			}
			else
			{
				colors[2][channel] = (unsigned char)((colors[0][channel] + colors[1][channel] + 1) / 2);
				colors[3][channel] = 0;
			}
		}

		colors[2][3] = 255;
		colors[3][3] = (color0 > color1 || !allowTransparency) ? 255 : 0;
	}

	static void DecodeAlphaBlock(const unsigned char* block, unsigned char alphas[8])
	{
		alphas[0] = block[0];
		alphas[1] = block[1];

		if (alphas[0] > alphas[1])
		{
			for (int i = 1; i < 7; i++)
				alphas[i + 1] = (unsigned char)((((7 - i) * alphas[0]) + (i * alphas[1]) + 3) / 7);
		}
		else
		{
			for (int i = 1; i < 5; i++)
				alphas[i + 1] = (unsigned char)((((5 - i) * alphas[0]) + (i * alphas[1]) + 2) / 5);

			alphas[6] = 0;
			alphas[7] = 255;
		}
	}

	// Decodes the top level of a DDS file with BC1, BC2 or BC3 blocks, the formats the WIC DDS codec reads.
	static bool DecodeDds(const unsigned char* data, int dataSize, DecodedImage& image)
	{
		if (dataSize < DDS_HEADER_SIZE)
			return false;

		int height = (int)(data[12] | (data[13] << 8) | (data[14] << 16) | ((unsigned int)data[15] << 24));
		int width = (int)(data[16] | (data[17] << 8) | (data[18] << 16) | ((unsigned int)data[19] << 24));
		unsigned int fourCC = data[DDS_FOURCC_OFFSET] | (data[DDS_FOURCC_OFFSET + 1] << 8) |
							  (data[DDS_FOURCC_OFFSET + 2] << 16) | ((unsigned int)data[DDS_FOURCC_OFFSET + 3] << 24);

		int version = 0;
		if (fourCC == MakeFourCC('D', 'X', 'T', '1'))
		{
			version = 1;
		}
		else if (fourCC == MakeFourCC('D', 'X', 'T', '2') || fourCC == MakeFourCC('D', 'X', 'T', '3'))
		{
			version = 2;
		}
		else if (fourCC == MakeFourCC('D', 'X', 'T', '4') || fourCC == MakeFourCC('D', 'X', 'T', '5'))
		{
			version = 3;
		}
		else
		{
			TENLog("Unsupported DDS image format: only BC1, BC2 and BC3 are supported.", LogLevel::Warning);
			return false;
		}

		int blockSize = (version == 1) ? 8 : 16;
		int blocksX = std::max((width + 3) / 4, 1);
		int blocksY = std::max((height + 3) / 4, 1);
		if (width <= 0 || height <= 0 || DDS_HEADER_SIZE + (blocksX * blocksY * blockSize) > dataSize)
			return false;

		image.Width = width;
		image.Height = height;
		image.Pixels.assign((long long)width * height * 4, 0);

		const auto* block = &data[DDS_HEADER_SIZE];
		for (int by = 0; by < blocksY; by++)
		{
			for (int bx = 0; bx < blocksX; bx++, block += blockSize)
			{
				unsigned char colors[4][4];
				unsigned char alphas[8] = {};
				const auto* colorBlock = (version == 1) ? block : (block + 8);

				DecodeColorBlock(colorBlock, colors, version == 1);
				if (version == 3)
					DecodeAlphaBlock(block, alphas);

				unsigned int colorIndices = colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | ((unsigned int)colorBlock[7] << 24);
				unsigned long long alphaIndices = 0;
				for (int i = 0; i < 6; i++)
					alphaIndices |= (unsigned long long)block[2 + i] << (8 * i);

				for (int py = 0; py < 4; py++)
				{
					for (int px = 0; px < 4; px++)
					{
						int x = (bx * 4) + px;
						int y = (by * 4) + py;
						if (x >= width || y >= height)
							continue;

						int texel = (py * 4) + px;
						auto* target = &image.Pixels[(((long long)y * width) + x) * 4];
						memcpy(target, colors[(colorIndices >> (texel * 2)) & 3], 4);

						if (version == 2)
						{
							unsigned int alpha = (block[texel / 2] >> ((texel % 2) * 4)) & 15;
							target[3] = (unsigned char)((alpha << 4) | alpha);
						}
						else if (version == 3)
						{
							target[3] = alphas[(alphaIndices >> (texel * 3)) & 7];
						}
					}
				}
			}
		}

		return true;
	}

	bool IsDdsImage(const unsigned char* data, int dataSize)
	{
		return (dataSize >= 4 && memcmp(data, "DDS ", 4) == 0);
	}

	bool DecodeImage(const unsigned char* data, int dataSize, DecodedImage& image)
	{
		image = {};

		if (IsDdsImage(data, dataSize))
			return DecodeDds(data, dataSize, image);

		int channelCount = 0;
		stbi_set_flip_vertically_on_load(false);
		auto* pixels = stbi_load_from_memory(data, dataSize, &image.Width, &image.Height, &channelCount, 4);
		if (pixels == nullptr)
			return false;

		image.Pixels.assign(pixels, pixels + ((long long)image.Width * image.Height * 4));
		stbi_image_free(pixels);

		static const unsigned char PNG_SIGNATURE[] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
		if (dataSize >= 8 && memcmp(data, PNG_SIGNATURE, 8) == 0)
		{
			image.IsSrgb = IsSrgbPng(data, dataSize);
		}
		else if (dataSize >= 2 && data[0] == 0xFF && data[1] == 0xD8)
		{
			image.IsSrgb = IsSrgbJpeg(data, dataSize);
		}

		return true;
	}
}
