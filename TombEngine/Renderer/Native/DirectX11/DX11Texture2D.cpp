#include "framework.h"

#ifdef HAS_DX11

#include "Renderer/Native/DirectX11/DX11Texture2D.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"
#include "Renderer/Native/DirectX11/DX11Utils.h"
#include "Specific/trutils.h"

#include <stb_image.h>

namespace TEN::Renderer::Native::DirectX11
{
	DX11Texture2D::DX11Texture2D(ID3D11Device* device, int width, int height, DXGI_FORMAT format, void* data, bool isDynamic)
	{
		HRESULT res;

		_width = width;
		_height = height;

		// Texture
		auto desc = D3D11_TEXTURE2D_DESC{};
		desc.Width = width;
		desc.Height = height;
		desc.Format = format;
		desc.MiscFlags = 0;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;

		if (isDynamic)
		{
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		}
		else
		{
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.CPUAccessFlags = 0;
		}

		if (data != nullptr)
		{
			auto subresourceData = D3D11_SUBRESOURCE_DATA{};
			subresourceData.pSysMem = data;
			subresourceData.SysMemPitch = GetBytesPerPixel(format) * width;
			subresourceData.SysMemSlicePitch = 0;

			res = device->CreateTexture2D(&desc, &subresourceData, _texture.GetAddressOf());
		}
		else
		{
			res = device->CreateTexture2D(&desc, nullptr, _texture.GetAddressOf());
		}

		throwIfFailed(res, device,
			"CreateTexture2D (" + std::to_string(width) + "x" + std::to_string(height) +
			" " + DXGIFormatToString(format) + "):");

		// SRV
		auto shaderDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
		shaderDesc.Format = desc.Format;
		shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shaderDesc.Texture2D.MostDetailedMip = 0;
		shaderDesc.Texture2D.MipLevels = 1;

		res = device->CreateShaderResourceView(_texture.Get(), &shaderDesc, _shaderResourceView.GetAddressOf());
		throwIfFailed(res, device,
			"CreateSRV for Texture2D (" + std::to_string(width) + "x" + std::to_string(height) + "):");

		int vramSize = ComputeTextureSize(width, height, 1, 1, format);
		_vram = VRAMAllocation(VRAMCategory::Texture, vramSize,
			"Texture2D allocated: " + std::to_string(width) + "x" + std::to_string(height) +
			" " + DXGIFormatToString(format) +
			" (" + BytesToMBString(vramSize) + " MB)");
	}

	// ======================================================================
	// Constructor 2: from file on disk (using stb_image).
	// ======================================================================
	DX11Texture2D::DX11Texture2D(ID3D11Device* device, const std::string& fileName)
	{
		HRESULT res;

		int channels;
		stbi_set_flip_vertically_on_load(false);
		unsigned char* pixels = stbi_load(fileName.c_str(), &_width, &_height, &channels, 4);
		if (!pixels)
		{
			TENLog("Failed to load texture: " + fileName, LogLevel::Error);
			return;
		}

		int mipLevels = 1 + (int)std::floor(std::log2(std::max(_width, _height)));

		// Create texture with full mip chain.
		auto desc = D3D11_TEXTURE2D_DESC{};
		desc.Width = _width;
		desc.Height = _height;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
		desc.MipLevels = mipLevels;
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;

		res = device->CreateTexture2D(&desc, nullptr, _texture.GetAddressOf());
		throwIfFailed(res, device,
			"CreateTexture2D from file (" + std::to_string(_width) + "x" + std::to_string(_height) + "):");

		// Upload mip 0.
		ID3D11DeviceContext* context = nullptr;
		device->GetImmediateContext(&context);
		context->UpdateSubresource(_texture.Get(), 0, nullptr, pixels, _width * 4, 0);

		stbi_image_free(pixels);

		// Create SRV.
		auto srvDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = mipLevels;

		res = device->CreateShaderResourceView(_texture.Get(), &srvDesc, _shaderResourceView.GetAddressOf());
		throwIfFailed(res, device, "CreateSRV from file:");

		// Generate mipmaps on GPU.
		context->GenerateMips(_shaderResourceView.Get());
		context->Release();

		int vramSize = ComputeTextureSize(_width, _height, 1, mipLevels, DXGI_FORMAT_R8G8B8A8_UNORM);
		_vram = VRAMAllocation(VRAMCategory::Texture, vramSize,
			"Texture2D allocated: " + std::to_string(_width) + "x" + std::to_string(_height) +
			" RGBA8 mips=" + std::to_string(mipLevels) +
			" (" + BytesToMBString(vramSize) + " MB)");
	}

