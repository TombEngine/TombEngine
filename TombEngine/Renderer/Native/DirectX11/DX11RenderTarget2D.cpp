#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11RenderTarget2D.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"
#include "Renderer/Native/DirectX11/DX11Utils.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Native::DirectX11
{
	// Default constructor.
	DX11RenderTarget2D::DX11RenderTarget2D(ID3D11Device* device, int width, int height, DXGI_FORMAT colorFormat, bool isTypeless)
	{
		HRESULT res;

		_width = width;
		_height = height;

		auto sizeStr = std::to_string(width) + "x" + std::to_string(height) + " " + DXGIFormatToString(colorFormat);

		// Texture.
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

		res = device->CreateTexture2D(&desc, nullptr, _texture.GetAddressOf());
		throwIfFailed(res, device, "CreateTexture2D for RenderTarget (" + sizeStr + "):");

		// Render target view.
		auto renderTargetViewDesc = D3D11_RENDER_TARGET_VIEW_DESC{};
		renderTargetViewDesc.Format = colorFormat;
		renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice = 0;

		auto renderTargetView = ComPtr<ID3D11RenderTargetView>{};
		res = device->CreateRenderTargetView(_texture.Get(), &renderTargetViewDesc, renderTargetView.GetAddressOf());
		throwIfFailed(res, device, "CreateRTV for RenderTarget (" + sizeStr + "):");
		_renderTargetViews.push_back(renderTargetView); // copy -> AddRef (ok)

		// Shader resource view.
		auto shaderResourceViewDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
		shaderResourceViewDesc.Format = colorFormat;
		shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;

		_shaderResourceView.Reset();
		res = device->CreateShaderResourceView(_texture.Get(), &shaderResourceViewDesc, _shaderResourceView.GetAddressOf());
		throwIfFailed(res, device, "CreateSRV for RenderTarget (" + sizeStr + "):");

		int vramSize = ComputeTextureSize(width, height, 1, 1, colorFormat);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, vramSize,
			"RenderTarget allocated: " + sizeStr +
			" (" + BytesToMBString(vramSize) + " MB)");
	}

	// Used by SMAA because it needs to have two render targets with the same texture.
	DX11RenderTarget2D::DX11RenderTarget2D(ID3D11Device* device, ID3D11Texture2D* texture, DXGI_FORMAT colorFormat)
	{
		HRESULT res;

		// Copy ComPtr from parent.
		texture->AddRef();
		_texture.Attach(texture);

		// RTV with different format.
		auto renderTargetViewDesc = D3D11_RENDER_TARGET_VIEW_DESC{};
		renderTargetViewDesc.Format = colorFormat;
		renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice = 0;

		auto renderTargetView = ComPtr<ID3D11RenderTargetView>{};
		res = device->CreateRenderTargetView(_texture.Get(), &renderTargetViewDesc, renderTargetView.GetAddressOf());
		throwIfFailed(res, device, "CreateRTV for SMAA RenderTarget (" + DXGIFormatToString(colorFormat) + "):");
		_renderTargetViews.push_back(renderTargetView);

		// Shader resource view.
		auto shaderResourceViewDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
		shaderResourceViewDesc.Format = colorFormat;
		shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;

		_shaderResourceView.Reset();
		res = device->CreateShaderResourceView(_texture.Get(), &shaderResourceViewDesc, _shaderResourceView.GetAddressOf());
		throwIfFailed(res, device, "CreateSRV for SMAA RenderTarget (" + DXGIFormatToString(colorFormat) + "):");
	}

	// Constructor for the backbuffer only.
	DX11RenderTarget2D::DX11RenderTarget2D(ID3D11Device* device, ID3D11Texture2D* textureRaw)
	{
		HRESULT res;

		// Copy ComPtr from parent.
		textureRaw->AddRef();
		_texture.Attach(textureRaw);

		// Render target view.
		auto texDesc = D3D11_TEXTURE2D_DESC{};
		_texture->GetDesc(&texDesc);

		_width = texDesc.Width;
		_height = texDesc.Height;

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = texDesc.Format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		auto renderTargetView = ComPtr<ID3D11RenderTargetView>{};
		res = device->CreateRenderTargetView(_texture.Get(), &rtvDesc, renderTargetView.GetAddressOf());
		throwIfFailed(res, device,
			"CreateRTV for backbuffer (" + std::to_string(_width) + "x" + std::to_string(_height) + "):");
		_renderTargetViews.push_back(renderTargetView);

		// Shader resource view.
		if (texDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
		{
			auto shaderResourceViewDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
			shaderResourceViewDesc.Format = texDesc.Format;
			shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
			shaderResourceViewDesc.Texture2D.MipLevels = 1;

			_shaderResourceView.Reset();
			res = device->CreateShaderResourceView(_texture.Get(), &shaderResourceViewDesc, _shaderResourceView.GetAddressOf());
			throwIfFailed(res, device,
				"CreateSRV for backbuffer (" + std::to_string(_width) + "x" + std::to_string(_height) + "):");
		}
	}

	// Texture array.
	DX11RenderTarget2D::DX11RenderTarget2D(ID3D11Device* device, int width, int height, int count, DXGI_FORMAT colorFormat)
	{
		HRESULT res;

		_width = width;
		_height = height;

		auto sizeStr = std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(count) +
			" " + DXGIFormatToString(colorFormat);

		auto desc = D3D11_TEXTURE2D_DESC{};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = count;
		desc.Format = colorFormat;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0x0;

		res = device->CreateTexture2D(&desc, NULL, _texture.GetAddressOf());
		throwIfFailed(res, device, "CreateTexture2D for RenderTarget array (" + sizeStr + "):");

		// Render target view.
		auto viewDesc = D3D11_RENDER_TARGET_VIEW_DESC{};
		viewDesc.Format = desc.Format;
		viewDesc.Texture2DArray.ArraySize = 1;
		viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;

		for (int i = 0; i < count; i++)
		{
			auto renderTargetView = ComPtr<ID3D11RenderTargetView>{};
			viewDesc.Texture2DArray.FirstArraySlice = D3D11CalcSubresource(0, i, 1);
			res = device->CreateRenderTargetView(_texture.Get(), &viewDesc, renderTargetView.GetAddressOf());
			throwIfFailed(res, device,
				"CreateRTV slice " + std::to_string(i) + " for RenderTarget array (" + sizeStr + "):");
			_renderTargetViews.push_back(renderTargetView);
		}

		// Shader resource view.
		auto shaderDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
		shaderDesc.Format = desc.Format;
		shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		shaderDesc.Texture2DArray.MostDetailedMip = 0;
		shaderDesc.Texture2DArray.MipLevels = 1;
		shaderDesc.Texture2DArray.ArraySize = count;
		shaderDesc.Texture2DArray.FirstArraySlice = 0;

		res = device->CreateShaderResourceView(_texture.Get(), &shaderDesc, _shaderResourceView.GetAddressOf());
		throwIfFailed(res, device, "CreateSRV for RenderTarget array (" + sizeStr + "):");

		int vramSize = ComputeTextureSize(width, height, count, 1, colorFormat);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, vramSize,
			"RenderTarget array allocated: " + sizeStr +
			" (" + BytesToMBString(vramSize) + " MB)");
	}

	DXGI_FORMAT DX11RenderTarget2D::MakeTypeless(DXGI_FORMAT format)
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

		default:
			return format;
		}
	}
}

#endif
