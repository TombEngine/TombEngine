#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11SpriteBatch.h"
#include "Renderer/Native/DirectX11/DX11Texture2D.h"
#include "Renderer/Native/DirectX11/DX11RenderTarget2D.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"

namespace TEN::Renderer::Native::DirectX11
{
	// HLSL inline shaders (matching DXTK SpriteEffect.fx behavior).
	static const char* s_spriteHLSL = R"(
		cbuffer SpriteCB : register(b0)
		{
			row_major float4x4 MatrixTransform;
		};

		struct VSInput {
			float2 Position : POSITION;
			float2 TexCoord : TEXCOORD0;
			float4 Color    : COLOR0;
		};

		struct PSInput {
			float4 Position : SV_Position;
			float2 TexCoord : TEXCOORD0;
			float4 Color    : COLOR0;
		};

		PSInput VSMain(VSInput input) {
			PSInput output;
			output.Position = mul(float4(input.Position, 0, 1), MatrixTransform);
			output.TexCoord = input.TexCoord;
			output.Color = input.Color;
			return output;
		}

		Texture2D SpriteTexture : register(t0);
		SamplerState SpriteSampler : register(s0);

		float4 PSMain(PSInput input) : SV_Target {
			return SpriteTexture.Sample(SpriteSampler, input.TexCoord) * input.Color;
		}
	)";

	void DX11SpriteBatch::CreateShaders()
	{
		// Compile vertex shader.
		ComPtr<ID3DBlob> vsBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3DCompile(s_spriteHLSL, strlen(s_spriteHLSL), "SpriteEffect",
			nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, vsBlob.GetAddressOf(), errorBlob.GetAddressOf());
		if (FAILED(hr))
		{
			if (errorBlob)
				TENLog(std::string("SpriteBatch VS compile error: ") + (char*)errorBlob->GetBufferPointer(), LogLevel::Error);
			return;
		}

		hr = _device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, _vertexShader.GetAddressOf());
		throwIfFailed(hr, _device.Get(), "CreateVertexShader for SpriteBatch");

		// Input layout.
		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		hr = _device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), _inputLayout.GetAddressOf());
		throwIfFailed(hr, _device.Get(), "CreateInputLayout for SpriteBatch");

		// Compile pixel shader.
		ComPtr<ID3DBlob> psBlob;
		hr = D3DCompile(s_spriteHLSL, strlen(s_spriteHLSL), "SpriteEffect",
			nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, psBlob.GetAddressOf(), errorBlob.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			if (errorBlob)
				TENLog(std::string("SpriteBatch PS compile error: ") + (char*)errorBlob->GetBufferPointer(), LogLevel::Error);
			return;
		}

		hr = _device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, _pixelShader.GetAddressOf());
		throwIfFailed(hr, _device.Get(), "CreatePixelShader for SpriteBatch");

		// Constant buffer (4x4 matrix = 64 bytes).
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.ByteWidth = 64;
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		hr = _device->CreateBuffer(&cbDesc, nullptr, _constantBuffer.GetAddressOf());
		throwIfFailed(hr, _device.Get(), "CreateBuffer CB for SpriteBatch");
	}

	void DX11SpriteBatch::CreateStates()
	{
		// Sampler: linear clamp.
		D3D11_SAMPLER_DESC sd = {};
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		_device->CreateSamplerState(&sd, _sampler.GetAddressOf());

		// Blend: opaque.
		D3D11_BLEND_DESC bd = {};
		bd.RenderTarget[0].BlendEnable = FALSE;
		bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		_device->CreateBlendState(&bd, _blendOpaque.GetAddressOf());

		// Blend: alpha (non-premultiplied).
		bd.RenderTarget[0].BlendEnable = TRUE;
		bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		_device->CreateBlendState(&bd, _blendAlpha.GetAddressOf());

		// Blend: additive.
		bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		_device->CreateBlendState(&bd, _blendAdditive.GetAddressOf());

		// Rasterizer: no culling, fill solid.
		D3D11_RASTERIZER_DESC rd = {};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_NONE;
		rd.ScissorEnable = TRUE;
		_device->CreateRasterizerState(&rd, _rasterizer.GetAddressOf());

		// Depth: disabled.
		D3D11_DEPTH_STENCIL_DESC dsd = {};
		dsd.DepthEnable = FALSE;
		dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		_device->CreateDepthStencilState(&dsd, _depthOff.GetAddressOf());
	}

	DX11SpriteBatch::DX11SpriteBatch(ID3D11Device* device, ID3D11DeviceContext* context)
		: _device(device), _context(context)
	{
		CreateShaders();
		CreateStates();

		// Create dynamic VBO.
		// 6 vertices per quad, 8 floats per vertex (pos2 + uv2 + color4).
		D3D11_BUFFER_DESC vbDesc = {};
		vbDesc.ByteWidth = MAX_QUADS * 6 * 8 * sizeof(float);
		vbDesc.Usage = D3D11_USAGE_DYNAMIC;
		vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		_device->CreateBuffer(&vbDesc, nullptr, _vbo.GetAddressOf());
	}

	ID3D11ShaderResourceView* DX11SpriteBatch::GetD3D11ShaderResourceView(ITextureBase* texture)
	{
		if (auto tex2D = dynamic_cast<DX11Texture2D*>(texture))
			return tex2D->GetD3D11ShaderResourceView();
		if (auto rt2D = dynamic_cast<DX11RenderTarget2D*>(texture))
			return rt2D->GetD3D11ShaderResourceView();
		return nullptr;
	}

	void DX11SpriteBatch::Begin(SpriteSortingMode sortingMode, BlendMode blendMode)
	{
		_queue.clear();
		_blendMode = blendMode;
	}

	void DX11SpriteBatch::Draw(ITextureBase* texture, RendererRectangle area, Vector4 color)
	{
		DX11SpriteBatchQuad quad;
		quad.SRV = GetD3D11ShaderResourceView(texture);
		quad.Left = (float)area.Left;
		quad.Top = (float)area.Top;
		quad.Right = (float)area.Right;
		quad.Bottom = (float)area.Bottom;
		quad.Color = color;
		_queue.push_back(quad);
	}

	void DX11SpriteBatch::DrawWithSRV(ID3D11ShaderResourceView* srv, RendererRectangle area, float u0, float v0, float u1, float v1, Vector4 color)
	{
		DX11SpriteBatchQuad quad;
		quad.SRV = srv;
		quad.Left = (float)area.Left;
		quad.Top = (float)area.Top;
		quad.Right = (float)area.Right;
		quad.Bottom = (float)area.Bottom;
		quad.U0 = u0;
		quad.V0 = v0;
		quad.U1 = u1;
		quad.V1 = v1;
		quad.Color = color;
		_queue.push_back(quad);
	}

	void DX11SpriteBatch::Flush()
	{
		if (_queue.empty())
			return;

		// Get viewport for orthographic projection.
		D3D11_VIEWPORT vp;
		UINT numVP = 1;
		_context->RSGetViewports(&numVP, &vp);

		// Build viewport → clip transform: maps [0,w]×[0,h] → [-1,1]×[-1,1].
		// row_major in HLSL: layout matches SimpleMath/DXTK convention.
		float xScale = 2.0f / vp.Width;
		float yScale = -2.0f / vp.Height;
		float transform[16] = {
			xScale,  0,       0, 0,
			0,       yScale,  0, 0,
			0,       0,       1, 0,
			-1.0f,   1.0f,    0, 1
		};

		// Update constant buffer.
		D3D11_MAPPED_SUBRESOURCE mapped;
		_context->Map(_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, transform, sizeof(transform));
		_context->Unmap(_constantBuffer.Get(), 0);

		// Build vertex data: 6 vertices per quad (8 floats each).
		std::vector<float> vertices;
		vertices.reserve(_queue.size() * 6 * 8);

		auto addVertex = [&](float x, float y, float u, float v, const Vector4& c)
		{
			vertices.push_back(x); vertices.push_back(y);
			vertices.push_back(u); vertices.push_back(v);
			vertices.push_back(c.x); vertices.push_back(c.y);
			vertices.push_back(c.z); vertices.push_back(c.w);
		};

		// Sort by texture for batching.
		std::sort(_queue.begin(), _queue.end(),
			[](const DX11SpriteBatchQuad& a, const DX11SpriteBatchQuad& b) { return a.SRV < b.SRV; });

		for (auto& q : _queue)
		{
			addVertex(q.Left,  q.Top,    q.U0, q.V0, q.Color);
			addVertex(q.Right, q.Top,    q.U1, q.V0, q.Color);
			addVertex(q.Right, q.Bottom, q.U1, q.V1, q.Color);
			addVertex(q.Left,  q.Top,    q.U0, q.V0, q.Color);
			addVertex(q.Right, q.Bottom, q.U1, q.V1, q.Color);
			addVertex(q.Left,  q.Bottom, q.U0, q.V1, q.Color);
		}

		// Upload vertices.
		_context->Map(_vbo.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(float));
		_context->Unmap(_vbo.Get(), 0);

		// Set pipeline state.
		_context->IASetInputLayout(_inputLayout.Get());
		UINT stride = 8 * sizeof(float);
		UINT offset = 0;
		_context->IASetVertexBuffers(0, 1, _vbo.GetAddressOf(), &stride, &offset);
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		_context->VSSetShader(_vertexShader.Get(), nullptr, 0);
		_context->VSSetConstantBuffers(0, 1, _constantBuffer.GetAddressOf());
		_context->PSSetShader(_pixelShader.Get(), nullptr, 0);
		_context->PSSetSamplers(0, 1, _sampler.GetAddressOf());

		// Set blend state.
		float blendFactor[4] = { 0, 0, 0, 0 };
		switch (_blendMode)
		{
		case BlendMode::Additive:
			_context->OMSetBlendState(_blendAdditive.Get(), blendFactor, 0xFFFFFFFF);
			break;
		case BlendMode::AlphaBlend:
			_context->OMSetBlendState(_blendAlpha.Get(), blendFactor, 0xFFFFFFFF);
			break;
		default:
			_context->OMSetBlendState(_blendOpaque.Get(), blendFactor, 0xFFFFFFFF);
			break;
		}

		_context->RSSetState(_rasterizer.Get());
		_context->OMSetDepthStencilState(_depthOff.Get(), 0);

		// Draw batched by texture.
		int vertexOffset = 0;
		ID3D11ShaderResourceView* currentSRV = _queue[0].SRV;

		for (size_t i = 0; i <= _queue.size(); i++)
		{
			bool flush = (i == _queue.size()) || (_queue[i].SRV != currentSRV);
			if (flush)
			{
				int count = ((int)i * 6) - vertexOffset;
				_context->PSSetShaderResources(0, 1, &currentSRV);
				_context->Draw(count, vertexOffset);

				if (i < _queue.size())
				{
					currentSRV = _queue[i].SRV;
					vertexOffset = (int)i * 6;
				}
			}
		}
	}

	void DX11SpriteBatch::End()
	{
		Flush();
	}
}

#endif
