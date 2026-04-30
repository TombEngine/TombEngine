#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Native/DirectX11/DX11GraphicsDevice.h"
#include "Renderer/Native/DirectX11/DX11ErrorHelper.h"
#include "Renderer/Native/DirectX11/DX11Utils.h"
#include "Specific/EngineMain.h"
#include "Specific/configuration.h"
#include "Specific/trutils.h"
#include <wincodec.h>
#include <ScreenGrab.h>
#include <ctime>

extern GameConfiguration g_Configuration;

using namespace TEN::Renderer::Graphics;

namespace TEN::Renderer::Native::DirectX11
{
	std::unique_ptr<IVertexBuffer> DX11GraphicsDevice::CreateVertexBuffer(int numVertices, int vertexSize, void* data)
	{
		return std::make_unique<DX11VertexBuffer>(_device.Get(), numVertices, vertexSize, data);
	}

	void DX11GraphicsDevice::UpdateVertexBuffer(IVertexBuffer* vertexBuffer, int startVertex, int count, void* data)
	{
		auto nativeVertexBuffer = static_cast<DX11VertexBuffer*>(vertexBuffer);
		nativeVertexBuffer->Update(_context.Get(), data, startVertex, count);
	}

	void DX11GraphicsDevice::BindVertexBuffer(IVertexBuffer* vertexBuffer)
	{
		auto nativeVertexBuffer = static_cast<DX11VertexBuffer*>(vertexBuffer);

		unsigned int stride = nativeVertexBuffer->GetStride();
		unsigned int offset = 0;

		auto d3dBuffer = nativeVertexBuffer->GetD3D11Buffer();

		_context->IASetVertexBuffers(0, 1, &d3dBuffer, &stride, &offset);
	}

	std::unique_ptr<IIndexBuffer> DX11GraphicsDevice::CreateIndexBuffer(int numIndices, int* indices)
	{
		return std::make_unique<DX11IndexBuffer>(_device.Get(), numIndices, indices);
	}

	void DX11GraphicsDevice::UpdateIndexBuffer(IIndexBuffer* indexBuffer, int numIndices, int startIndex, int* data)
	{
		auto nativeIndexBuffer = static_cast<DX11IndexBuffer*>(indexBuffer);
		nativeIndexBuffer->Update(_context.Get(), data, startIndex, numIndices);
	}

	void DX11GraphicsDevice::BindIndexBuffer(IIndexBuffer* indexBuffer)
	{
		auto nativeIndexBuffer = static_cast<DX11IndexBuffer*>(indexBuffer);
		auto d3dBuffer = nativeIndexBuffer->GetD3D11Buffer();

		_context->IASetIndexBuffer(d3dBuffer, DXGI_FORMAT_R32_UINT, 0);
	}

	std::unique_ptr<IRenderSurface2D> DX11GraphicsDevice::CreateRenderSurface2D(int width, int height, SurfaceFormat colorFormat, bool isTypeless, DepthFormat depthFormat)
	{
		auto nativeRenderTarget = std::make_unique<DX11RenderTarget2D>(_device.Get(), width, height, GetDXGIFormat(colorFormat), isTypeless);

		std::unique_ptr<IDepthTarget> nativeDepthTarget = nullptr;
		if (depthFormat != DepthFormat::None)
			nativeDepthTarget = std::make_unique<DX11DepthTarget>(_device.Get(), width, height, GetDXGIFormat(depthFormat));

		// Flush GPU command buffer to prevent TDR on Intel integrated GPUs.
		_context->Flush();

		return std::make_unique<IRenderSurface2D>(
			std::move(nativeRenderTarget),
			std::move(nativeDepthTarget));
	}

	std::unique_ptr<IRenderSurface2D> DX11GraphicsDevice::CreateRenderSurface2D(int width, int height, int arraySize, SurfaceFormat colorFormat, DepthFormat depthFormat)
	{
		auto nativeRenderTarget = std::make_unique<DX11RenderTarget2D>(_device.Get(), width, height, arraySize, GetDXGIFormat(colorFormat));

		std::unique_ptr<IDepthTarget> nativeDepthTarget = nullptr;
		if (depthFormat != DepthFormat::None)
			nativeDepthTarget = std::make_unique<DX11DepthTarget>(_device.Get(), width, height, arraySize, GetDXGIFormat(depthFormat));

		_context->Flush();

		return std::make_unique<IRenderSurface2D>(
			std::move(nativeRenderTarget),
			std::move(nativeDepthTarget));
	}

	std::unique_ptr<IRenderSurface2D> DX11GraphicsDevice::CreateRenderSurface2D(IRenderSurface2D* parentRenderTarget, SurfaceFormat colorFormat)
	{
		auto nativeRenderTarget = static_cast<DX11RenderTarget2D*>(parentRenderTarget->GetRenderTarget());

		auto result = std::make_unique<IRenderSurface2D>(
			std::move(std::make_unique<DX11RenderTarget2D>(_device.Get(), nativeRenderTarget->GetD3D11Texture(), GetDXGIFormat(colorFormat))),
			nullptr
		);

		_context->Flush();

		return result;
	}

