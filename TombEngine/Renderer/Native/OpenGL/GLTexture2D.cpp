#include "framework.h"

#ifdef HAS_OPENGL

#include "Renderer/Native/OpenGL/GLTexture2D.h"
#include "Renderer/Native/OpenGL/GLUtils.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace TEN::Renderer::Native::OpenGL
{
	GLTexture2D::GLTexture2D(int width, int height, GLenum internalFormat, GLenum pixelFormat, GLenum pixelType, void* data)
	{
		_width = width;
		_height = height;
		_internalFormat = internalFormat;

		glCreateTextures(GL_TEXTURE_2D, 1, &_texture);
		CheckGLError("glCreateTextures 2D (tex=" + std::to_string(_texture) + ")");

		glTextureStorage2D(_texture, 1, internalFormat, width, height);
		CheckGLError("glTextureStorage2D (" + std::to_string(width) + "x" + std::to_string(height) +
			" " + GLFormatToString(internalFormat) + " tex=" + std::to_string(_texture) + ")");

		if (data)
		{
			glTextureSubImage2D(_texture, 0, 0, 0, width, height, pixelFormat, pixelType, data);
			CheckGLError("glTextureSubImage2D");
		}

		glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		CheckGLError("glTextureParameteri filters");

		int vramSize = ComputeTextureSize(width, height, 1, 1, internalFormat);
		_vram = VRAMAllocation(VRAMCategory::Texture, vramSize,
			"Texture2D allocated: " + std::to_string(width) + "x" + std::to_string(height) +
			" " + GLFormatToString(internalFormat) +
			" (" + BytesToMBString(vramSize) + " MB)");
	}

	GLTexture2D::GLTexture2D(const std::string& fileName)
	{
		int channels;
		stbi_set_flip_vertically_on_load(false);
		unsigned char* pixels = stbi_load(fileName.c_str(), &_width, &_height, &channels, 4);

		if (!pixels)
		{
			TENLog("Failed to load texture: " + fileName, LogLevel::Error);
			return;
		}

		_internalFormat = GL_RGBA8;

		int mipLevels = 1 + (int)std::floor(std::log2(std::max(_width, _height)));

		glCreateTextures(GL_TEXTURE_2D, 1, &_texture);
		CheckGLError("glCreateTextures FromFile tex=" + std::to_string(_texture));

		glTextureStorage2D(_texture, mipLevels, GL_RGBA8, _width, _height);
		CheckGLError("glTextureStorage2D FromFile " + std::to_string(_width) + "x" + std::to_string(_height) +
			" mips=" + std::to_string(mipLevels) + " tex=" + std::to_string(_texture));

		glTextureSubImage2D(_texture, 0, 0, 0, _width, _height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		CheckGLError("glTextureSubImage2D FromFile");

		glGenerateTextureMipmap(_texture);
		CheckGLError("glGenerateTextureMipmap FromFile");

		glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		CheckGLError("glTextureParameteri FromFile");

		stbi_image_free(pixels);

		int vramSize = ComputeTextureSize(_width, _height, 1, mipLevels, GL_RGBA8);
		_vram = VRAMAllocation(VRAMCategory::Texture, vramSize,
			"Texture2D allocated: " + std::to_string(_width) + "x" + std::to_string(_height) +
			" RGBA8 mips=" + std::to_string(mipLevels) +
			" (" + BytesToMBString(vramSize) + " MB)");
	}

	GLTexture2D::GLTexture2D(int dataSize, unsigned char* data)
	{
		// Check for DDS magic number "DDS ".
		if (dataSize >= 4 && data[0] == 'D' && data[1] == 'D' && data[2] == 'S' && data[3] == ' ')
		{
			// Parse DDS header.
			// DDS header is 128 bytes: 4 byte magic + 124 byte header.
			if (dataSize < 128)
			{
				TENLog("DDS data too small", LogLevel::Error);
				return;
			}

			struct DDSHeader
			{
				unsigned int size;
				unsigned int flags;
				unsigned int height;
				unsigned int width;
				unsigned int pitchOrLinearSize;
				unsigned int depth;
				unsigned int mipMapCount;
				unsigned int reserved1[11];
				// Pixel format.
				unsigned int pfSize;
				unsigned int pfFlags;
				unsigned int pfFourCC;
				unsigned int pfRGBBitCount;
				unsigned int pfRBitMask;
				unsigned int pfGBitMask;
				unsigned int pfBBitMask;
				unsigned int pfABitMask;
				unsigned int caps;
				unsigned int caps2;
				unsigned int caps3;
				unsigned int caps4;
				unsigned int reserved2;
			};

			const DDSHeader* hdr = reinterpret_cast<const DDSHeader*>(data + 4);
			_width = hdr->width;
			_height = hdr->height;
			int mipCount = std::max((unsigned int)1, hdr->mipMapCount);

			const unsigned char* pixelData = data + 128;
			int remainingSize = dataSize - 128;

			// Check for DX10 extended header.
			unsigned int fourCC = hdr->pfFourCC;
			GLenum compressedFormat = 0;
			int blockSize = 0;
			bool isDX10 = false;

			constexpr unsigned int FOURCC_DXT1 = 0x31545844; // "DXT1"
			constexpr unsigned int FOURCC_DXT3 = 0x33545844; // "DXT3"
			constexpr unsigned int FOURCC_DXT5 = 0x35545844; // "DXT5"
			constexpr unsigned int FOURCC_DX10 = 0x30315844; // "DX10"

			if (fourCC == FOURCC_DX10)
			{
				isDX10 = true;
				// DX10 extended header is 20 bytes after main header.
				if (dataSize < 148)
				{
					TENLog("DDS DX10 data too small", LogLevel::Error);
					return;
				}

				struct DX10Header
				{
					unsigned int dxgiFormat;
					unsigned int resourceDimension;
					unsigned int miscFlag;
					unsigned int arraySize;
					unsigned int miscFlags2;
				};

				const DX10Header* dx10 = reinterpret_cast<const DX10Header*>(data + 128);
				pixelData = data + 148;
				remainingSize = dataSize - 148;

				// Map DXGI formats to GL compressed formats.
				switch (dx10->dxgiFormat)
				{
				case 71: // DXGI_FORMAT_BC1_UNORM
					compressedFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
					blockSize = 8;
					break;
				case 77: // DXGI_FORMAT_BC3_UNORM
					compressedFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
					blockSize = 16;
					break;
				case 98: // DXGI_FORMAT_BC7_UNORM
					compressedFormat = GL_COMPRESSED_RGBA_BPTC_UNORM;
					blockSize = 16;
					break;
				default:
					TENLog("Unsupported DX10 DXGI format: " + std::to_string(dx10->dxgiFormat), LogLevel::Error);
					return;
				}
			}
			else
			{
				switch (fourCC)
				{
				case FOURCC_DXT1:
					compressedFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
					blockSize = 8;
					break;
				case FOURCC_DXT3:
					compressedFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
					blockSize = 16;
					break;
				case FOURCC_DXT5:
					compressedFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
					blockSize = 16;
					break;
				default:
					TENLog("Unsupported DDS FourCC: " + std::to_string(fourCC), LogLevel::Error);
					return;
				}
			}

			_internalFormat = compressedFormat;

			glCreateTextures(GL_TEXTURE_2D, 1, &_texture);
			glTextureStorage2D(_texture, mipCount, compressedFormat, _width, _height);

			const unsigned char* mipData = pixelData;
			for (int mip = 0; mip < mipCount; mip++)
			{
				int mipWidth = std::max(_width >> mip, 1);
				int mipHeight = std::max(_height >> mip, 1);
				int blocksX = std::max((mipWidth + 3) / 4, 1);
				int blocksY = std::max((mipHeight + 3) / 4, 1);
				int mipSize = blocksX * blocksY * blockSize;

				if (mipData + mipSize > data + dataSize)
					break;

				glCompressedTextureSubImage2D(_texture, mip, 0, 0, mipWidth, mipHeight,
					compressedFormat, mipSize, mipData);
				mipData += mipSize;
			}

			glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER,
				mipCount > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
			glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			ThrowIfGLError("CreateTexture2D from DDS memory");

			// Estimate VRAM.
			int vramSize = 0;
			for (int mip = 0; mip < mipCount; mip++)
			{
				int mw = std::max(_width >> mip, 1);
				int mh = std::max(_height >> mip, 1);
				vramSize += std::max((mw + 3) / 4, 1) * std::max((mh + 3) / 4, 1) * blockSize;
			}
			_vram = VRAMAllocation(VRAMCategory::Texture, vramSize,
				"Texture2D DDS allocated: " + std::to_string(_width) + "x" + std::to_string(_height) +
				" mips=" + std::to_string(mipCount) +
				" (" + BytesToMBString(vramSize) + " MB)");
		}
		else
		{
			// Non-DDS: decode with stb_image (PNG, TGA, etc.).
			int channels;
			stbi_set_flip_vertically_on_load(false);
			unsigned char* pixels = stbi_load_from_memory(data, dataSize, &_width, &_height, &channels, 4);

			if (!pixels)
			{
				TENLog("Failed to decode texture from memory", LogLevel::Error);
				return;
			}

			_internalFormat = GL_RGBA8;

			int mipLevels = 1 + (int)std::floor(std::log2(std::max(_width, _height)));

			glCreateTextures(GL_TEXTURE_2D, 1, &_texture);
			glTextureStorage2D(_texture, mipLevels, GL_RGBA8, _width, _height);
			glTextureSubImage2D(_texture, 0, 0, 0, _width, _height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
			glGenerateTextureMipmap(_texture);

			glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			stbi_image_free(pixels);

			ThrowIfGLError("CreateTexture2D from memory (PNG)");

			int vramSize = ComputeTextureSize(_width, _height, 1, mipLevels, GL_RGBA8);
			_vram = VRAMAllocation(VRAMCategory::Texture, vramSize,
				"Texture2D PNG allocated: " + std::to_string(_width) + "x" + std::to_string(_height) +
				" mips=" + std::to_string(mipLevels) +
				" (" + BytesToMBString(vramSize) + " MB)");
		}
	}

	GLTexture2D::~GLTexture2D()
	{
		if (_texture && _ownsTexture)
			glDeleteTextures(1, &_texture);
	}
}

#endif
