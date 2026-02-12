#pragma once
#include <d3d11.h>
#include <string>
#include <wrl/client.h>
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
#include "Renderer/Graphics/TextureBase.h"
#include "Renderer/RendererUtils.h"

namespace TEN::Renderer::Graphics
{
	using namespace TEN::Renderer::Utils;
	using namespace DirectX;

	using Microsoft::WRL::ComPtr;

	class Texture2D : public TextureBase
	{
	public:
		int Width;
		int Height;

		ComPtr<ID3D11Texture2D> Texture;

		Texture2D() = default;
		
		Texture2D(ID3D11Device* device, int width, int height, byte* data)
		{
			Width = width;
			Height = height;

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
			throwIfFailed(device->CreateTexture2D(&desc, &subresourceData, &Texture));

			auto shaderDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
			shaderDesc.Format = desc.Format;
			shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shaderDesc.Texture2D.MostDetailedMip = 0;
			shaderDesc.Texture2D.MipLevels = 1;
			throwIfFailed(device->CreateShaderResourceView(Texture.Get(), &shaderDesc, ShaderResourceView.GetAddressOf()));
		}

		Texture2D(ID3D11Device* device, int width, int height, DXGI_FORMAT format, int pitch, const void* data)
		{
			Width = width;
			Height = height;

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

			throwIfFailed(device->CreateTexture2D(&desc, &subresourceData, &Texture));

			auto shaderDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
			shaderDesc.Format = desc.Format;
			shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shaderDesc.Texture2D.MostDetailedMip = 0;
			shaderDesc.Texture2D.MipLevels = 1;

			throwIfFailed(device->CreateShaderResourceView(Texture.Get(), &shaderDesc, ShaderResourceView.GetAddressOf()));
		}

		Texture2D(ID3D11Device* device, const std::wstring& fileName)
		{
			ComPtr<ID3D11Resource> resource;
			ID3D11DeviceContext* context = nullptr;
			device->GetImmediateContext(&context);

			throwIfFailed(CreateWICTextureFromFile(device, context, fileName.c_str(), resource.GetAddressOf(), ShaderResourceView.GetAddressOf(), (size_t)0), L"Opening Texture file '" + fileName + L"': ");
			throwIfFailed(resource->QueryInterface(Texture.GetAddressOf()));

			D3D11_TEXTURE2D_DESC desc;
			Texture->GetDesc(&desc);
			Width = desc.Width;
			Height = desc.Height;
		}

		Texture2D(ID3D11Device* device, byte* data, int length)
		{
			ComPtr<ID3D11Resource> resource;
			ID3D11DeviceContext* context = nullptr;
			device->GetImmediateContext(&context);

			if (data[0] == 0x44 && data[1] == 0x44 && data[2] == 0x53)
			{
				// DDS texture
				throwIfFailed(CreateDDSTextureFromMemory(
					device,
					context,
					data,
					length,
					resource.GetAddressOf(),
					ShaderResourceView.GetAddressOf()));
			}
			else
			{
				// PNG legacy texture
				throwIfFailed(CreateWICTextureFromMemory(
					device,
					context,
					data,
					length,
					resource.GetAddressOf(),
					ShaderResourceView.GetAddressOf()));
			}

			context->GenerateMips(ShaderResourceView.Get());

			throwIfFailed(resource->QueryInterface(Texture.GetAddressOf()));

			D3D11_TEXTURE2D_DESC desc;
			Texture->GetDesc(&desc);
			Width = desc.Width;
			Height = desc.Height;
		}

		~Texture2D() = default;
	};
}