	// ======================================================================
	// Constructor 3: from memory (DDS or PNG via stb_image).
	// ======================================================================
	DX11Texture2D::DX11Texture2D(ID3D11Device* device, int dataSize, unsigned char* data)
	{
		HRESULT res;

		ID3D11DeviceContext* context = nullptr;
		device->GetImmediateContext(&context);

		// Check for DDS magic number "DDS ".
		if (dataSize >= 4 && data[0] == 'D' && data[1] == 'D' && data[2] == 'S' && data[3] == ' ')
		{
			if (dataSize < 128)
			{
				TENLog("DDS data too small", LogLevel::Error);
				context->Release();
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

			unsigned int fourCC = hdr->pfFourCC;
			DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
			int blockSize = 0;

			constexpr unsigned int FOURCC_DXT1 = 0x31545844;
			constexpr unsigned int FOURCC_DXT3 = 0x33545844;
			constexpr unsigned int FOURCC_DXT5 = 0x35545844;
			constexpr unsigned int FOURCC_DX10 = 0x30315844;

			if (fourCC == FOURCC_DX10)
			{
				if (dataSize < 148)
				{
					TENLog("DDS DX10 data too small", LogLevel::Error);
					context->Release();
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

				switch (dx10->dxgiFormat)
				{
				case 71: // DXGI_FORMAT_BC1_UNORM
					dxgiFormat = DXGI_FORMAT_BC1_UNORM;
					blockSize = 8;
					break;
				case 77: // DXGI_FORMAT_BC3_UNORM
					dxgiFormat = DXGI_FORMAT_BC3_UNORM;
					blockSize = 16;
					break;
				case 98: // DXGI_FORMAT_BC7_UNORM
					dxgiFormat = DXGI_FORMAT_BC7_UNORM;
					blockSize = 16;
					break;
				default:
					TENLog("Unsupported DX10 DXGI format: " + std::to_string(dx10->dxgiFormat), LogLevel::Error);
					context->Release();
					return;
				}
			}
			else
			{
				switch (fourCC)
				{
				case FOURCC_DXT1:
					dxgiFormat = DXGI_FORMAT_BC1_UNORM;
					blockSize = 8;
					break;
				case FOURCC_DXT3:
					dxgiFormat = DXGI_FORMAT_BC2_UNORM;
					blockSize = 16;
					break;
				case FOURCC_DXT5:
					dxgiFormat = DXGI_FORMAT_BC3_UNORM;
					blockSize = 16;
					break;
				default:
					TENLog("Unsupported DDS FourCC: " + std::to_string(fourCC), LogLevel::Error);
					context->Release();
					return;
				}
			}

			// Create immutable texture with subresource data for each mip.
			std::vector<D3D11_SUBRESOURCE_DATA> initData(mipCount);
			const unsigned char* mipPtr = pixelData;

			for (int mip = 0; mip < mipCount; mip++)
			{
				int mipWidth = std::max((int)_width >> mip, 1);
				int mipHeight = std::max((int)_height >> mip, 1);
				int blocksX = std::max((mipWidth + 3) / 4, 1);
				int blocksY = std::max((mipHeight + 3) / 4, 1);
				int mipSize = blocksX * blocksY * blockSize;

				if (mipPtr + mipSize > data + dataSize)
				{
					mipCount = mip;
					initData.resize(mipCount);
					break;
				}

				initData[mip].pSysMem = mipPtr;
				initData[mip].SysMemPitch = blocksX * blockSize;
				initData[mip].SysMemSlicePitch = 0;
				mipPtr += mipSize;
			}

			auto desc = D3D11_TEXTURE2D_DESC{};
			desc.Width = _width;
			desc.Height = _height;
			desc.Format = dxgiFormat;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.MipLevels = mipCount;
			desc.ArraySize = 1;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_IMMUTABLE;

			res = device->CreateTexture2D(&desc, initData.data(), _texture.GetAddressOf());
			throwIfFailed(res, device,
				"CreateTexture2D DDS (" + std::to_string(_width) + "x" + std::to_string(_height) +
				" " + DXGIFormatToString(dxgiFormat) + " mips=" + std::to_string(mipCount) + "):");

			auto srvDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
			srvDesc.Format = dxgiFormat;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = mipCount;

			res = device->CreateShaderResourceView(_texture.Get(), &srvDesc, _shaderResourceView.GetAddressOf());
			throwIfFailed(res, device, "CreateSRV for DDS:");

			int vramSize = ComputeTextureSize(_width, _height, 1, mipCount, dxgiFormat);
			_vram = VRAMAllocation(VRAMCategory::Texture, vramSize,
				"Texture2D DDS allocated: " + std::to_string(_width) + "x" + std::to_string(_height) +
				" " + DXGIFormatToString(dxgiFormat) + " mips=" + std::to_string(mipCount) +
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
				context->Release();
				return;
			}

			int mipLevels = 1 + (int)std::floor(std::log2(std::max(_width, _height)));

			auto desc = D3D11_TEXTURE2D_DESC{};
			desc.Width = _width;
			desc.Height = _height;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			desc.MipLevels = mipLevels;
			desc.ArraySize = 1;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;

			res = device->CreateTexture2D(&desc, nullptr, _texture.GetAddressOf());
			throwIfFailed(res, device,
				"CreateTexture2D PNG (" + std::to_string(_width) + "x" + std::to_string(_height) + "):");

			context->UpdateSubresource(_texture.Get(), 0, nullptr, pixels, _width * 4, 0);
			stbi_image_free(pixels);

			auto srvDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
			srvDesc.Format = desc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = mipLevels;

			res = device->CreateShaderResourceView(_texture.Get(), &srvDesc, _shaderResourceView.GetAddressOf());
			throwIfFailed(res, device, "CreateSRV for PNG from memory:");

			context->GenerateMips(_shaderResourceView.Get());

			int vramSize = ComputeTextureSize(_width, _height, 1, mipLevels, DXGI_FORMAT_R8G8B8A8_UNORM);
			_vram = VRAMAllocation(VRAMCategory::Texture, vramSize,
				"Texture2D PNG allocated: " + std::to_string(_width) + "x" + std::to_string(_height) +
				" mips=" + std::to_string(mipLevels) +
				" (" + BytesToMBString(vramSize) + " MB)");
		}

		context->Release();
	}
}

#endif
