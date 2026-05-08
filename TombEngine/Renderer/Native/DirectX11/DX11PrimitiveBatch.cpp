#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11PrimitiveBatch.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"

namespace TEN::Renderer::Native::DirectX11
{
	DX11PrimitiveBatch::DX11PrimitiveBatch(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		_context = context;
		_vertices.reserve(MAX_BATCH_VERTICES);

		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = MAX_BATCH_VERTICES * sizeof(Vertex);
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT res = device->CreateBuffer(&desc, nullptr, _vbo.GetAddressOf());
		throwIfFailed(res, device, "DX11PrimitiveBatch: CreateBuffer");
	}

	void DX11PrimitiveBatch::Begin()
	{
		_vertices.clear();
	}

	void DX11PrimitiveBatch::DrawLine(Vertex const& v1, Vertex const& v2)
	{
		_topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		_vertices.push_back(v1);
		_vertices.push_back(v2);
	}

	void DX11PrimitiveBatch::DrawTriangle(Vertex const& v1, Vertex const& v2, Vertex const& v3)
	{
		_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		_vertices.push_back(v1);
		_vertices.push_back(v2);
		_vertices.push_back(v3);
	}

	void DX11PrimitiveBatch::DrawQuad(Vertex const& v1, Vertex const& v2, Vertex const& v3, Vertex const& v4)
	{
		_topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		_vertices.push_back(v1);
		_vertices.push_back(v2);
		_vertices.push_back(v3);
		_vertices.push_back(v1);
		_vertices.push_back(v3);
		_vertices.push_back(v4);
	}

	void DX11PrimitiveBatch::Flush()
	{
		if (_vertices.empty())
			return;

		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = _context->Map(_vbo.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		if (FAILED(hr)) return;

		memcpy(mapped.pData, _vertices.data(), _vertices.size() * sizeof(Vertex));
		_context->Unmap(_vbo.Get(), 0);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		_context->IASetVertexBuffers(0, 1, _vbo.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(_topology);
		_context->Draw((UINT)_vertices.size(), 0);
	}

	void DX11PrimitiveBatch::End()
	{
		Flush();
	}
}

#endif
