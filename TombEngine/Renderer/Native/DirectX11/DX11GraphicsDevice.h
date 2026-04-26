#pragma once
#include "framework.h"

#ifdef SDL_PLATFORM_WIN32

#include "Renderer/Graphics/IGraphicsDevice.h"
#include <wrl/client.h>
#include <CommonStates.h>
#include <SpriteFont.h>
#include <PrimitiveBatch.h>
#include <SimpleMath.h>
#include "Renderer/Native/DirectX11/DX11IndexBuffer.h"
#include "Renderer/Native/DirectX11/DX11VertexBuffer.h"
#include "Renderer/Native/DirectX11/DX11RenderTarget2D.h"
#include "Renderer/Native/DirectX11/DX11Texture2D.h"
#include "Renderer/Native/DirectX11/DX11DepthTarget.h"
#include "Renderer/Native/DirectX11/DX11ConstantBuffer.h"
#include "Renderer/Native/DirectX11/DX11InputLayout.h"
#include "Renderer/Native/DirectX11/DX11Shader.h"
#include "Renderer/Native/DirectX11/DX11SpriteBatch.h"
#include "Renderer/Native/DirectX11/DX11PrimitiveBatch.h"
#include "Renderer/Native/DirectX11/DX11SpriteFont.h"

using namespace TEN::Renderer::Graphics;
using namespace TEN::Renderer::Structures;
using namespace DirectX::SimpleMath;
using namespace Microsoft::WRL;

namespace TEN::Renderer::Native::DirectX11
{
	class DX11GraphicsDevice final : public IGraphicsDevice
	{
	private:
		ComPtr<ID3D11Device> _device = nullptr;
		ComPtr<ID3D11DeviceContext> _context = nullptr;
		ComPtr<IDXGISwapChain> _swapChain = nullptr;

		std::unique_ptr<CommonStates> _renderStates = nullptr;

		ComPtr <ID3D11SamplerState> _pointWrapSamplerState = nullptr;
		ComPtr<ID3D11SamplerState> _shadowSampler;

		ComPtr<ID3D11BlendState> _subtractiveBlendState = nullptr;
		ComPtr<ID3D11BlendState> _screenBlendState = nullptr;
		ComPtr<ID3D11BlendState> _lightenBlendState = nullptr;
		ComPtr<ID3D11BlendState> _excludeBlendState = nullptr;
		ComPtr<ID3D11BlendState> _transparencyBlendState = nullptr;
		ComPtr<ID3D11BlendState> _finalTransparencyBlendState = nullptr;

		ComPtr<ID3D11RasterizerState> _cullCounterClockwiseRasterizerState = nullptr;
		ComPtr<ID3D11RasterizerState> _cullClockwiseRasterizerState = nullptr;
		ComPtr<ID3D11RasterizerState> _cullNoneRasterizerState = nullptr;
		
		ComPtr<ID3D11InputLayout> _inputLayout = nullptr;
		ComPtr<ID3D11InputLayout> _fullscreenTriangleInputLayout = nullptr;

		Viewport _viewportToolkit;

		int _screenWidth;
		int _screenHeight;
		int _refreshRate;
		HWND _handle;

		inline ID3D11DeviceContext* GetDeviceContext() { return _context.Get(); }

		std::vector<D3D_SHADER_MACRO> ToD3DMacros(const std::map<std::string, std::string>& m)
		{
			std::vector<D3D_SHADER_MACRO> out;
			out.reserve(m.size() + 1);
			for (auto& kv : m)
			{
				out.push_back(D3D_SHADER_MACRO{ kv.first.c_str(),
												kv.second.empty() ? nullptr : kv.second.c_str() });
			}
			out.push_back(D3D_SHADER_MACRO{ nullptr, nullptr });
			return out;
		}

		inline ID3D11ShaderResourceView* GetD3D11ShaderResourceView(ITextureBase* texture)
		{
			ID3D11ShaderResourceView* srv = nullptr;

			if (auto tex2D = dynamic_cast<DX11Texture2D*>(texture))
			{
				srv = tex2D->GetD3D11ShaderResourceView();
			}
			else if (auto rt2D = dynamic_cast<DX11RenderTarget2D*>(texture))
			{
				srv = rt2D->GetD3D11ShaderResourceView();
			}

			return srv;
		}

	public:
		~DX11GraphicsDevice() = default;

		std::unique_ptr<IVertexBuffer> CreateVertexBuffer(int numVertices, int vertexSize, void* data) override;
		void UpdateVertexBuffer(IVertexBuffer* vertexBuffer, int startVertex, int count, void* data) override;
		void BindVertexBuffer(IVertexBuffer* vertexBuffer) override;

		std::unique_ptr<IIndexBuffer> CreateIndexBuffer(int numIndices, int* data) override;
		void UpdateIndexBuffer(IIndexBuffer* indexBuffer, int numIndices, int startIndex, int* data) override;
		void BindIndexBuffer(IIndexBuffer* indexBuffer) override;

