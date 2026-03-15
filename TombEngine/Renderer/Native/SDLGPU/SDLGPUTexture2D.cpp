#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUTexture2D.h"
#include "Renderer/Native/SDLGPU/SDLGPUUtils.h"

#include <stb_image.h>
#include <algorithm>
#include <cmath>

namespace TEN::Renderer::Native::SDLGPU
{
	// --- Helper: upload raw pixels to a GPU texture via transfer buffer ---
	static void UploadPixels(SDL_GPUDevice* device, SDL_GPUTexture* texture,
	                          int width, int height, int mipLevel, const void* data, size_t dataSize)
	{
		SDL_GPUTransferBufferCreateInfo tbci = {};
		tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		tbci.size = (Uint32)dataSize;
		auto transferBuffer = SDL_CreateGPUTransferBuffer(device, &tbci);

		void* mapped = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
		std::memcpy(mapped, data, dataSize);
		SDL_UnmapGPUTransferBuffer(device, transferBuffer);

		auto cmdBuf = SDL_AcquireGPUCommandBuffer(device);
		auto copyPass = SDL_BeginGPUCopyPass(cmdBuf);

		SDL_GPUTextureTransferInfo src = {};
		src.transfer_buffer = transferBuffer;
		src.offset = 0;

		SDL_GPUTextureRegion dst = {};
		dst.texture = texture;
		dst.mip_level = mipLevel;
		dst.w = width;
		dst.h = height;
		dst.d = 1;

		SDL_UploadToGPUTexture(copyPass, &src, &dst, false);
		SDL_EndGPUCopyPass(copyPass);
		SDL_SubmitGPUCommandBuffer(cmdBuf);

		SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
	}

	// --- DDS helpers ---
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

	struct DX10Header
	{
		unsigned int dxgiFormat;
		unsigned int resourceDimension;
		unsigned int miscFlag;
		unsigned int arraySize;
		unsigned int miscFlags2;
	};

	constexpr unsigned int FOURCC_DXT1 = 0x31545844;
	constexpr unsigned int FOURCC_DXT3 = 0x33545844;
	constexpr unsigned int FOURCC_DXT5 = 0x35545844;
	constexpr unsigned int FOURCC_DX10 = 0x30315844;

	struct BCFormatInfo
	{
		SDL_GPUTextureFormat format;
		int blockSize;
	};

	static bool GetBCFormat(unsigned int fourCC, const DX10Header* dx10, BCFormatInfo& out)
	{
		if (dx10)
		{
			switch (dx10->dxgiFormat)
			{
			case 71: out = { SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM, 8 }; return true;
			case 77: out = { SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM, 16 }; return true;
			case 98: out = { SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM, 16 }; return true;
			default: return false;
			}
		}

		switch (fourCC)
		{
		case FOURCC_DXT1: out = { SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM, 8 }; return true;
		case FOURCC_DXT3: out = { SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM, 16 }; return true;
		case FOURCC_DXT5: out = { SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM, 16 }; return true;
		default: return false;
		}
	}

	// --- Constructors ---

	SDLGPUTexture2D::SDLGPUTexture2D(SDL_GPUDevice* device, int width, int height, SDL_GPUTextureFormat format, void* data)
		: _device(device), _width(width), _height(height), _format(format)
	{
		SDL_GPUTextureCreateInfo tci = {};
		tci.type = SDL_GPU_TEXTURETYPE_2D;
		tci.format = format;
		tci.width = width;
		tci.height = height;
		tci.layer_count_or_depth = 1;
		tci.num_levels = 1;
		tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

		_texture = SDL_CreateGPUTexture(device, &tci);
		_vram = VRAMAllocation(VRAMCategory::Texture, (int)ComputeTextureSize(width, height, format), "");

		if (data && _texture)
			UploadPixels(device, _texture, width, height, 0, data, ComputeTextureSize(width, height, format));
	}

	SDLGPUTexture2D::SDLGPUTexture2D(SDL_GPUDevice* device, const std::string& fileName)
		: _device(device)
	{
		int channels;
		stbi_set_flip_vertically_on_load(false);
		unsigned char* pixels = stbi_load(fileName.c_str(), &_width, &_height, &channels, 4);

		if (!pixels)
		{
			TENLog("Failed to load texture: " + fileName, LogLevel::Error);
			return;
		}

		_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
		int mipLevels = 1 + (int)std::floor(std::log2(std::max(_width, _height)));

		SDL_GPUTextureCreateInfo tci = {};
		tci.type = SDL_GPU_TEXTURETYPE_2D;
		tci.format = _format;
		tci.width = _width;
		tci.height = _height;
		tci.layer_count_or_depth = 1;
		tci.num_levels = mipLevels;
		tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
			| (mipLevels > 1 ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET : 0);

		_texture = SDL_CreateGPUTexture(device, &tci);

		if (_texture)
		{
			UploadPixels(device, _texture, _width, _height, 0, pixels,
				(size_t)_width * _height * 4);

			// Generate mipmaps via blit chain.
			if (mipLevels > 1)
			{
				auto cmdBuf = SDL_AcquireGPUCommandBuffer(device);
				for (int mip = 1; mip < mipLevels; ++mip)
				{
					int srcW = std::max(_width >> (mip - 1), 1);
					int srcH = std::max(_height >> (mip - 1), 1);
					int dstW = std::max(_width >> mip, 1);
					int dstH = std::max(_height >> mip, 1);

					SDL_GPUBlitInfo blit = {};
					blit.source.texture = _texture;
					blit.source.mip_level = mip - 1;
					blit.source.w = srcW;
					blit.source.h = srcH;
					blit.destination.texture = _texture;
					blit.destination.mip_level = mip;
					blit.destination.w = dstW;
					blit.destination.h = dstH;
					blit.filter = SDL_GPU_FILTER_LINEAR;
					blit.load_op = SDL_GPU_LOADOP_DONT_CARE;

					SDL_BlitGPUTexture(cmdBuf, &blit);
				}
				SDL_SubmitGPUCommandBuffer(cmdBuf);
			}
		}

		stbi_image_free(pixels);

		size_t totalVram = 0;
		for (int mip = 0; mip < mipLevels; ++mip)
			totalVram += (size_t)std::max(_width >> mip, 1) * std::max(_height >> mip, 1) * 4;
		_vram = VRAMAllocation(VRAMCategory::Texture, (int)totalVram, "");
	}

