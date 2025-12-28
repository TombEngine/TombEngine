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

		DX11Texture2D(ID3D11Device* device, int width, int height, byte* data);
		DX11Texture2D(ID3D11Device* device, int width, int height, DXGI_FORMAT format);
		DX11Texture2D(ID3D11Device* device, int width, int height, DXGI_FORMAT format, int pitch, const void* data);
		DX11Texture2D(ID3D11Device* device, const std::wstring& fileName);
		DX11Texture2D(ID3D11Device* device, byte* data, int length);
	};
}

#endif
