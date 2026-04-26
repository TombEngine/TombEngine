#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>

namespace TEN::Renderer::Native::DirectX11
{
	inline int GetBytesPerPixel(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
			return 16;

		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
		case DXGI_FORMAT_R32G32B32_TYPELESS:
			return 12;

		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R32G32_TYPELESS:
			return 8;

		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
			return 4;

		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_D16_UNORM:
			return 2;

		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_A8_UNORM:
			return 1;

		// Block-compressed formats: return 0 and handle separately.
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
		case DXGI_FORMAT_BC4_TYPELESS:
			return 0; // 8 bytes per 4x4 block = 0.5 bpp

		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
		case DXGI_FORMAT_BC7_TYPELESS:
			return 0; // 16 bytes per 4x4 block = 1 bpp

		default:
			return 4; // Fallback.
		}
	}

	// Compute total bytes for a texture, handling block-compressed formats.
	inline int ComputeTextureSize(int width, int height, int arraySize, int mipLevels, DXGI_FORMAT format)
	{
		int totalBytes = 0;
		int bpp = GetBytesPerPixel(format);

		for (int mip = 0; mip < mipLevels; mip++)
		{
			int mipWidth = std::max(width >> mip, 1);
			int mipHeight = std::max(height >> mip, 1);

			if (bpp > 0)
			{
				totalBytes += mipWidth * mipHeight * bpp;
			}
			else
			{
				// Block-compressed format.
				int blocksX = std::max((mipWidth + 3) / 4, 1);
				int blocksY = std::max((mipHeight + 3) / 4, 1);
				int blockSize = 0;

				switch (format)
				{
				case DXGI_FORMAT_BC1_UNORM:
				case DXGI_FORMAT_BC1_UNORM_SRGB:
				case DXGI_FORMAT_BC1_TYPELESS:
				case DXGI_FORMAT_BC4_UNORM:
				case DXGI_FORMAT_BC4_SNORM:
				case DXGI_FORMAT_BC4_TYPELESS:
					blockSize = 8;
					break;
				default:
					blockSize = 16;
					break;
				}

				totalBytes += blocksX * blocksY * blockSize;
			}
		}

		return totalBytes * arraySize;
	}

	inline std::string DXGIFormatToString(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_R8G8B8A8_UNORM:      return "RGBA8_UNORM";
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "RGBA8_SRGB";
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:    return "RGBA8_TYPELESS";
		case DXGI_FORMAT_B8G8R8A8_UNORM:       return "BGRA8_UNORM";
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:  return "BGRA8_SRGB";
		case DXGI_FORMAT_R32_FLOAT:            return "R32_FLOAT";
		case DXGI_FORMAT_R16G16_FLOAT:         return "RG16_FLOAT";
		case DXGI_FORMAT_R16G16B16A16_FLOAT:   return "RGBA16_FLOAT";
		case DXGI_FORMAT_R32G32B32A32_FLOAT:   return "RGBA32_FLOAT";
		case DXGI_FORMAT_R11G11B10_FLOAT:      return "R11G11B10_FLOAT";
		case DXGI_FORMAT_D32_FLOAT:            return "D32_FLOAT";
		case DXGI_FORMAT_D24_UNORM_S8_UINT:    return "D24S8";
		case DXGI_FORMAT_D16_UNORM:            return "D16_UNORM";
		case DXGI_FORMAT_R8_UNORM:             return "R8_UNORM";
		case DXGI_FORMAT_R8G8_UNORM:           return "RG8_UNORM";
		case DXGI_FORMAT_BC1_UNORM:            return "BC1_UNORM";
		case DXGI_FORMAT_BC1_UNORM_SRGB:       return "BC1_SRGB";
		case DXGI_FORMAT_BC2_UNORM:            return "BC2_UNORM";
		case DXGI_FORMAT_BC2_UNORM_SRGB:       return "BC2_SRGB";
		case DXGI_FORMAT_BC3_UNORM:            return "BC3_UNORM";
		case DXGI_FORMAT_BC3_UNORM_SRGB:       return "BC3_SRGB";
		case DXGI_FORMAT_BC7_UNORM:            return "BC7_UNORM";
		case DXGI_FORMAT_BC7_UNORM_SRGB:       return "BC7_SRGB";
		default:                               return "DXGI_FORMAT_" + std::to_string((int)format);
		}
	}

	inline std::string BytesToMBString(int bytes)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%.2f", bytes / (1024.0f * 1024.0f));
		return buf;
	}

	inline DXGI_FORMAT GetDXGIFormat(SurfaceFormat format)
	{
		switch (format)
		{
		case SurfaceFormat::SF_RGBA8_Unorm:
			return DXGI_FORMAT_R8G8B8A8_UNORM;
		case SurfaceFormat::SF_RGBA8_Unorm_Srgb:
			return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case SurfaceFormat::SF_R32_Float:
			return DXGI_FORMAT_R32_FLOAT;
		case SurfaceFormat::SF_RG8_Unorm:
			return DXGI_FORMAT_R8G8_UNORM;
		case SurfaceFormat::SF_R8_Unorm:
			return DXGI_FORMAT_R8_UNORM;
		case SurfaceFormat::SF_RGBA32_Float:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case SurfaceFormat::SF_BGRA8_Unorm:
			return DXGI_FORMAT_B8G8R8A8_UNORM;
		case SurfaceFormat::Unknown:
		default:
			return DXGI_FORMAT_UNKNOWN;
		}
	}

	inline DXGI_FORMAT GetDXGIFormat(DepthFormat format)
	{
		switch (format)
		{
		case DepthFormat::Depth24Stencil8:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case DepthFormat::Depth32:
			return DXGI_FORMAT_D32_FLOAT;
		default:
			return DXGI_FORMAT_D32_FLOAT;
		}
	}

	inline unsigned int GetClearFlags(DepthStencilClearFlags flags)
	{
		switch (flags)
		{
		case DepthStencilClearFlags::Depth:
			return D3D11_CLEAR_DEPTH;
		case DepthStencilClearFlags::Stencil:
			return D3D11_CLEAR_STENCIL;
		case DepthStencilClearFlags::DepthAndStencil:
		default:
			return (D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL);
		}
	}
}

#endif