	IRenderTargetCube* DX11GraphicsDevice::CreateRenderTargetCube(int size, SurfaceFormat colorFormat)
	{
		return nullptr; // new DX11RenderTargetCube(_device.Get(), size, GetDXGIFormat(colorFormat), GetDXGIFormat(depthFormat));
	}

	std::unique_ptr<ITexture2D> DX11GraphicsDevice::CreateTexture2D(int width, int height, SurfaceFormat format, void* data, bool isDynamic)
	{
		auto texture = std::make_unique<DX11Texture2D>(_device.Get(), width, height, GetDXGIFormat(format), data, isDynamic);
		_context->Flush();
		return texture;
	}

	std::unique_ptr<ITexture2D> DX11GraphicsDevice::CreateTexture2DFromFile(const std::string fileName)
	{
		auto texture = std::make_unique<DX11Texture2D>(_device.Get(), TEN::Utils::ToWString(fileName));
		_context->Flush();
		return texture;
	}

	std::unique_ptr<ITexture2D> DX11GraphicsDevice::CreateTexture2DFromFileInMemory(int dataSize, unsigned char* data)
	{
		auto texture = std::make_unique<DX11Texture2D>(_device.Get(), dataSize, data);
		_context->Flush();
		return texture;
	}

	void DX11GraphicsDevice::SetBlendMode(BlendMode blendMode)
	{
		switch (blendMode)
		{
		default:
		case BlendMode::Opaque:
			_context->OMSetBlendState(_renderStates->Opaque(), nullptr, 0xFFFFFFFF);
			break;

		case BlendMode::AlphaBlend:
			_context->OMSetBlendState(_renderStates->NonPremultiplied(), nullptr, 0xFFFFFFFF);
			break;

		case BlendMode::AlphaTest:
			_context->OMSetBlendState(_renderStates->Opaque(), nullptr, 0xFFFFFFFF);
			break;

		case BlendMode::Subtractive:
			_context->OMSetBlendState(_subtractiveBlendState.Get(), nullptr, 0xFFFFFFFF);
			break;

		case BlendMode::Additive:
			_context->OMSetBlendState(_renderStates->Additive(), nullptr, 0xFFFFFFFF);
			break;

		case BlendMode::Screen:
			_context->OMSetBlendState(_screenBlendState.Get(), nullptr, 0xFFFFFFFF);
			break;

		case BlendMode::Lighten:
			_context->OMSetBlendState(_lightenBlendState.Get(), nullptr, 0xFFFFFFFF);
			break;

		case BlendMode::Exclude:
			_context->OMSetBlendState(_excludeBlendState.Get(), nullptr, 0xFFFFFFFF);
			break;

		case BlendMode::PremultipliedAlphaBlend:
			_context->OMSetBlendState(_renderStates->AlphaBlend(), nullptr, 0xFFFFFFFF);
			break;
		}
	}

	void DX11GraphicsDevice::SetDepthState(DepthState depthState)
	{
		switch (depthState)
		{
		case DepthState::Read:
			_context->OMSetDepthStencilState(_renderStates->DepthRead(), 0xFFFFFFFF);
			break;

		case DepthState::Write:
			_context->OMSetDepthStencilState(_renderStates->DepthDefault(), 0xFFFFFFFF);
			break;

		case DepthState::None:
			_context->OMSetDepthStencilState(_renderStates->DepthNone(), 0xFFFFFFFF);
			break;

		}
	}

	void DX11GraphicsDevice::SetCullMode(CullMode cullMode)
	{
		switch (cullMode)
		{
		case CullMode::None:
			_context->RSSetState(_cullNoneRasterizerState.Get());
			break;

		case CullMode::CounterClockwise:
			_context->RSSetState(_cullCounterClockwiseRasterizerState.Get());
			break;

		case CullMode::Clockwise:
			_context->RSSetState(_cullClockwiseRasterizerState.Get());
			break;

		case CullMode::Wireframe:
			_context->RSSetState(_renderStates->Wireframe());
			break;

		}
	}

	void DX11GraphicsDevice::SetScissor(RendererRectangle rectangle)
	{
		D3D11_RECT rects;
		rects.left = rectangle.Left;
		rects.top = rectangle.Top;
		rects.right = rectangle.Right;
		rects.bottom = rectangle.Bottom;

		_context->RSSetScissorRects(1, &rects);
	}

