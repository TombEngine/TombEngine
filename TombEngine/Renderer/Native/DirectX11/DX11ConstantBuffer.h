#pragma once

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Graphics/IConstantBuffer.h"
#include <d3d11.h>
#include <wrl/client.h>
#include "Game/Debug/Debug.h"

namespace TEN::Renderer::Native::DirectX11
{
	using Microsoft::WRL::ComPtr;

	using namespace TEN::Debug;
	using namespace TEN::Renderer::Graphics;

	class DX11ConstantBuffer : public IConstantBuffer
	{
	private:
		ComPtr<ID3D11Buffer> _buffer = {};
		int                  _size   = 0;

	public:
		DX11ConstantBuffer() = default;
		~DX11ConstantBuffer() = default;

		ID3D11Buffer* GetD3D11Buffer() const noexcept { return _buffer.Get(); }

		DX11ConstantBuffer(ID3D11Device* device, int size, std::wstring name);
		void UpdateData(void* data, ID3D11DeviceContext* ctx);
	};
}

#endif
