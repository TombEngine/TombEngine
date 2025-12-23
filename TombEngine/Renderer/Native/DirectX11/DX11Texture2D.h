#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <wrl/client.h>
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
#include "Renderer/Native/DirectX11/DX11TextureBase.h"
#include "Renderer/RendererUtils.h"
#include "Renderer/Graphics/ITexture2D.h"

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Utils;
	using namespace TEN::Renderer::Graphics;
	using namespace DirectX;

	using Microsoft::WRL::ComPtr;

	class DX11Texture2D : public ITexture2D
	{
	protected:
		int _width;
		int _height;
		ComPtr<ID3D11Texture2D> _texture;
		ComPtr<ID3D11ShaderResourceView> _shaderResourceView;

	public:
		DX11Texture2D() = default;
		~DX11Texture2D() = default;
		
		int GetWidth() override { return _width; }
		int GetHeight() override { return _height; }
		ID3D11ShaderResourceView* GetD3D11ShaderResourceView() const noexcept { return _shaderResourceView.Get(); }
		ID3D11Texture2D* GetD3D11Texture() const noexcept { return _texture.Get(); }
		bool IsValid() override { return _texture != nullptr; }

		DX11Texture2D(ID3D11Device* device, int width, int height, byte* data)
		{
			HRESULT res;

			_width = width;
			_height = height;

			// Texture
			auto desc = D3D11_TEXTURE2D_DESC{};
			desc.Width = width;
			desc.Height = height;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DYNAMIC;

			auto subresourceData = D3D11_SUBRESOURCE_DATA{};
			subresourceData.pSysMem = data;
			subresourceData.SysMemPitch = width * 4;
			subresourceData.SysMemSlicePitch = 0;

			res = device->CreateTexture2D(&desc, &subresourceData, _texture.GetAddressOf());
			throwIfFailed(res);

			// SRV
			auto shaderDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
			shaderDesc.Format = desc.Format;
			shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shaderDesc.Texture2D.MostDetailedMip = 0;
			shaderDesc.Texture2D.MipLevels = 1;
			res = device->CreateShaderResourceView(_texture.Get(), &shaderDesc, _shaderResourceView.GetAddressOf());
			throwIfFailed(res);
		}

		DX11Texture2D(ID3D11Device* device, int width, int height, DXGI_FORMAT format)
		{
			HRESULT res;

			_width = width;
			_height = height;

			// Texture
			auto desc = D3D11_TEXTURE2D_DESC{};
			desc.Width = width;
			desc.Height = height;
			desc.Format = format;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			desc.MiscFlags = 0;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DYNAMIC;

			// SRV
			auto shaderDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
			shaderDesc.Format = desc.Format;
			shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shaderDesc.Texture2D.MostDetailedMip = 0;
			shaderDesc.Texture2D.MipLevels = 1;
			res = device->CreateShaderResourceView(_texture.Get(), &shaderDesc, _shaderResourceView.GetAddressOf());
			throwIfFailed(res);
		}

		DX11Texture2D(ID3D11Device* device, int width, int height, DXGI_FORMAT format, int pitch, const void* data)
		{
			HRESULT res;

			_width = width;
			_height = height;

			// Texture
			auto desc = D3D11_TEXTURE2D_DESC{};
			desc.Width = width;
			desc.Height = height;
			desc.Format = format;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;

			auto subresourceData = D3D11_SUBRESOURCE_DATA{};
			subresourceData.pSysMem = data;
			subresourceData.SysMemPitch = pitch;
			subresourceData.SysMemSlicePitch = 0;

			res = device->CreateTexture2D(&desc, &subresourceData, _texture.GetAddressOf());
			throwIfFailed(res);

			// SRV
			auto shaderDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
			shaderDesc.Format = desc.Format;
			shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shaderDesc.Texture2D.MostDetailedMip = 0;
			shaderDesc.Texture2D.MipLevels = 1;

			throwIfFailed(device->CreateShaderResourceView(_texture.Get(), &shaderDesc, _shaderResourceView.GetAddressOf()));
		}

		DX11Texture2D(ID3D11Device* device, const std::wstring& fileName)
		{
			HRESULT res;

			ComPtr<ID3D11Resource> resource;
			ID3D11DeviceContext* context = nullptr;
			device->GetImmediateContext(&context);

			res = CreateWICTextureFromFile(device, context, fileName.c_str(), resource.GetAddressOf(), _shaderResourceView.GetAddressOf(), (size_t)0);
			throwIfFailed(res, L"Opening Texture file '" + fileName + L"': ");

			res = resource->QueryInterface(_texture.GetAddressOf());
			throwIfFailed(res);

			D3D11_TEXTURE2D_DESC desc;
			_texture->GetDesc(&desc);

			_width = desc.Width;
			_height = desc.Height;
		}

		DX11Texture2D(ID3D11Device* device, byte* data, int length)
		{
			HRESULT res;

			ComPtr<ID3D11Resource> resource;
			ID3D11DeviceContext* context = nullptr;
			device->GetImmediateContext(&context);

			if (data[0] == 0x44 && data[1] == 0x44 && data[2] == 0x53)
			{
				// DDS texture
				res = CreateDDSTextureFromMemory(
					device,
					context,
					data,
					length,
					resource.GetAddressOf(),
					_shaderResourceView.GetAddressOf());
				throwIfFailed(res);
			}
			else
			{
				// PNG legacy texture
				res = CreateWICTextureFromMemory(
					device,
					context,
					data,
					length,
					resource.GetAddressOf(),
					_shaderResourceView.GetAddressOf());
				throwIfFailed(res);
			}

			context->GenerateMips(_shaderResourceView.Get());

			throwIfFailed(resource->QueryInterface(_texture.GetAddressOf()));

			D3D11_TEXTURE2D_DESC desc;
			_texture->GetDesc(&desc);
			
			_width = desc.Width;
			_height = desc.Height;
		}
	};
}

#endif
