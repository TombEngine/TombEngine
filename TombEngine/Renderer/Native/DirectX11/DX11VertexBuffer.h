#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include "Renderer/RendererUtils.h"
#include "Renderer/Graphics/Vertices/Vertex.h"
#include "Renderer/Graphics/IVertexBuffer.h"
#include <wrl/client.h>
#include "Specific/fast_vector.h"

using namespace TEN::Renderer::Graphics::Vertices;

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Utils;
	using namespace TEN::Renderer::Graphics;

	using Microsoft::WRL::ComPtr;
	
	class DX11VertexBuffer final : public IVertexBuffer
	{
	private:
		int                  _numVertices;
		ComPtr<ID3D11Buffer> _buffer;
		int                  _stride;

	public:
		DX11VertexBuffer() = default;
		~DX11VertexBuffer() = default;

		ID3D11Buffer* GetD3D11Buffer() const { return _buffer.Get(); }
		int GetStride() const { return _stride; }

		DX11VertexBuffer(ID3D11Device* device, int numVertices, int stride, void* vertices)
		{
			_stride = stride;

			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = _stride * (numVertices > 0 ? numVertices : 1);
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			if (numVertices != 0)
			{
				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = vertices;
				initData.SysMemPitch = _stride * numVertices;

				throwIfFailed(device->CreateBuffer(&desc, &initData, &_buffer));
			}
			else
			{
				throwIfFailed(device->CreateBuffer(&desc, nullptr, &_buffer));
			}

			_numVertices = numVertices;
		}

		bool Update(ID3D11DeviceContext* context, void* data, int startVertex, int count)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			HRESULT res = context->Map(_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

			if (SUCCEEDED(res))
			{
				void* dataPtr = (mappedResource.pData);
				auto* src = static_cast<std::byte*>(data) + startVertex * _stride;
				std::memcpy(dataPtr, src, static_cast<size_t>(count) * _stride);
				context->Unmap(_buffer.Get(), 0);
				return true;
			}
			else
			{
				TENLog("Could not update constant buffer! " + std::to_string(res), LogLevel::Error);
				return false;
			}
		}
	};
}

#endif
