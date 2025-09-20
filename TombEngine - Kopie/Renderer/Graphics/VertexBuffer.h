#pragma once
#include <d3d11.h>
#include "Renderer/RendererUtils.h"
#include "Renderer/Graphics/Vertices/Vertex.h"
#include <wrl/client.h>
#include <vector>
#include "Specific/fast_vector.h"

using namespace TEN::Renderer::Graphics::Vertices;

namespace TEN::Renderer::Graphics
{
	using namespace TEN::Renderer::Utils;

	using Microsoft::WRL::ComPtr;

	template <typename CVertex>
	class VertexBuffer
	{
	private:
		int _numVertices;

	public:
		ComPtr<ID3D11Buffer> Buffer;
		
		VertexBuffer() 
		{
		};
		
		template <typename CVertex>
		VertexBuffer(ID3D11Device* device, int numVertices, std::vector<CVertex> vertices)
		{
			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = sizeof(CVertex) * (numVertices > 0 ? numVertices : 1);
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			if (vertices.size() != 0)
			{
				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = vertices.data();
				initData.SysMemPitch = sizeof(CVertex) * numVertices;

				throwIfFailed(device->CreateBuffer(&desc, &initData, &Buffer));
			}
			else
			{
				throwIfFailed(device->CreateBuffer(&desc, nullptr, &Buffer));
			}

			_numVertices = numVertices;
		}

		template <typename CVertex>
		VertexBuffer(ID3D11Device* device, int numVertices, fast_vector<CVertex> vertices)
		{
			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = sizeof(CVertex) * (numVertices > 0 ? numVertices : 1);
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			if (vertices.size() != 0)
			{
				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = vertices.data();
				initData.SysMemPitch = sizeof(CVertex) * numVertices;

				throwIfFailed(device->CreateBuffer(&desc, &initData, &Buffer));
			}
			else
			{
				throwIfFailed(device->CreateBuffer(&desc, nullptr, &Buffer));
			}

			_numVertices = numVertices;
		}

		template <typename CVertex>
		VertexBuffer(ID3D11Device* device, int numVertices, CVertex* vertices)
		{
			D3D11_BUFFER_DESC desc = {};

			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.ByteWidth = sizeof(CVertex) * (numVertices > 0 ? numVertices : 1);
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

			if (numVertices != 0)
			{
				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem = vertices;
				initData.SysMemPitch = sizeof(CVertex) * numVertices;

				throwIfFailed(device->CreateBuffer(&desc, &initData, &Buffer));
			}
			else
			{
				throwIfFailed(device->CreateBuffer(&desc, nullptr, &Buffer));
			}

			_numVertices = numVertices;
		}

		template <typename CVertex>
		bool Update(ID3D11DeviceContext* context, CVertex* data, int startVertex, int count)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			HRESULT res = context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

			if (SUCCEEDED(res))
			{
				void* dataPtr = (mappedResource.pData);
				memcpy(dataPtr, &data[startVertex], count * sizeof(CVertex));
				context->Unmap(Buffer.Get(), 0);
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
