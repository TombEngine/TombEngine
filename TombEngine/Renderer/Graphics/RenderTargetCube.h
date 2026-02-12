#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "Renderer/Graphics/TextureBase.h"
#include "Renderer/RendererUtils.h"

namespace TEN::Renderer::Graphics
{
	using namespace TEN::Renderer::Utils;

	using Microsoft::WRL::ComPtr;
	using std::array;

	class RenderTargetCube : public TextureBase
	{
	public:
		static constexpr Vector3 forwardVectors[6] = 
		{
			//+X (right)
			Vector3(-1,0,0),
			//-X (left)
			Vector3(1,0,0),
			//-Y (up)
			Vector3(0,-1,0),
			//+Y (down)
			Vector3(0,1,0),
			//+Z (forward)
			Vector3(0,0,1),
			//-Z (backward)
			Vector3(0,0,-1),
		};

		static constexpr Vector3 upVectors[6] = 
		{
			Vector3(0,-1,0),
			Vector3(0,-1,0),
			Vector3(0,0,-1),
			Vector3(0,0,1),
			Vector3(0,-1,0),
			Vector3(0,-1,0),
		};

		array<ComPtr<ID3D11RenderTargetView>, 6> RenderTargetView;
		ComPtr<ID3D11Texture2D> Texture;
		array<ComPtr<ID3D11DepthStencilView>, 6> DepthStencilView;
		ComPtr<ID3D11Texture2D> DepthStencilTexture;
		int Resolution;
		D3D11_VIEWPORT Viewport;

		RenderTargetCube() : Resolution(0), Viewport({}) {};
		
		RenderTargetCube(ID3D11Device* device, int resolution, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat)
			: Resolution(resolution)
		{

			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = resolution;
			desc.Height = resolution;
			desc.MipLevels = 1;
			desc.ArraySize = 6;
			desc.Format = colorFormat;
			desc.SampleDesc.Count = 1;
			desc.SampleDesc.Quality = 0;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

			HRESULT res = device->CreateTexture2D(&desc, NULL, Texture.GetAddressOf());
			throwIfFailed(res);

			D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
			viewDesc.Format = desc.Format;
			viewDesc.Texture2DArray.ArraySize = 1;
			viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;

			for (int i = 0; i < 6; i++)
			{
				viewDesc.Texture2DArray.FirstArraySlice = D3D11CalcSubresource(0, i, 1);
				res = device->CreateRenderTargetView(Texture.Get(), &viewDesc, RenderTargetView[i].GetAddressOf());
				throwIfFailed(res);
			}

			// Setup the description of the shader resource view.
			D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc = {};
			shaderDesc.Format = desc.Format;
			shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
			shaderDesc.Texture2DArray.MostDetailedMip = 0;
			shaderDesc.Texture2DArray.MipLevels = 1;
			shaderDesc.Texture2DArray.ArraySize = 6;
			shaderDesc.Texture2DArray.FirstArraySlice = 0;
			res = device->CreateShaderResourceView(Texture.Get(), &shaderDesc, ShaderResourceView.GetAddressOf());
			throwIfFailed(res);

			D3D11_TEXTURE2D_DESC depthTexDesc = {};
			depthTexDesc.Width = resolution;
			depthTexDesc.Height = resolution;
			depthTexDesc.MipLevels = 1;
			depthTexDesc.ArraySize = 6;
			depthTexDesc.SampleDesc.Count = 1;
			depthTexDesc.SampleDesc.Quality = 0;
			depthTexDesc.Format = depthFormat;
			depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
			depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			depthTexDesc.CPUAccessFlags = 0;
			depthTexDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

			res = device->CreateTexture2D(&depthTexDesc, NULL, DepthStencilTexture.GetAddressOf());
			throwIfFailed(res);

			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = depthTexDesc.Format;
			dsvDesc.Flags = 0;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.ArraySize = 1;

			for (int i = 0; i < 6; i++)
			{
				dsvDesc.Texture2DArray.FirstArraySlice = D3D11CalcSubresource(0, i, 1);
				res = device->CreateDepthStencilView(DepthStencilTexture.Get(), &dsvDesc, DepthStencilView[i].GetAddressOf());
				throwIfFailed(res);
			}

		}
	};
}
