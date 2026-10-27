#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <wrl/client.h>
#include "Renderer/Native/DirectX11/DX11TextureBase.h"
#include "Renderer/Native/DirectX11/DX11Texture2D.h"
#include "Renderer/Graphics/IRenderTarget2D.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;
	using Microsoft::WRL::ComPtr;

	// NOTE: Texture array is supported and so it's possible to have multiple views.
	// In most situations, however, the vector of the views is just one element.
	class DX11RenderTarget2D : public IRenderTarget2D
	{
	private:
		int                                         _width              = 0;
		int                                         _height             = 0;
		ComPtr<ID3D11Texture2D>                     _texture            = {};
		ComPtr<ID3D11ShaderResourceView>            _shaderResourceView = {};
		std::vector<ComPtr<ID3D11RenderTargetView>> _renderTargetViews  = {};
		VRAMAllocation                              _vram               = {};

	public:
		DX11RenderTarget2D() = default;
		~DX11RenderTarget2D() = default;

		int                       GetWidth() override { return _width; }
		int                       GetHeight() override { return _height; }
		int                       GetArraySize() override { return (int)_renderTargetViews.size(); }
		ID3D11RenderTargetView*   GetD3D11RenderTargetView(int arrayIndex) const noexcept { return _renderTargetViews[arrayIndex].Get(); }
		ID3D11RenderTargetView*   GetD3D11RenderTargetView() const noexcept { return GetD3D11RenderTargetView(0); }
		ID3D11ShaderResourceView* GetD3D11ShaderResourceView() const noexcept { return _shaderResourceView.Get(); }
		ID3D11Texture2D*          GetD3D11Texture() const noexcept { return _texture.Get(); }
		bool                      IsValid() override { return _texture != nullptr; }

		DX11RenderTarget2D(ID3D11Device* device, int width, int height, DXGI_FORMAT colorFormat, bool isTypeless);
		DX11RenderTarget2D(ID3D11Device* device, ID3D11Texture2D* texture, DXGI_FORMAT colorFormat);
		DX11RenderTarget2D(ID3D11Device* device, ID3D11Texture2D* textureRaw);
		DX11RenderTarget2D(ID3D11Device* device, int width, int height, int count, DXGI_FORMAT colorFormat);

	private:
		static DXGI_FORMAT MakeTypeless(DXGI_FORMAT format);
	};
}

#endif
