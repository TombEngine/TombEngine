#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <wrl/client.h>
#include "Specific/fast_vector.h"
#include "Renderer/RendererUtils.h"
#include "Game/Debug/Debug.h"
#include "Renderer/Graphics/IIndexBuffer.h"

namespace TEN::Renderer::Native::DirectX11
{
	using Microsoft::WRL::ComPtr;

	using namespace TEN::Renderer::Utils;
	using namespace TEN::Renderer::Graphics;
	using namespace TEN::Debug;

	class DX11IndexBuffer final : public IIndexBuffer
	{
	private:
		int _numIndices = 0;
		ComPtr<ID3D11Buffer> _buffer;

	public:
		DX11IndexBuffer() = default;
		~DX11IndexBuffer() = default;

		ID3D11Buffer* GetD3D11Buffer() const { return _buffer.Get(); }

		DX11IndexBuffer(ID3D11Device* device, int numIndices, int* indices)
		{
			D3D11_BUFFER_DESC desc = {};
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = sizeof(int) * (numIndices > 0 ? numIndices : 1);
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			if (numIndices > 0 && indices)
			{
				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = indices;
				initData.SysMemPitch = sizeof(int) * numIndices;

				throwIfFailed(device->CreateBuffer(&desc, &initData, &_buffer));
			}
			else
			{
				throwIfFailed(device->CreateBuffer(&desc, nullptr, &_buffer));
			}

			_numIndices = numIndices;
		}

		bool Update(ID3D11DeviceContext* context, int* data, int startIndex, int count)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			HRESULT res = context->Map(_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			if (SUCCEEDED(res))
			{
				void* dataPtr = mappedResource.pData;
				std::memcpy(dataPtr, &data[startIndex], count * sizeof(int));
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
