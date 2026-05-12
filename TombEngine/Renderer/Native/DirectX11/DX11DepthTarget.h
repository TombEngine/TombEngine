#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <wrl/client.h>
#include "Renderer/Graphics/IDepthTarget.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;

	using Microsoft::WRL::ComPtr;

	// NOTE: Texture array is supported and so it's possible to have multiple views.
	// In most situations, however, the vector of the views is just one element.
	class DX11DepthTarget : public IDepthTarget
	{
	private:
		int                                         _width               = 0;
		int                                         _height              = 0;
		DepthFormat                                 _format              = DepthFormat::None;
		std::vector<ComPtr<ID3D11DepthStencilView>> _depthStencilViews   = {};
		ComPtr<ID3D11Texture2D>	                    _depthStencilTexture = {};
		VRAMAllocation                              _vram                = {};

	public:
		DX11DepthTarget() = default;
		~DX11DepthTarget() = default;

		int         GetArraySize() override { return (int)_depthStencilViews.size(); }
		DepthFormat GetFormat()    override { return _format; }

		ID3D11DepthStencilView* GetD3D11DepthStencilView(int arrayIndex) const noexcept { return _depthStencilViews[arrayIndex].Get(); }
		ID3D11DepthStencilView* GetD3D11DepthStencilView()               const noexcept { return GetD3D11DepthStencilView(0); }
		ID3D11Texture2D*	    GetD3D11Texture()						 const noexcept { return _depthStencilTexture.Get(); }

		DX11DepthTarget(ID3D11Device* device, int width, int height, DepthFormat format, DXGI_FORMAT depthFormat);
		DX11DepthTarget(ID3D11Device* device, int width, int height, int count, DepthFormat format, DXGI_FORMAT depthFormat);
	};
}

#endif