	SDLGPUTexture2D::SDLGPUTexture2D(SDL_GPUDevice* device, int dataSize, unsigned char* data)
		: _device(device)
	{
		// Check for DDS magic number.
		if (dataSize >= 4 && data[0] == 'D' && data[1] == 'D' && data[2] == 'S' && data[3] == ' ')
		{
			if (dataSize < 128)
			{
				TENLog("DDS data too small", LogLevel::Error);
				return;
			}

			const DDSHeader* hdr = reinterpret_cast<const DDSHeader*>(data + 4);
			_width = hdr->width;
			_height = hdr->height;
			int mipCount = std::max((int)hdr->mipMapCount, 1);

			const unsigned char* pixelData = data + 128;
			int headerSize = 128;

			const DX10Header* dx10 = nullptr;
			if (hdr->pfFourCC == FOURCC_DX10)
			{
				if (dataSize < 148)
				{
					TENLog("DDS DX10 data too small", LogLevel::Error);
					return;
				}
				dx10 = reinterpret_cast<const DX10Header*>(data + 128);
				pixelData = data + 148;
				headerSize = 148;
			}

			BCFormatInfo bcInfo;
			if (!GetBCFormat(hdr->pfFourCC, dx10, bcInfo))
			{
				TENLog("Unsupported DDS format: " + std::to_string(hdr->pfFourCC), LogLevel::Error);
				return;
			}

			_format = bcInfo.format;

			SDL_GPUTextureCreateInfo tci = {};
			tci.type = SDL_GPU_TEXTURETYPE_2D;
			tci.format = _format;
			tci.width = _width;
			tci.height = _height;
			tci.layer_count_or_depth = 1;
			tci.num_levels = mipCount;
			tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

			_texture = SDL_CreateGPUTexture(device, &tci);

			if (_texture)
			{
				const unsigned char* mipData = pixelData;
				size_t totalVram = 0;

				for (int mip = 0; mip < mipCount; ++mip)
				{
					int mipWidth = std::max((int)(_width >> mip), 1);
					int mipHeight = std::max((int)(_height >> mip), 1);
					int blocksX = std::max((mipWidth + 3) / 4, 1);
					int blocksY = std::max((mipHeight + 3) / 4, 1);
					size_t mipSize = (size_t)blocksX * blocksY * bcInfo.blockSize;

					if (mipData + mipSize > data + dataSize)
						break;

					UploadPixels(device, _texture, mipWidth, mipHeight, mip, mipData, mipSize);
					mipData += mipSize;
					totalVram += mipSize;
				}

				_vram = VRAMAllocation(VRAMCategory::Texture, (int)totalVram, "");
			}
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

			_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
			int mipLevels = 1 + (int)std::floor(std::log2(std::max(_width, _height)));

			SDL_GPUTextureCreateInfo tci = {};
			tci.type = SDL_GPU_TEXTURETYPE_2D;
			tci.format = _format;
			tci.width = _width;
			tci.height = _height;
			tci.layer_count_or_depth = 1;
			tci.num_levels = mipLevels;
			tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
				| (mipLevels > 1 ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET : 0);

			_texture = SDL_CreateGPUTexture(device, &tci);

			if (_texture)
			{
				UploadPixels(device, _texture, _width, _height, 0, pixels,
					(size_t)_width * _height * 4);

				// Generate mipmaps via blit chain.
				if (mipLevels > 1)
				{
					auto cmdBuf = SDL_AcquireGPUCommandBuffer(device);
					for (int mip = 1; mip < mipLevels; ++mip)
					{
						int srcW = std::max(_width >> (mip - 1), 1);
						int srcH = std::max(_height >> (mip - 1), 1);
						int dstW = std::max(_width >> mip, 1);
						int dstH = std::max(_height >> mip, 1);

						SDL_GPUBlitInfo blit = {};
						blit.source.texture = _texture;
						blit.source.mip_level = mip - 1;
						blit.source.w = srcW;
						blit.source.h = srcH;
						blit.destination.texture = _texture;
						blit.destination.mip_level = mip;
						blit.destination.w = dstW;
						blit.destination.h = dstH;
						blit.filter = SDL_GPU_FILTER_LINEAR;
						blit.load_op = SDL_GPU_LOADOP_DONT_CARE;

						SDL_BlitGPUTexture(cmdBuf, &blit);
					}
					SDL_SubmitGPUCommandBuffer(cmdBuf);
				}
			}

			stbi_image_free(pixels);

			size_t totalVram = 0;
			for (int mip = 0; mip < mipLevels; ++mip)
				totalVram += (size_t)std::max(_width >> mip, 1) * std::max(_height >> mip, 1) * 4;
			_vram = VRAMAllocation(VRAMCategory::Texture, (int)totalVram, "");
		}
	}

	SDLGPUTexture2D::~SDLGPUTexture2D()
	{
		if (_texture && _device && _ownsTexture)
			SDL_ReleaseGPUTexture(_device, _texture);
	}
}

#endif