	void DX11GraphicsDevice::BindTexture(TextureRegister registerType, ITextureBase* texture, SamplerStateRegister samplerType)
	{
		auto* d3dShaderResourceView = GetD3D11ShaderResourceView(texture);

		_context->PSSetShaderResources((unsigned int)registerType, 1, &d3dShaderResourceView);

		ID3D11SamplerState* d3dSamplerState = nullptr;
		switch (samplerType)
		{
		case SamplerStateRegister::AnisotropicClamp:
			d3dSamplerState = _renderStates->AnisotropicClamp();
			break;

		case SamplerStateRegister::AnisotropicWrap:
			d3dSamplerState = _renderStates->AnisotropicWrap();
			break;

		case SamplerStateRegister::LinearClamp:
			d3dSamplerState = _renderStates->LinearClamp();
			break;

		case SamplerStateRegister::LinearWrap:
			d3dSamplerState = _renderStates->LinearWrap();
			break;

		case SamplerStateRegister::PointWrap:
			d3dSamplerState = _pointWrapSamplerState.Get();
			break;

		case SamplerStateRegister::ShadowMap:
			d3dSamplerState = _shadowSampler.Get();
			break;

		default:
			return;
		}

		_context->PSSetSamplers((unsigned int)registerType, 1, &d3dSamplerState);
	}

	void DX11GraphicsDevice::BindConstantBuffer(ShaderStage shaderStage, ConstantBufferRegister constantBufferType, IConstantBuffer* buffer)
	{
		auto nativeConstantBuffer = static_cast<DX11ConstantBuffer*>(buffer);
		auto d3dBuffer = nativeConstantBuffer->GetD3D11Buffer();

		switch (shaderStage)
		{
		case ShaderStage::VertexShader:
			_context->VSSetConstantBuffers((unsigned int)constantBufferType, 1, &d3dBuffer);
			break;

		case ShaderStage::GeometryShader:
			_context->GSSetConstantBuffers((unsigned int)constantBufferType, 1, &d3dBuffer);
			break;

		case ShaderStage::PixelShader:
			_context->PSSetConstantBuffers((unsigned int)constantBufferType, 1, &d3dBuffer);
			break;

		case ShaderStage::ComputeShader:
			_context->CSSetConstantBuffers((unsigned int)constantBufferType, 1, &d3dBuffer);
			break;

		case ShaderStage::HullShader:
			_context->HSSetConstantBuffers((unsigned int)constantBufferType, 1, &d3dBuffer);
			break;

		case ShaderStage::DomainShader:
			_context->DSSetConstantBuffers((unsigned int)constantBufferType, 1, &d3dBuffer);
			break;

		default:
			break;
		}
	}

	std::unique_ptr<IConstantBuffer> DX11GraphicsDevice::CreateConstantBuffer(int size, std::wstring name)
	{
		return std::make_unique<DX11ConstantBuffer>(_device.Get(), size, name);
	}

	void DX11GraphicsDevice::UpdateConstantBuffer(IConstantBuffer* constantBuffer, void* data)
	{
		auto nativeConstantBuffer = static_cast<DX11ConstantBuffer*>(constantBuffer);
		nativeConstantBuffer->UpdateData(data, _context.Get());
	}

	void DX11GraphicsDevice::DrawIndexedTriangles(int count, int baseIndex, int baseVertex)
	{
		_context->DrawIndexed(count, baseIndex, baseVertex);
	}

	void DX11GraphicsDevice::DrawIndexedInstancedTriangles(int count, int instances, int baseIndex, int baseVertex)
	{
		_context->DrawIndexedInstanced(count, instances, baseIndex, baseVertex, 0);
	}

	void DX11GraphicsDevice::DrawInstancedTriangles(int count, int instances, int baseVertex)
	{
		_context->DrawInstanced(count, instances, baseVertex, 0);
	}

	void DX11GraphicsDevice::DrawTriangles(int count, int baseVertex)
	{
		_context->Draw(count, baseVertex);
	}

	void DX11GraphicsDevice::ClearRenderTarget2D(IRenderTarget2D* renderTarget, XMVECTORF32 clearColor)
	{
		auto nativeRenderTarget = static_cast<DX11RenderTarget2D*>(renderTarget);
		_context->ClearRenderTargetView(nativeRenderTarget->GetD3D11RenderTargetView(), clearColor);
	}

	void DX11GraphicsDevice::ClearRenderTarget2D(IRenderTarget2D* renderTarget, int arrayIndex, XMVECTORF32 clearColor)
	{
		auto nativeRenderTarget = static_cast<DX11RenderTarget2D*>(renderTarget);
		_context->ClearRenderTargetView(nativeRenderTarget->GetD3D11RenderTargetView(arrayIndex), clearColor);
	}

	void DX11GraphicsDevice::ClearDepthStencil(IDepthTarget* renderTarget, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil)
	{
		auto nativeRenderTarget = static_cast<DX11DepthTarget*>(renderTarget);
		_context->ClearDepthStencilView(nativeRenderTarget->GetD3D11DepthStencilView(), GetClearFlags(clearFlags), depth, stencil);
	}

