#pragma once

#ifdef HAS_DX11

#include <d3d11.h>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include "Renderer/Graphics/IPrimitiveBatch.h"

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;

	using Microsoft::WRL::ComPtr;

	class DX11PrimitiveBatch final : public IPrimitiveBatch
	{
	private:
		ComPtr<ID3D11DeviceContext> _context;
		ComPtr<ID3D11Buffer> _vbo;

		std::vector<Vertex> _vertices;
		D3D11_PRIMITIVE_TOPOLOGY _topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		static constexpr int MAX_BATCH_VERTICES = 65536;

	public:
		DX11PrimitiveBatch() = default;
		~DX11PrimitiveBatch() = default;

		DX11PrimitiveBatch(ID3D11Device* device, ID3D11DeviceContext* context);

		void Begin() override;
		void End() override;
		void DrawLine(Vertex const& v1, Vertex const& v2) override;
		void DrawTriangle(Vertex const& v1, Vertex const& v2, Vertex const& v3) override;
		void DrawQuad(Vertex const& v1, Vertex const& v2, Vertex const& v3, Vertex const& v4) override;

	private:
		void Flush();
	};
}

#endif
