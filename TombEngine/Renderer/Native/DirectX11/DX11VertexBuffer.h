#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <wrl/client.h>
#include "Renderer/Graphics/Vertices/Vertex.h"
#include "Renderer/Graphics/IVertexBuffer.h"
#include "Renderer/Graphics/VRAMAllocation.h"
#include "Specific/fast_vector.h"

using namespace TEN::Renderer::Graphics::Vertices;

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;

	using Microsoft::WRL::ComPtr;

	class DX11VertexBuffer final : public IVertexBuffer
	{
	private:
		int                  _numVertices;
		ComPtr<ID3D11Buffer> _buffer;
		int                  _stride;
		VRAMAllocation       _vram;

	public:
		DX11VertexBuffer() = default;
		~DX11VertexBuffer() = default;

		ID3D11Buffer* GetD3D11Buffer() const { return _buffer.Get(); }
		int GetStride() const { return _stride; }

		DX11VertexBuffer(ID3D11Device* device, int numVertices, int stride, void* vertices);

		bool Update(ID3D11DeviceContext* context, void* data, int startVertex, int count);
	};
}

#endif
