#pragma once

#include <atomic>
#include <d3d11.h>
#include <string>

#include "Specific/trutils.h"

using namespace TEN::Utils;

namespace TEN::Renderer::Graphics
{
	enum class VRAMCategory
	{
		Texture,
		RenderTarget,
		VertexBuffer,
		IndexBuffer,

		Count
	};

	class VRAMTracker
	{
	public:
		static VRAMTracker& Get()
		{
			static VRAMTracker instance;
			return instance;
		}

		void Add(VRAMCategory category, unsigned long long bytes)
		{
			_categories[(int)category] += bytes;
			_total += bytes;
		}

		void Remove(VRAMCategory category, unsigned long long bytes)
		{
			_categories[(int)category] -= bytes;
			_total -= bytes;
		}

		unsigned long long GetTotal() const
		{
			return _total.load();
		}

		unsigned long long GetCategory(VRAMCategory category) const
		{
			return _categories[(int)category].load();
		}

		std::string GetSummary() const
		{
			return fmt::format("VRAM usage: {:.1f} MB total, textures: {:.1f} MB, RT: {:.1f} MB, VB: {:.1f} MB, IB: {:.1f} MB.",
				ToMegabytes(GetTotal()),
				ToMegabytes(GetCategory(VRAMCategory::Texture)),
				ToMegabytes(GetCategory(VRAMCategory::RenderTarget)),
				ToMegabytes(GetCategory(VRAMCategory::VertexBuffer)),
				ToMegabytes(GetCategory(VRAMCategory::IndexBuffer)));
		}

		static int GetDXGIFormatBytesPerPixel(DXGI_FORMAT format)
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

			// Block-compressed formats: return 0, use ComputeBlockCompressedSize() instead.
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC4_UNORM:
			case DXGI_FORMAT_BC4_SNORM:
			case DXGI_FORMAT_BC4_TYPELESS:
			case DXGI_FORMAT_BC5_UNORM:
			case DXGI_FORMAT_BC5_SNORM:
			case DXGI_FORMAT_BC5_TYPELESS:
			case DXGI_FORMAT_BC6H_UF16:
			case DXGI_FORMAT_BC6H_SF16:
			case DXGI_FORMAT_BC6H_TYPELESS:
			case DXGI_FORMAT_BC7_UNORM:
			case DXGI_FORMAT_BC7_UNORM_SRGB:
			case DXGI_FORMAT_BC7_TYPELESS:
				return 0;

			default:
				return 4;
			}
		}

		static int ComputeTexture2DSize(const D3D11_TEXTURE2D_DESC& desc)
		{
			int totalSize = 0;
			int bpp = GetDXGIFormatBytesPerPixel(desc.Format);

			int mipLevels = desc.MipLevels;
			if (mipLevels == 0)
			{
				// MipLevels = 0 means full mip chain down to 1x1.
				unsigned int maxDim = std::max(desc.Width, desc.Height);
				mipLevels = 1;
				while (maxDim > 1)
				{
					maxDim >>= 1;
					mipLevels++;
				}
			}

			for (int mip = 0; mip < mipLevels; mip++)
			{
				int mipWidth  = std::max(1u, desc.Width >> mip);
				int mipHeight = std::max(1u, desc.Height >> mip);

				if (bpp > 0)
				{
					totalSize += mipWidth * mipHeight * bpp;
				}
				else
				{
					// Block-compressed: 4x4 blocks.
					int blockWidth  = std::max(1, (mipWidth + 3) / 4);
					int blockHeight = std::max(1, (mipHeight + 3) / 4);

					int blockSize = 8; // BC1, BC4
					switch (desc.Format)
					{
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
						blockSize = 16;
						break;
					default:
						break;
					}

					totalSize += blockWidth * blockHeight * blockSize;
				}
			}

			totalSize *= desc.ArraySize;
			return totalSize;
		}

	private:
		VRAMTracker() = default;

		std::atomic<unsigned long long> _total = 0;
		std::atomic<unsigned long long> _categories[static_cast<int>(VRAMCategory::Count)] = {};
	};
}
