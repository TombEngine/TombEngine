#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include "Renderer/Graphics/TextureBase.h"
#include <vector>

namespace TEN::Renderer::Graphics
{
	using namespace TEN::Renderer::Utils;

	using Microsoft::WRL::ComPtr;

	class Texture2DArray : public TextureBase
	{
	public:

		std::vector<ComPtr<ID3D11RenderTargetView>> RenderTargetView;
		ComPtr<ID3D11Texture2D> Texture;
		std::vector<ComPtr<ID3D11DepthStencilView>> DepthStencilView;
		ComPtr<ID3D11Texture2D> DepthStencilTexture;
		int Resolution;
		D3D11_VIEWPORT Viewport;

		Texture2DArray() : Resolution(0), Viewport({}) {};

		Texture2DArray(ID3D11Device* device, int resolution, int count, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat)
			: Resolution(resolution)
		{
			DepthStencilView.resize(count);
			RenderTargetView.resize(count);
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = resolution;
			desc.Height = resolution;
			desc.MipLevels = 1;
			desc.ArraySize = count;
			desc.Format = colorFormat;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0x0;

			HRESULT res = device->CreateTexture2D(&desc, NULL, Texture.GetAddressOf());
			throwIfFailed(res);

			D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
			viewDesc.Format = desc.Format;
			viewDesc.Texture2DArray.ArraySize = 1;
			viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;

			for (int i = 0; i < count; i++)
			{
				viewDesc.Texture2DArray.FirstArraySlice = D3D11CalcSubresource(0, i, 1);
				res = device->CreateRenderTargetView(Texture.Get(), &viewDesc, RenderTargetView[i].GetAddressOf());
				throwIfFailed(res);
			}

			// Setup the description of the shader resource view.
			D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc = {};
			shaderDesc.Format = desc.Format;
			shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			shaderDesc.Texture2DArray.MostDetailedMip = 0;
			shaderDesc.Texture2DArray.MipLevels = 1;
			shaderDesc.Texture2DArray.ArraySize = count;
			shaderDesc.Texture2DArray.FirstArraySlice = 0;
			res = device->CreateShaderResourceView(Texture.Get(), &shaderDesc, ShaderResourceView.GetAddressOf());
			throwIfFailed(res);

			D3D11_TEXTURE2D_DESC depthTexDesc = {};
			depthTexDesc.Width = resolution;
			depthTexDesc.Height = resolution;
			depthTexDesc.MipLevels = 1;
			depthTexDesc.ArraySize = count;
			depthTexDesc.SampleDesc.Count = 1;
			depthTexDesc.SampleDesc.Quality = 0;
			depthTexDesc.Format = depthFormat;
			depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
			depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			depthTexDesc.CPUAccessFlags = 0;
			depthTexDesc.MiscFlags = 0x0;

			res = device->CreateTexture2D(&depthTexDesc, NULL, DepthStencilTexture.GetAddressOf());
			throwIfFailed(res);

			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = depthTexDesc.Format;
			dsvDesc.Flags = 0;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.ArraySize = 1;

			for (int i = 0; i < count; i++)
			{
				dsvDesc.Texture2DArray.FirstArraySlice = D3D11CalcSubresource(0, i, 1);
				res = device->CreateDepthStencilView(DepthStencilTexture.Get(), &dsvDesc, DepthStencilView[i].GetAddressOf());
				throwIfFailed(res);
			}
		}
	};
}
