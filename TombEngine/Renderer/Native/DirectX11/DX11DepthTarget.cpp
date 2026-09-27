#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11DepthTarget.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"
#include "Renderer/Native/DirectX11/DX11Utils.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11DepthTarget::DX11DepthTarget(ID3D11Device* device, int width, int height, DXGI_FORMAT depthFormat)
	{
		HRESULT res;

		_width = width;
		_height = height;

		auto sizeStr = std::to_string(width) + "x" + std::to_string(height) + " " + DXGIFormatToString(depthFormat);

		// Texture
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

		res = device->CreateTexture2D(&depthTexDesc, nullptr, &_depthStencilTexture);
		throwIfFailed(res, device, "CreateTexture2D for DepthTarget (" + sizeStr + "):");

		// DSV
		auto dsvDesc = D3D11_DEPTH_STENCIL_VIEW_DESC{};
		dsvDesc.Format = depthTexDesc.Format;
		dsvDesc.Flags = 0;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;

		ComPtr<ID3D11DepthStencilView> dsv;
		res = device->CreateDepthStencilView(_depthStencilTexture.Get(), &dsvDesc, &dsv);
		throwIfFailed(res, device, "CreateDSV for DepthTarget (" + sizeStr + "):");
		_depthStencilViews.push_back(dsv);

		int vramSize = ComputeTextureSize(width, height, 1, 1, depthFormat);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, vramSize,
			"DepthTarget allocated: " + sizeStr +
			" (" + BytesToMBString(vramSize) + " MB)");
	}

	DX11DepthTarget::DX11DepthTarget(ID3D11Device* device, int width, int height, int count, DXGI_FORMAT depthFormat)
	{
		HRESULT res;

		_width = width;
		_height = height;

		auto sizeStr = std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(count) +
			" " + DXGIFormatToString(depthFormat);

		// Texture
		D3D11_TEXTURE2D_DESC depthTexDesc = {};
		depthTexDesc.Width = width;
		depthTexDesc.Height = height;
		depthTexDesc.MipLevels = 1;
		depthTexDesc.ArraySize = count;
		depthTexDesc.SampleDesc.Count = 1;
		depthTexDesc.SampleDesc.Quality = 0;
		depthTexDesc.Format = depthFormat;
		depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
		depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthTexDesc.CPUAccessFlags = 0;
		depthTexDesc.MiscFlags = 0x0;

		res = device->CreateTexture2D(&depthTexDesc, nullptr, _depthStencilTexture.GetAddressOf());
		throwIfFailed(res, device, "CreateTexture2D for DepthTarget array (" + sizeStr + "):");

		// DSV
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = depthTexDesc.Format;
		dsvDesc.Flags = 0;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.ArraySize = 1;

		for (int i = 0; i < count; i++)
		{
			ComPtr<ID3D11DepthStencilView> dsv;
			dsvDesc.Texture2DArray.FirstArraySlice = D3D11CalcSubresource(0, i, 1);
			res = device->CreateDepthStencilView(_depthStencilTexture.Get(), &dsvDesc, dsv.GetAddressOf());
			throwIfFailed(res, device,
				"CreateDSV slice " + std::to_string(i) + " for DepthTarget array (" + sizeStr + "):");
			_depthStencilViews.push_back(dsv);
		}

		int vramSize = ComputeTextureSize(width, height, count, 1, depthFormat);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, vramSize,
			"DepthTarget array allocated: " + sizeStr +
			" (" + BytesToMBString(vramSize) + " MB)");
	}
}

#endif
