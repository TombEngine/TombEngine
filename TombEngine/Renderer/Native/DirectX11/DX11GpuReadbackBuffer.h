#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <wrl/client.h>
#include "Renderer/Graphics/IGpuReadbackBuffer.h"

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;
	using Microsoft::WRL::ComPtr;

	class DX11GpuReadbackBuffer : public IGpuReadbackBuffer
	{
	private:
		ComPtr<ID3D11Device>        _device;
		ComPtr<ID3D11DeviceContext> _context;
		ComPtr<ID3D11Texture2D>     _stagingTextures[2];
		int                         _byteSize     = 0;
		int                         _writeIndex   = 0;
		int                         _submitCount  = 0;

	public:
		DX11GpuReadbackBuffer(ID3D11Device* device, ID3D11DeviceContext* context,
			int width, int height, DXGI_FORMAT format);
		~DX11GpuReadbackBuffer() = default;

		void SubmitCopy(ITexture2D* src) override;
		bool TryRead(void* outBytes, int byteCount) override;
	};
}

#endif
