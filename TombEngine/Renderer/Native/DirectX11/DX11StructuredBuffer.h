#pragma once

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Graphics/IStructuredBuffer.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <string>

namespace TEN::Renderer::Native::DirectX11
{
	using Microsoft::WRL::ComPtr;

	using namespace TEN::Renderer::Graphics;

	class DX11StructuredBuffer : public IStructuredBuffer
	{
	private:
		ComPtr<ID3D11Buffer>             _buffer       = {};
		ComPtr<ID3D11ShaderResourceView> _srv          = {};
		int                              _stride       = 0;
		int                              _elementCount = 0;

	public:
		DX11StructuredBuffer() = default;
		~DX11StructuredBuffer() = default;

		DX11StructuredBuffer(ID3D11Device* device, int stride, int elementCount, std::wstring name);

		ID3D11ShaderResourceView* GetD3D11ShaderResourceView() const noexcept { return _srv.Get(); }

		void UpdateData(const void* data, int elementCount, ID3D11DeviceContext* ctx);
	};
}

#endif