		std::unique_ptr<IRenderSurface2D> CreateRenderSurface2D(int width, int height, SurfaceFormat colorFormat, bool isTypeless, DepthFormat depthFormat) override;
		std::unique_ptr<IRenderSurface2D> CreateRenderSurface2D(int width, int height, int arraySize, SurfaceFormat colorFormat, DepthFormat depthFormat) override;
		std::unique_ptr<IRenderSurface2D> CreateRenderSurface2D(IRenderSurface2D* parentRenderTarget, SurfaceFormat colorFormat) override;

		IRenderTargetCube* CreateRenderTargetCube(int size, SurfaceFormat colorFormat) override;

		std::unique_ptr<ITexture2D> CreateTexture2D(int width, int height, SurfaceFormat format, void* data, bool isDynamic = false) override;
		std::unique_ptr<ITexture2D> CreateTexture2DFromFile(const std::string fileName) override;
		std::unique_ptr<ITexture2D> CreateTexture2DFromFileInMemory(int dataSize, unsigned char* data) override;
		
		void SetBlendMode(BlendMode blendMode) override;
		void SetDepthState(DepthState depthState) override;
		void SetCullMode(CullMode cullMode) override;
		void SetScissor(RendererRectangle rectangle) override;
		void SetScissor(RendererViewport viewport) override;

		void BindTexture(TextureRegister registerType, ITextureBase* texture, SamplerStateRegister samplerType) override;

		std::unique_ptr<IConstantBuffer> CreateConstantBuffer(int size, std::wstring name) override;
		void UpdateConstantBuffer(IConstantBuffer* constantBuffer, void* data) override;
		void BindConstantBuffer(ShaderStage shaderStage, ConstantBufferRegister constantBufferType, IConstantBuffer* buffer) override;

		void DrawIndexedTriangles(int count, int baseIndex, int baseVertex) override;
		void DrawIndexedInstancedTriangles(int count, int instances, int baseIndex, int baseVertex) override;
		void DrawInstancedTriangles(int count, int instances, int baseVertex) override;
		void DrawTriangles(int count, int baseVertex) override;

		void ClearRenderTarget2D(IRenderTarget2D* renderTarget, XMVECTORF32 clearColor) override;
		void ClearRenderTarget2D(IRenderTarget2D* renderTarget, int arrayIndex, XMVECTORF32 clearColor) override;
		//void ClearRenderTarget2DOfCube(IRenderTargetCube* textureCube, int index, XMVECTORF32 clearColor) override;

		void ClearDepthStencil(IDepthTarget* renderTarget, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil) override;
		void ClearDepthStencil(IDepthTarget* renderTarget, int arrayIndex, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil) override;

		void BindRenderTarget(IRenderTarget2D* renderTarget, IDepthTarget* depthTarget) override;
		void BindRenderTarget(IRenderTargetBinding renderTarget, IDepthTargetBinding depthTarget) override;
		void BindRenderTargets(std::vector<IRenderTarget2D*> renderTargets, IDepthTarget* depthTarget) override;
		void BindRenderTargets(std::vector<IRenderTargetBinding> renderTargets, IDepthTargetBinding depthTarget) override;

		void SetViewport(RendererViewport viewport) override;
		void SetPrimitiveType(PrimitiveType primitiveType) override;

		void SetInputLayout(IInputLayout* inputLayout) override;
		std::unique_ptr<IInputLayout> CreateInputLayout(std::vector<RendererInputLayoutField> fields, IShader* shader) override;

		void CreateDevice() override;
		void Initialize() override;
		std::unique_ptr<IRenderSurface2D> InitializeSwapChain(int width, int height) override;
		std::string GetDefaultAdapterName() override;
		AdapterInfo GetAdapterInfo() override;
		void ResizeSwapChain(int width, int height) override;

		std::unique_ptr<IShader> CreateShader(ShaderCompileRequest& request) override;
		void BindShader(ShaderStage shaderStage, IShader* shader, bool forceNull) override;

		void Present() override;
		void ClearState() override;

		std::unique_ptr<ISpriteFont> InitializeSpriteFont(std::wstring fontPath) override;
		std::unique_ptr<ISpriteBatch> InitializeSpriteBatch() override;
		std::unique_ptr<IPrimitiveBatch> InitializePrimitiveBatch() override;

		Vector3 Unproject(Vector3 position, Matrix projection, Matrix view, Matrix world) override;

		void SaveScreenshot(IRenderTarget2D* renderTarget, std::wstring path) override;

		void Flush() override;
		void UnbindAllRenderTargets() override;

		void UpdateTexture2D(ITexture2D* texture, std::vector<char> data) override;

		int GetRefreshRate() override;

		int GetScreenWidth() override;
		int GetScreenHeight() override;
	};
}

#endif