	void DX11GraphicsDevice::ClearDepthStencil(IDepthTarget* renderTarget, int arrayIndex, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil)
	{
		auto nativeRenderTarget = static_cast<DX11DepthTarget*>(renderTarget);
		_context->ClearDepthStencilView(nativeRenderTarget->GetD3D11DepthStencilView(arrayIndex), GetClearFlags(clearFlags), depth, stencil);
	}

	void DX11GraphicsDevice::BindRenderTarget(IRenderTarget2D* renderTarget, IDepthTarget* depthTarget)
	{
		auto nativeRenderTarget = static_cast<DX11RenderTarget2D*>(renderTarget);
		auto d3dRenderTargetView = nativeRenderTarget->GetD3D11RenderTargetView();

		ID3D11DepthStencilView* d3dDepthStencilView = nullptr;
		if (depthTarget != nullptr)
		{
			auto nativeDepthTarget = static_cast<DX11DepthTarget*>(depthTarget);
			d3dDepthStencilView = nativeDepthTarget->GetD3D11DepthStencilView();
		}
				
		_context->OMSetRenderTargets(1, &d3dRenderTargetView, d3dDepthStencilView);
	}

	void DX11GraphicsDevice::BindRenderTarget(IRenderTargetBinding renderTarget, IDepthTargetBinding depthTarget)
	{
		auto nativeRenderTarget = static_cast<DX11RenderTarget2D*>(renderTarget.RenderTarget);
		auto d3dRenderTargetView = nativeRenderTarget->GetD3D11RenderTargetView(renderTarget.ArrayIndex);

		ID3D11DepthStencilView* d3dDepthStencilView = nullptr;
		if (depthTarget.DepthTarget != nullptr)
		{
			auto nativeDepthTarget = static_cast<DX11DepthTarget*>(depthTarget.DepthTarget);
			d3dDepthStencilView = nativeDepthTarget->GetD3D11DepthStencilView(depthTarget.ArrayIndex);
		}

		_context->OMSetRenderTargets(1, &d3dRenderTargetView, d3dDepthStencilView);
	}

	void DX11GraphicsDevice::BindRenderTargets(std::vector<IRenderTarget2D*> renderTargets, IDepthTarget* depthTarget)
	{
		std::vector<ID3D11RenderTargetView*> d3dRenderTargetViews;
		for (int i = 0; i < renderTargets.size(); i++)
		{
			auto renderTarget = renderTargets[i];
			auto nativeRenderTarget = static_cast<DX11RenderTarget2D*>(renderTarget);
			auto d3dRenderTargetView = nativeRenderTarget->GetD3D11RenderTargetView(0);
			d3dRenderTargetViews.push_back(d3dRenderTargetView);
		}

		ID3D11DepthStencilView* d3dDepthStencilView = nullptr;
		if (depthTarget != nullptr)
		{
			auto nativeDepthTarget = static_cast<DX11DepthTarget*>(depthTarget);
			d3dDepthStencilView = nativeDepthTarget->GetD3D11DepthStencilView();
		}

		_context->OMSetRenderTargets((int)d3dRenderTargetViews.size(), d3dRenderTargetViews.data(), d3dDepthStencilView);
	}

	void DX11GraphicsDevice::BindRenderTargets(std::vector<IRenderTargetBinding> renderTargets, IDepthTargetBinding depthTarget)
	{
		std::vector<ID3D11RenderTargetView*> d3dRenderTargetViews;
		for (int i = 0; i < renderTargets.size(); i++)
		{
			auto renderTarget = renderTargets[i].RenderTarget;
			auto nativeRenderTarget = static_cast<DX11RenderTarget2D*>(renderTarget);
			auto d3dRenderTargetView = nativeRenderTarget->GetD3D11RenderTargetView(renderTargets[i].ArrayIndex);
			d3dRenderTargetViews.push_back(d3dRenderTargetView);
		}

		ID3D11DepthStencilView* d3dDepthStencilView = nullptr;
		if (depthTarget.DepthTarget != nullptr)
		{
			auto nativeDepthTarget = static_cast<DX11DepthTarget*>(depthTarget.DepthTarget);
			d3dDepthStencilView = nativeDepthTarget->GetD3D11DepthStencilView(depthTarget.ArrayIndex);
		}

		_context->OMSetRenderTargets((int)d3dRenderTargetViews.size(), d3dRenderTargetViews.data(), d3dDepthStencilView);
	}

	void DX11GraphicsDevice::SetViewport(RendererViewport viewport)
	{
		D3D11_VIEWPORT d3dViewport;
		d3dViewport.TopLeftX = viewport.X;
		d3dViewport.TopLeftY = viewport.Y;
		d3dViewport.Width = viewport.Width;
		d3dViewport.Height = viewport.Height;
		d3dViewport.MinDepth = viewport.MinDepth;
		d3dViewport.MaxDepth = viewport.MaxDepth;

		_context->RSSetViewports(1, &d3dViewport);
	}

