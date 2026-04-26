#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11Texture2D.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"
#include "Renderer/Native/DirectX11/DX11Utils.h"
#include "Specific/trutils.h"

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

	DX11Texture2D::DX11Texture2D(ID3D11Device* device, const std::wstring& fileName)
	{
		HRESULT res;

		ComPtr<ID3D11Resource> resource;
		ID3D11DeviceContext* context = nullptr;
		device->GetImmediateContext(&context);

		res = CreateWICTextureFromFile(device, context, fileName.c_str(), resource.GetAddressOf(), _shaderResourceView.GetAddressOf(), (size_t)0);
		throwIfFailed(res, L"Opening Texture file '" + fileName + L"': ");

		res = resource->QueryInterface(_texture.GetAddressOf());
		throwIfFailed(res, device, "QueryInterface for Texture2D from file:");

		D3D11_TEXTURE2D_DESC desc;
		_texture->GetDesc(&desc);

		_width = desc.Width;
		_height = desc.Height;

		int vramSize = ComputeTextureSize(desc.Width, desc.Height, desc.ArraySize, desc.MipLevels, desc.Format);
		_vram = VRAMAllocation(VRAMCategory::Texture, vramSize,
			"Texture2D allocated: " + std::to_string(desc.Width) + "x" + std::to_string(desc.Height) +
			" " + DXGIFormatToString(desc.Format) + " mips=" + std::to_string(desc.MipLevels) +
			" (" + BytesToMBString(vramSize) + " MB)");
	}

	DX11Texture2D::DX11Texture2D(ID3D11Device* device, int dataSize, unsigned char* data)
	{
		HRESULT res;

		ComPtr<ID3D11Resource> resource;
		ID3D11DeviceContext* context = nullptr;
		device->GetImmediateContext(&context);

		if (data[0] == 0x44 && data[1] == 0x44 && data[2] == 0x53)
		{
			// DDS texture (mipmaps are pre-authored by the level editor).
			res = CreateDDSTextureFromMemory(
				device,
				data,
				dataSize,
				resource.GetAddressOf(),
				_shaderResourceView.GetAddressOf());
			throwIfFailed(res, device, "CreateDDSTextureFromMemory (" + std::to_string(dataSize) + " bytes):");
		}
		else
		{
			// PNG legacy texture (mipmaps generated on GPU).
			res = CreateWICTextureFromMemory(
				device,
				context,
				data,
				dataSize,
				resource.GetAddressOf(),
				_shaderResourceView.GetAddressOf());
			throwIfFailed(res, device, "CreateWICTextureFromMemory (" + std::to_string(dataSize) + " bytes):");

			context->GenerateMips(_shaderResourceView.Get());
		}

		res = resource->QueryInterface(_texture.GetAddressOf());
		throwIfFailed(res, device, "QueryInterface for Texture2D from memory:");

		D3D11_TEXTURE2D_DESC desc;
		_texture->GetDesc(&desc);

		_width = desc.Width;
		_height = desc.Height;

		int vramSize = ComputeTextureSize(desc.Width, desc.Height, desc.ArraySize, desc.MipLevels, desc.Format);
		_vram = VRAMAllocation(VRAMCategory::Texture, vramSize,
			"Texture2D allocated: " + std::to_string(desc.Width) + "x" + std::to_string(desc.Height) +
			" " + DXGIFormatToString(desc.Format) + " mips=" + std::to_string(desc.MipLevels) +
			" (" + BytesToMBString(vramSize) + " MB)");
	}
}

#endif
