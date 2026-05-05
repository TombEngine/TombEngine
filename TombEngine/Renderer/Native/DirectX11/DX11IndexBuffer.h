#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <wrl/client.h>
#include "Specific/fast_vector.h"
#include "Game/Debug/Debug.h"
#include "Renderer/Graphics/IIndexBuffer.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::DirectX11
{
	using Microsoft::WRL::ComPtr;

	using namespace TEN::Renderer::Graphics;
	using namespace TEN::Debug;

	class DX11IndexBuffer final : public IIndexBuffer
	{
	private:
		int _numIndices = 0;
		ComPtr<ID3D11Buffer> _buffer;
		VRAMAllocation _vram;

	public:
		DX11IndexBuffer() = default;
		~DX11IndexBuffer() = default;

		ID3D11Buffer* GetD3D11Buffer() const { return _buffer.Get(); }

		DX11IndexBuffer(ID3D11Device* device, int numIndices, int* indices);

		bool Update(ID3D11DeviceContext* context, int* data, int startIndex, int count);
	};
}

#endif