	void DX11GraphicsDevice::SetScissor(RendererViewport viewport)
	{
		D3D11_RECT rects[1];
		rects[0].left = viewport.X;
		rects[0].right = viewport.Width;
		rects[0].top = viewport.Y;
		rects[0].bottom = viewport.Height;

		_context->RSSetScissorRects(1, rects);
	}

	void DX11GraphicsDevice::SetInputLayout(IInputLayout* inputLayout)
	{
		auto nativeInputLayout = static_cast<DX11InputLayout*>(inputLayout);
		_context->IASetInputLayout(nativeInputLayout->GetD3D11InputLayout());
	}

	std::unique_ptr<IInputLayout> DX11GraphicsDevice::CreateInputLayout(std::vector<RendererInputLayoutField> fields, IShader* shader)
	{
		auto nativeShader = static_cast<DX11Shader*>(shader);
		return std::make_unique<DX11InputLayout>(_device.Get(), fields, nativeShader);
	}

	void DX11GraphicsDevice::SetPrimitiveType(PrimitiveType primitiveType)
	{
		switch (primitiveType)
		{
		case PrimitiveType::TriangleList:
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); 
			break;

		case PrimitiveType::TriangleStrip:
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			break;

		case PrimitiveType::LineList:
			_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
			break;
		}
	}

	void DX11GraphicsDevice::Initialize()
	{
		_renderStates = std::make_unique<CommonStates>(_device.Get());

		D3D11_BLEND_DESC blendStateDesc{};
		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = false;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		throwIfFailed(_device->CreateBlendState(&blendStateDesc, _subtractiveBlendState.GetAddressOf()));

		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = false;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		throwIfFailed(_device->CreateBlendState(&blendStateDesc, _screenBlendState.GetAddressOf()));

		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = false;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		throwIfFailed(_device->CreateBlendState(&blendStateDesc, _lightenBlendState.GetAddressOf()));

		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = false;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_DEST_COLOR;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_SUBTRACT;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		throwIfFailed(_device->CreateBlendState(&blendStateDesc, _excludeBlendState.GetAddressOf()));

		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = true;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blendStateDesc.RenderTarget[1].BlendEnable = true;
		blendStateDesc.RenderTarget[1].SrcBlend = D3D11_BLEND_ZERO;
		blendStateDesc.RenderTarget[1].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
		blendStateDesc.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blendStateDesc.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED;
		throwIfFailed(_device->CreateBlendState(&blendStateDesc, _transparencyBlendState.GetAddressOf()));

		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = false;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		throwIfFailed(_device->CreateBlendState(&blendStateDesc, _finalTransparencyBlendState.GetAddressOf()));

		D3D11_SAMPLER_DESC shadowSamplerDesc = {};
		shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		throwIfFailed(_device->CreateSamplerState(&shadowSamplerDesc, _shadowSampler.GetAddressOf()));
		_shadowSampler->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("ShadowSampler") - 1, "ShadowSampler");

		D3D11_RASTERIZER_DESC rasterizerStateDesc = {};

		rasterizerStateDesc.CullMode = D3D11_CULL_BACK;
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.DepthClipEnable = true;
		rasterizerStateDesc.MultisampleEnable = true;
		rasterizerStateDesc.AntialiasedLineEnable = true;
		rasterizerStateDesc.ScissorEnable = true;
		throwIfFailed(_device->CreateRasterizerState(&rasterizerStateDesc, _cullCounterClockwiseRasterizerState.GetAddressOf()));

		rasterizerStateDesc.CullMode = D3D11_CULL_FRONT;
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.DepthClipEnable = true;
		rasterizerStateDesc.MultisampleEnable = true;
		rasterizerStateDesc.AntialiasedLineEnable = true;
		rasterizerStateDesc.ScissorEnable = true;
		throwIfFailed(_device->CreateRasterizerState(&rasterizerStateDesc, _cullClockwiseRasterizerState.GetAddressOf()));

		rasterizerStateDesc.CullMode = D3D11_CULL_NONE;
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.DepthClipEnable = true;
		rasterizerStateDesc.MultisampleEnable = true;
		rasterizerStateDesc.AntialiasedLineEnable = true;
		rasterizerStateDesc.ScissorEnable = true;
		throwIfFailed(_device->CreateRasterizerState(&rasterizerStateDesc, _cullNoneRasterizerState.GetAddressOf()));

		D3D11_SAMPLER_DESC samplerStateDesc = {};
		samplerStateDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		samplerStateDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerStateDesc.MinLOD = 0;
		samplerStateDesc.MaxLOD = 0;
		samplerStateDesc.MipLODBias = 0;
		samplerStateDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerStateDesc.MaxAnisotropy = 1;
		throwIfFailed(_device->CreateSamplerState(&samplerStateDesc, _pointWrapSamplerState.GetAddressOf()));
	}

	std::unique_ptr<IRenderSurface2D> DX11GraphicsDevice::InitializeSwapChain(int width, int height)
	{
		auto props = SDL_GetWindowProperties(g_Platform->GetSDL3Window());
		_handle = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

		DXGI_SWAP_CHAIN_DESC sd;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		if (!g_Configuration.EnableHighFramerate)
		{
			_refreshRate = 30;

			sd.BufferDesc.RefreshRate.Numerator = 0;
			sd.BufferDesc.RefreshRate.Denominator = 0;
		}
		else
		{
			_refreshRate = GetCurrentScreenRefreshRate();
			if (_refreshRate == 0)
			{
				_refreshRate = 60;
			}

			sd.BufferDesc.RefreshRate.Numerator = _refreshRate;
			sd.BufferDesc.RefreshRate.Denominator = 1;
		}
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
		sd.Windowed = true;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		sd.Flags = 0;
		sd.OutputWindow = _handle;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferCount = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		ComPtr<IDXGIDevice> dxgiDevice;
		throwIfFailed(_device.As(&dxgiDevice));

		ComPtr<IDXGIAdapter> dxgiAdapter;
		throwIfFailed(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), &dxgiAdapter));

		ComPtr<IDXGIFactory> dxgiFactory;
		throwIfFailed(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory));

		throwIfFailed(dxgiFactory->CreateSwapChain(_device.Get(), &sd, &_swapChain));

		dxgiFactory->MakeWindowAssociation(_handle, DXGI_MWA_NO_ALT_ENTER);

		// Initialize render targets
		ID3D11Texture2D* backBufferTexture = NULL;
		throwIfFailed(_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast <void**>(&backBufferTexture)));

		_viewportToolkit = Viewport(0, 0, width, height, 0.0f, 1.0f);

		_screenWidth = width;
		_screenHeight = height;

		auto result = std::make_unique<IRenderSurface2D>(
			std::move(std::make_unique<DX11RenderTarget2D>(_device.Get(), backBufferTexture)),
			std::move(std::make_unique<DX11DepthTarget>(_device.Get(), width, height, DXGI_FORMAT_D32_FLOAT)));

		_context->Flush();

		return result;
	}

	void DX11GraphicsDevice::CreateDevice()
	{
		TENLog("DirectX 11 renderer", LogLevel::Info);

		D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
		D3D_FEATURE_LEVEL featureLevel;
		HRESULT res;

		if constexpr (DEBUG_BUILD)
		{
			res = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG,
				levels, 1, D3D11_SDK_VERSION, &_device, &featureLevel, &_context);
		}
		else
		{
			res = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL,
				levels, 1, D3D11_SDK_VERSION, &_device, &featureLevel, &_context);
		}

		throwIfFailed(res);
	}

	std::string DX11GraphicsDevice::GetDefaultAdapterName()
	{
		IDXGIFactory* dxgiFactory = NULL;
		throwIfFailed(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&dxgiFactory));

		IDXGIAdapter* dxgiAdapter = NULL;

		dxgiFactory->EnumAdapters(0, &dxgiAdapter);

		DXGI_ADAPTER_DESC adapterDesc = {};

		dxgiAdapter->GetDesc(&adapterDesc);
		dxgiFactory->Release();

		return TEN::Utils::ToString(adapterDesc.Description);
	}

	AdapterInfo DX11GraphicsDevice::GetAdapterInfo()
	{
		IDXGIFactory* dxgiFactory = NULL;
		throwIfFailed(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&dxgiFactory));

		IDXGIAdapter* dxgiAdapter = NULL;
		dxgiFactory->EnumAdapters(0, &dxgiAdapter);

		DXGI_ADAPTER_DESC adapterDesc = {};
		dxgiAdapter->GetDesc(&adapterDesc);

		dxgiAdapter->Release();
		dxgiFactory->Release();

		auto info = AdapterInfo{};
		info.Name = TEN::Utils::ToString(adapterDesc.Description);
		info.VendorId = adapterDesc.VendorId;
		info.DeviceId = adapterDesc.DeviceId;
		info.SubSysId = adapterDesc.SubSysId;
		info.Revision = adapterDesc.Revision;
		info.DedicatedVideoMemory = adapterDesc.DedicatedVideoMemory;
		info.DedicatedSystemMemory = adapterDesc.DedicatedSystemMemory;
		info.SharedSystemMemory = adapterDesc.SharedSystemMemory;

		return info;
	}

	void DX11GraphicsDevice::ResizeSwapChain(int width, int height)
	{
		ID3D11RenderTargetView* nullViews[] = { nullptr };
		_context->OMSetRenderTargets(0, nullViews, NULL);
		_context->Flush();
		_context->ClearState();

		IDXGIOutput* output;
		throwIfFailed(_swapChain->GetContainingOutput(&output));

		DXGI_SWAP_CHAIN_DESC scd;
		throwIfFailed(_swapChain->GetDesc(&scd));

		unsigned int numModes = 1024;
		DXGI_MODE_DESC modes[1024];
		throwIfFailed(output->GetDisplayModeList(scd.BufferDesc.Format, 0, &numModes, modes));

		DXGI_MODE_DESC* mode = &modes[0];
		for (unsigned int i = 0; i < numModes; i++)
		{
			mode = &modes[i];
			if (mode->Width == width && mode->Height == height)
				break;
		}

		throwIfFailed(_swapChain->ResizeTarget(mode));

		_screenWidth = width;
		_screenHeight = height;

		_viewportToolkit = Viewport(0, 0, width, height, 0.0f, 1.0f);
	}

	std::unique_ptr<IShader> DX11GraphicsDevice::CreateShader(ShaderCompileRequest& req)
	{
		auto nativeShader = std::make_unique<DX11Shader>();

		auto baseFileName = req.SourceDirectory + req.FileName;
		auto prefix = ((req.CompileIndex < 10) ? L"0" : L"") + std::to_wstring(req.CompileIndex) + L"_";

		// VS
		auto makeCsoName = [&](const std::string& shaderType) {
			return req.BinaryDirectory + prefix + req.FileName + L"." +
				std::wstring(shaderType.begin(), shaderType.end()) + L".cso";
			};

		auto macros = ToD3DMacros(req.Macros);

		auto compileOne = [&](const std::string& shaderType,
			const std::wstring& entry,
			const char* model,
			ID3D10Blob** outBlob)
			{
				auto csoFileName = makeCsoName(shaderType);
				auto srcFileName = baseFileName;

				auto srcFileNameWithExt = srcFileName + L".hlsl";
				if (!std::filesystem::exists(srcFileNameWithExt))
				{
					srcFileNameWithExt = srcFileName + L".fx";
				}

				bool loadedFromDisk = false;
				if (!req.ForceRecompile && std::filesystem::exists(csoFileName))
				{
					auto csoTime = std::filesystem::last_write_time(csoFileName);
					auto srcTime = std::filesystem::last_write_time(srcFileNameWithExt);
					if (srcTime < csoTime)
					{
						std::ifstream ifs(csoFileName, std::ios::binary);
						if (ifs)
						{
							ifs.seekg(0, std::ios::end);
							auto size = ifs.tellg();
							ifs.seekg(0, std::ios::beg);
							std::vector<char> buf(size);
							ifs.read(buf.data(), size);
							D3DCreateBlob((SIZE_T)size, outBlob);
							memcpy((*outBlob)->GetBufferPointer(), buf.data(), size);
							loadedFromDisk = true;
						}
					}
				}

				if (!loadedFromDisk)
				{
					UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#ifdef _DEBUG
					flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
					flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_IEEE_STRICTNESS;
#endif

					ComPtr<ID3D10Blob> errors;
					auto target = shaderType + TEN::Utils::ToString(entry);
					auto hr = D3DCompileFromFile(
						srcFileNameWithExt.c_str(),
						macros.data(),
						D3D_COMPILE_STANDARD_FILE_INCLUDE,
						target.c_str(),
						model,
						flags,
						0,
						outBlob,
						&errors
					);
					if (FAILED(hr))
					{
						if (errors)
						{
							TENLog((const char*)errors->GetBufferPointer(), LogLevel::Error);
						}
						throwIfFailed(hr);
					}

					std::ofstream ofs(csoFileName, std::ios::binary);
					if (ofs)
					{
						ofs.write((const char*)(*outBlob)->GetBufferPointer(), (*outBlob)->GetBufferSize());
					}
				}
			};

		if (req.Type == ShaderType::Pixel || req.Type == ShaderType::PixelAndVertex)
		{
			auto blob = nativeShader->GetD3D10Blob();
			compileOne("PS", req.EntryPoint, "ps_5_0", &blob);
			ComPtr<ID3D11PixelShader> ps;
			throwIfFailed(_device->CreatePixelShader(blob->GetBufferPointer(),
				blob->GetBufferSize(),
				nullptr,
				&ps));
			nativeShader->SetD3D11PixelShader(ps);
		}

		if (req.Type == ShaderType::Vertex || req.Type == ShaderType::PixelAndVertex)
		{
			ComPtr<ID3D10Blob> vsBlob;
			compileOne("VS", req.EntryPoint, "vs_5_0", vsBlob.GetAddressOf());
			ComPtr<ID3D11VertexShader> vs;
			throwIfFailed(_device->CreateVertexShader(vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(),
				nullptr,
				&vs));
			nativeShader->SetD3D11VertexShader(vs);
			nativeShader->SetD3D10Blob(vsBlob);
		}

		if (req.Type == ShaderType::Geometry)
		{
			ComPtr<ID3D10Blob> gsBlob;
			compileOne("GS", req.EntryPoint, "gs_5_0", gsBlob.GetAddressOf());
			ComPtr<ID3D11GeometryShader> gs;
			throwIfFailed(_device->CreateGeometryShader(gsBlob->GetBufferPointer(),
				gsBlob->GetBufferSize(),
				nullptr,
				&gs));
			nativeShader->SetD3D11GeometryShader(gs);
			nativeShader->SetD3D10Blob(gsBlob);
		}

		return nativeShader;
	}


	void DX11GraphicsDevice::BindShader(ShaderStage shaderStage, IShader* shader, bool forceNull)
	{
		auto* nativeShader = static_cast<DX11Shader*>(shader);

		if (!nativeShader)
		{
			if (forceNull)
			{
				_context->VSSetShader(nullptr, nullptr, 0);

				switch (shaderStage)
				{
				case ShaderStage::VertexShader:
					_context->VSSetShader(nullptr, nullptr, 0);
					break;
				case ShaderStage::GeometryShader:
					_context->GSSetShader(nullptr, nullptr, 0);
					break;
				case ShaderStage::PixelShader:
					_context->PSSetShader(nullptr, nullptr, 0);
					break;
				}
			}
			return;
		}

		switch (shaderStage)
		{
		case ShaderStage::VertexShader:
			if (nativeShader->GetD3D11VertexShader() || forceNull)
				_context->VSSetShader(nativeShader->GetD3D11VertexShader(), nullptr, 0);
			break;
		case ShaderStage::GeometryShader:
			if (nativeShader->GetD3D11GeometryShader() || forceNull)
				_context->GSSetShader(nativeShader->GetD3D11GeometryShader(), nullptr, 0);
			break;
		case ShaderStage::PixelShader:
			if (nativeShader->GetD3D11PixelShader() || forceNull)
				_context->PSSetShader(nativeShader->GetD3D11PixelShader(), nullptr, 0);
			break;
		}
	}

	void DX11GraphicsDevice::Present()
	{
		_swapChain->Present(1, 0);
	}

	void DX11GraphicsDevice::ClearState()
	{
		_context->ClearState();
	}

	std::unique_ptr<ISpriteFont> DX11GraphicsDevice::InitializeSpriteFont(std::wstring fontPath)
	{
		return std::make_unique<DX11SpriteFont>(_device.Get(), fontPath);
	}

	std::unique_ptr<ISpriteBatch> DX11GraphicsDevice::InitializeSpriteBatch()
	{
		return std::make_unique<DX11SpriteBatch>(_device.Get(), _context.Get());
	}

	std::unique_ptr<IPrimitiveBatch> DX11GraphicsDevice::InitializePrimitiveBatch()
	{
		return std::make_unique<DX11PrimitiveBatch>(_context.Get());
	}

	void DX11GraphicsDevice::SaveScreenshot(IRenderTarget2D* renderTarget, std::wstring path)
	{
		auto nativeRenderTarget = static_cast<DX11RenderTarget2D*>(renderTarget);
		SaveWICTextureToFile(_context.Get(), nativeRenderTarget->GetD3D11Texture(), GUID_ContainerFormatPng, path.c_str(), 
			&GUID_WICPixelFormat24bppBGR, nullptr, true);
	}

	Vector3 DX11GraphicsDevice::Unproject(Vector3 position, Matrix projection, Matrix view, Matrix world)
	{
		return _viewportToolkit.Unproject(position, projection, view, world);
	}

	void DX11GraphicsDevice::Flush()
	{
		_context->Flush();
	}

	void DX11GraphicsDevice::UnbindAllRenderTargets()
	{
		ID3D11RenderTargetView* nullViews[] = { nullptr };
		_context->OMSetRenderTargets(0, nullViews, NULL);
	}

	void DX11GraphicsDevice::UpdateTexture2D(ITexture2D* texture, std::vector<char> data)
	{
		auto nativeTexture = static_cast<DX11Texture2D*>(texture);
		auto d3dTexture = nativeTexture->GetD3D11Texture();
		
		int width = texture->GetWidth();
		int height = texture->GetHeight();

		auto mappedResource = D3D11_MAPPED_SUBRESOURCE{};
		if (d3dTexture && SUCCEEDED(_context->Map(d3dTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
		{
			// Copy framebuffer row by row, otherwise skewing may occur.
			unsigned char* pData = reinterpret_cast<unsigned char*>(mappedResource.pData);
			for (int row = 0; row < height; row++)
				memcpy(pData + row * mappedResource.RowPitch, data.data() + row * width * 4, width * 4);
			_context->Unmap(d3dTexture, 0);
		}
		else
		{
			TENLog("Failed to render video texture", LogLevel::Error);
		}

	}

	int DX11GraphicsDevice::GetRefreshRate()
	{
		return _refreshRate;
	}

	int DX11GraphicsDevice::GetScreenWidth()
	{
		return _screenWidth;
	}

	int DX11GraphicsDevice::GetScreenHeight()
	{
		return _screenHeight;
	}
}

#endif
