#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "Renderer/Graphics/TextureBase.h"
#include "Renderer/RendererUtils.h"

namespace TEN::Renderer::Graphics
{
	using namespace TEN::Renderer::Utils;

	using Microsoft::WRL::ComPtr;

	class RenderTarget2D : public TextureBase
	{
	public:
		ComPtr<ID3D11RenderTargetView>	 RenderTargetView;
		ComPtr<ID3D11Texture2D>			 Texture;
		ComPtr<ID3D11DepthStencilView>	 DepthStencilView;
		ComPtr<ID3D11Texture2D>			 DepthStencilTexture;
		ComPtr<ID3D11ShaderResourceView> DepthShaderResourceView;

		RenderTarget2D() {};

		RenderTarget2D(ID3D11Device* device, int width, int height, DXGI_FORMAT colorFormat, bool isTypeless, DXGI_FORMAT depthFormat)
		{
			auto srvDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			auto dsvDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			auto rtvDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

			// Set up render target.
			auto desc = D3D11_TEXTURE2D_DESC{};
			desc.Width = width;
			desc.Height = height;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = isTypeless ? MakeTypeless(colorFormat) : colorFormat;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			auto res = device->CreateTexture2D(&desc, nullptr, &Texture);
			throwIfFailed(res);

			auto viewDesc = D3D11_RENDER_TARGET_VIEW_DESC{};
			viewDesc.Format = colorFormat;
			viewDesc.ViewDimension = rtvDimension;
			viewDesc.Texture2D.MipSlice = 0;

			res = device->CreateRenderTargetView(Texture.Get(), &viewDesc, &RenderTargetView);
			throwIfFailed(res);

			// Set up description of shader resource view.
			auto shaderDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
			shaderDesc.Format = colorFormat;
			shaderDesc.ViewDimension = srvDimension;
			shaderDesc.Texture2D.MostDetailedMip = 0;
			shaderDesc.Texture2D.MipLevels = 1;

			res = device->CreateShaderResourceView(Texture.Get(), &shaderDesc, &ShaderResourceView);
			throwIfFailed(res);

			if (depthFormat != DXGI_FORMAT_UNKNOWN)
			{
				auto depthTexDesc = D3D11_TEXTURE2D_DESC{};
				depthTexDesc.Width = width;
				depthTexDesc.Height = height;
				depthTexDesc.MipLevels = 1;
				depthTexDesc.ArraySize = 1;
				depthTexDesc.SampleDesc.Count = 1;
				depthTexDesc.SampleDesc.Quality = 0;
				depthTexDesc.Format = depthFormat;
				depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
				depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
				depthTexDesc.CPUAccessFlags = 0;
				depthTexDesc.MiscFlags = 0;

				res = device->CreateTexture2D(&depthTexDesc, NULL, &DepthStencilTexture);
				throwIfFailed(res);

				auto dsvDesc = D3D11_DEPTH_STENCIL_VIEW_DESC{};
				dsvDesc.Format = depthTexDesc.Format;
				dsvDesc.Flags = 0;
				dsvDesc.ViewDimension = dsvDimension;
				dsvDesc.Texture2D.MipSlice = 0;

				res = device->CreateDepthStencilView(DepthStencilTexture.Get(), &dsvDesc, &DepthStencilView);
				throwIfFailed(res);
			}
		}

		// Constructor is for sharing same texture resource of another render target.
		// Used in SMAA technique to have one UNORM and one SRGB render target, but using same texture.
		RenderTarget2D(ID3D11Device* device, RenderTarget2D* parent, DXGI_FORMAT colorFormat)
		{
			auto desc = D3D11_TEXTURE2D_DESC{};
			parent->Texture.Get()->GetDesc(&desc);
			int width = desc.Width;
			int height = desc.Height;
			parent->Texture.CopyTo(Texture.GetAddressOf());

			auto viewDesc = D3D11_RENDER_TARGET_VIEW_DESC{};
			viewDesc.Format = colorFormat;
			viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			viewDesc.Texture2D.MipSlice = 0;

			auto res = device->CreateRenderTargetView(Texture.Get(), &viewDesc, &RenderTargetView);
			throwIfFailed(res);

			// Set up description of shader resource view.
			auto shaderDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
			shaderDesc.Format = colorFormat;
			shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shaderDesc.Texture2D.MostDetailedMip = 0;
			shaderDesc.Texture2D.MipLevels = 1;

			res = device->CreateShaderResourceView(Texture.Get(), &shaderDesc, &ShaderResourceView);
			throwIfFailed(res);
		}

