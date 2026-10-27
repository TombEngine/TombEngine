#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <wrl/client.h>
#include "Renderer/Graphics/ITextureBase.h"

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;
	
	using Microsoft::WRL::ComPtr;
	
	class DX11TextureBase : public virtual ITextureBase
	{
	protected:
		int _width;
		int _height;
		ComPtr<ID3D11ShaderResourceView> _shaderResourceView;

	public:
		DX11TextureBase() = default;
		~DX11TextureBase() = default;

		ID3D11ShaderResourceView* GetD3D11ShaderResourceView() const noexcept { return _shaderResourceView.Get(); }
	};
}

#endif
