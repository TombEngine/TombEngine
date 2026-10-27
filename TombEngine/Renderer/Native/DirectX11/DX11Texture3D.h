#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <wrl/client.h>
#include "Renderer/Graphics/ITexture3D.h"

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;
	using Microsoft::WRL::ComPtr;

	class DX11Texture3D : public ITexture3D
	{
	private:
		int                              _width  = 0;
		int                              _height = 0;
		int                              _depth  = 0;
		ComPtr<ID3D11Texture3D>          _texture;
		ComPtr<ID3D11ShaderResourceView> _shaderResourceView;

	public:
		DX11Texture3D() = default;
		~DX11Texture3D() = default;

		DX11Texture3D(ID3D11Device* device, int width, int height, int depth,
			DXGI_FORMAT format, const void* data);

		int  GetWidth() override  { return _width; }
		int  GetHeight() override { return _height; }
		int  GetDepth() override  { return _depth; }
		bool IsValid() override   { return _texture != nullptr; }

		ID3D11Texture3D*          GetD3D11Texture() const noexcept            { return _texture.Get(); }
		ID3D11ShaderResourceView* GetD3D11ShaderResourceView() const noexcept { return _shaderResourceView.Get(); }
	};
}

#endif