		RenderTarget2D(ID3D11Device* device, ID3D11Texture2D* texture, DXGI_FORMAT depthFormat)
		{
			D3D11_SRV_DIMENSION srvDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			D3D11_DSV_DIMENSION dsvDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			D3D11_RTV_DIMENSION rtvDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

			// Now set up render target.
			Texture = texture;

			D3D11_TEXTURE2D_DESC desc = {};
			texture->GetDesc(&desc);

			D3D11_RENDER_TARGET_VIEW_DESC viewDesc;
			viewDesc.Format = desc.Format;
			viewDesc.ViewDimension = rtvDimension;
			viewDesc.Texture2D.MipSlice = 0;

			HRESULT res = device->CreateRenderTargetView(Texture.Get(), &viewDesc, &RenderTargetView);
			throwIfFailed(res);

			// Setup the description of the shader resource view.
			if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc;
				shaderDesc.Format = desc.Format;
				shaderDesc.ViewDimension = srvDimension;
				shaderDesc.Texture2D.MostDetailedMip = 0;
				shaderDesc.Texture2D.MipLevels = 1;

				res = device->CreateShaderResourceView(Texture.Get(), &shaderDesc, &ShaderResourceView);
				throwIfFailed(res);
			}

			if (depthFormat != DXGI_FORMAT_UNKNOWN)
			{
				D3D11_TEXTURE2D_DESC depthTexDesc = {};
				depthTexDesc.Width = desc.Width;
				depthTexDesc.Height = desc.Height;
				depthTexDesc.MipLevels = 1;
				depthTexDesc.ArraySize = 1;
				depthTexDesc.SampleDesc.Count = 1;
				depthTexDesc.SampleDesc.Quality = 0;
				depthTexDesc.Format = depthFormat;
				depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
				depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
				depthTexDesc.CPUAccessFlags = 0;
				depthTexDesc.MiscFlags = 0;

				res = device->CreateTexture2D(&depthTexDesc, NULL, &DepthStencilTexture);
				throwIfFailed(res);

				D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
				dsvDesc.Format = depthTexDesc.Format;
				dsvDesc.Flags = 0;
				dsvDesc.ViewDimension = dsvDimension;
				dsvDesc.Texture2D.MipSlice = 0;

				res = device->CreateDepthStencilView(DepthStencilTexture.Get(), &dsvDesc, &DepthStencilView);
				throwIfFailed(res);
			}
		}

	private:
		DXGI_FORMAT MakeTypeless(DXGI_FORMAT format)
		{
			switch (format)
			{
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UINT:
			case DXGI_FORMAT_R8G8B8A8_SNORM:
			case DXGI_FORMAT_R8G8B8A8_SINT:
				return DXGI_FORMAT_R8G8B8A8_TYPELESS;

			case DXGI_FORMAT_BC1_UNORM_SRGB:
			case DXGI_FORMAT_BC1_UNORM:
				return DXGI_FORMAT_BC1_TYPELESS;

			case DXGI_FORMAT_BC2_UNORM_SRGB:
			case DXGI_FORMAT_BC2_UNORM:
				return DXGI_FORMAT_BC2_TYPELESS;

			case DXGI_FORMAT_BC3_UNORM_SRGB:
			case DXGI_FORMAT_BC3_UNORM:
				return DXGI_FORMAT_BC3_TYPELESS;
			};

			return format;
		}
	};
}
