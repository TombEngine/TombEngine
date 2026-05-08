#pragma once

#ifdef SDL_PLATFORM_WIN32

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <memory>
#include <vector>
#include "Renderer/Graphics/ISpriteBatch.h"
#include "Renderer/Native/DirectX11/DX11TextureBase.h"

namespace TEN::Renderer::Native::DirectX11
{
	using namespace TEN::Renderer::Graphics;

	using Microsoft::WRL::ComPtr;

	struct DX11SpriteBatchQuad
	{
		ID3D11ShaderResourceView* SRV;
		float Left, Top, Right, Bottom;
		float U0 = 0, V0 = 0, U1 = 1, V1 = 1;
		Vector4 Color;
	};

	class DX11SpriteBatch final : public ISpriteBatch
	{
	private:
		ComPtr<ID3D11Device>           _device;
		ComPtr<ID3D11DeviceContext>     _context;
		ComPtr<ID3D11Buffer>           _vbo;
		ComPtr<ID3D11InputLayout>      _inputLayout;
		ComPtr<ID3D11VertexShader>     _vertexShader;
		ComPtr<ID3D11PixelShader>      _pixelShader;
		ComPtr<ID3D11Buffer>           _constantBuffer;
		ComPtr<ID3D11SamplerState>     _sampler;
		ComPtr<ID3D11BlendState>       _blendOpaque;
		ComPtr<ID3D11BlendState>       _blendAlpha;
		ComPtr<ID3D11BlendState>       _blendAdditive;
		ComPtr<ID3D11BlendState>       _blendPremultipliedAlpha;
		ComPtr<ID3D11RasterizerState>  _rasterizer;
		ComPtr<ID3D11DepthStencilState> _depthOff;

		std::vector<DX11SpriteBatchQuad> _queue;
		BlendMode _blendMode = BlendMode::Opaque;

		static constexpr int MAX_QUADS = 4096;

	public:
		~DX11SpriteBatch() = default;

		DX11SpriteBatch(ID3D11Device* device, ID3D11DeviceContext* context);

		ID3D11ShaderResourceView* GetD3D11ShaderResourceView(ITextureBase* texture);

		void Begin(SpriteSortingMode sortingMode, BlendMode blendMode) override;
		void End() override;
		void Draw(ITextureBase* texture, RendererRectangle area, Vector4 color) override;
		void DrawWithSRV(ID3D11ShaderResourceView* srv, RendererRectangle area, float u0, float v0, float u1, float v1, Vector4 color);

	private:
		void Flush();
		void CreateShaders();
		void CreateStates();
	};
}

#endif
