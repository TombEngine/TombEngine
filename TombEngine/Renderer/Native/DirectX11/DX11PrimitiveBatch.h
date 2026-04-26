#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <memory>
#include <wrl/client.h>
#include "Renderer/Graphics/IPrimitiveBatch.h"
#include <PrimitiveBatch.h>

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;
	using namespace DirectX;

	using Microsoft::WRL::ComPtr;

	class DX11PrimitiveBatch : public IPrimitiveBatch
	{
	private:
		std::unique_ptr<PrimitiveBatch<Vertex>> _primitiveBatch;

	public:
		DX11PrimitiveBatch() = default;
		~DX11PrimitiveBatch() = default;

		DX11PrimitiveBatch(ID3D11DeviceContext* context)
		{
			_primitiveBatch = std::make_unique<PrimitiveBatch<Vertex>>(context);
		}

		void Begin() override { _primitiveBatch->Begin(); }
		void End() override { _primitiveBatch->End(); }
		void DrawLine(Vertex const& v1, Vertex const& v2) override { _primitiveBatch->DrawLine(v1, v2); }
		void DrawTriangle(Vertex const& v1, Vertex const& v2, Vertex const& v3) override { _primitiveBatch->DrawTriangle(v1, v2, v3); }
		void DrawQuad(Vertex const& v1, Vertex const& v2, Vertex const& v3, Vertex const& v4) override { _primitiveBatch->DrawQuad(v1, v2, v3, v4); }
	};
}

#endif
