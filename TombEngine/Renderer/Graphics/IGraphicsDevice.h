#pragma once
#include <vector>
#include "Specific/fast_vector.h"
#include <string>
#include <SimpleMath.h>
#include "Renderer/Graphics/IIndexBuffer.h"
#include "Renderer/Graphics/IVertexBuffer.h"
#include "Renderer/Graphics/IRenderTarget2D.h"
#include "Renderer/Graphics/IRenderTargetCube.h"
#include "Renderer/Graphics/ITexture2D.h"
#include "Renderer/Graphics/IConstantBuffer.h"
#include "Renderer/Graphics/IInputLayout.h"
#include "Renderer/Graphics/IShader.h"
#include "Renderer/Graphics/IDepthTarget.h"
#include "Renderer/Graphics/IRenderSurface2D.h"
#include "Renderer/Graphics/IPrimitiveBatch.h"
#include "Renderer/Graphics/ISpriteBatch.h"
#include "Renderer/Graphics/ISpriteFont.h"
#include "Renderer/RendererEnums.h"
#include "Renderer/Graphics/AdapterInfo.h"
#include "Renderer/Structures/RendererRectangle.h"
#include "Renderer/Structures/RendererInputLayout.h"
#include "Renderer/Structures/RendererViewport.h"

using namespace TEN::Renderer::Structures;
using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace TEN::Renderer::Graphics
{
	class IGraphicsDevice
	{
	public:
		virtual std::unique_ptr<IVertexBuffer> CreateVertexBuffer(int numVertices, int vertexSize, void* data) = 0;
		virtual void UpdateVertexBuffer(IVertexBuffer* vertexBuffer, int startVertex, int count, void* data) = 0;
		virtual void BindVertexBuffer(IVertexBuffer* vertexBuffer) = 0;

		virtual std::unique_ptr<IIndexBuffer> CreateIndexBuffer(int numIndices, int* data) = 0;
		virtual void UpdateIndexBuffer(IIndexBuffer* indexBuffer, int numIndices, int startIndex, int* data) = 0;
		virtual void BindIndexBuffer(IIndexBuffer* indexBuffer) = 0;

		virtual std::unique_ptr<IRenderSurface2D> CreateRenderSurface2D(int width, int height, SurfaceFormat colorFormat, bool isTypeless, DepthFormat depthFormat) = 0;
		virtual std::unique_ptr<IRenderSurface2D> CreateRenderSurface2D(int width, int height, int arraySize, SurfaceFormat colorFormat, DepthFormat depthFormat) = 0;
		virtual std::unique_ptr<IRenderSurface2D> CreateRenderSurface2D(IRenderSurface2D* parentRenderTarget, SurfaceFormat colorFormat) = 0;

		virtual IRenderTargetCube* CreateRenderTargetCube(int size, SurfaceFormat colorFormat) = 0;

		virtual std::unique_ptr<ITexture2D> CreateTexture2D(int width, int height, SurfaceFormat format, void* data, bool isDynamic = false) = 0;
		virtual std::unique_ptr<ITexture2D> CreateTexture2DFromFile(const std::string fileName) = 0;
		virtual std::unique_ptr<ITexture2D> CreateTexture2DFromFileInMemory(int dataSize, unsigned char* data) = 0;
		virtual void UpdateTexture2D(ITexture2D* texture, std::vector<char> data) = 0;

		virtual void SetBlendMode(BlendMode blendMode) = 0;
		virtual void SetDepthState(DepthState depthState) = 0;
		virtual void SetCullMode(CullMode cullMode) = 0;
		virtual void SetScissor(RendererRectangle rectangle) = 0;
		virtual void SetScissor(RendererViewport viewport) = 0;

		virtual void BindTexture(TextureRegister registerType, ITextureBase* texture, SamplerStateRegister samplerType) = 0;
		
		virtual std::unique_ptr<IConstantBuffer> CreateConstantBuffer(int size, std::wstring name) = 0;
		virtual void UpdateConstantBuffer(IConstantBuffer* constantBuffer, void* data) = 0;
		virtual void BindConstantBuffer(ShaderStage shaderStage, ConstantBufferRegister constantBufferType, IConstantBuffer* buffer) = 0;
		
		virtual void DrawIndexedTriangles(int count, int baseIndex, int baseVertex) = 0;
		virtual void DrawIndexedInstancedTriangles(int count, int instances, int baseIndex, int baseVertex) = 0;
		virtual void DrawInstancedTriangles(int count, int instances, int baseVertex) = 0;
		virtual void DrawTriangles(int count, int baseVertex) = 0;

		virtual void ClearRenderTarget2D(IRenderTarget2D* renderTarget, XMVECTORF32 clearColor) = 0;
		virtual void ClearRenderTarget2D(IRenderTarget2D* renderTarget, int arrayIndex, XMVECTORF32 clearColor) = 0;
		//virtual void ClearRenderTargetCube(IRenderTargetCube* textureCube, int faceIndex, XMVECTORF32 clearColor) = 0;
		
		virtual void ClearDepthStencil(IDepthTarget* depthTarget, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil) = 0;
		virtual void ClearDepthStencil(IDepthTarget* depthTarget, int arrayIndex, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil) = 0;

		virtual void BindRenderTarget(IRenderTarget2D* renderTarget, IDepthTarget* depthTarget) = 0;
		virtual void BindRenderTarget(IRenderTargetBinding renderTarget, IDepthTargetBinding depthTarget) = 0;
		virtual void BindRenderTargets(std::vector<IRenderTarget2D*> renderTargets, IDepthTarget* depthTarget) = 0;
		virtual void BindRenderTargets(std::vector<IRenderTargetBinding> renderTargets, IDepthTargetBinding depthTarget) = 0;

		virtual void SetPrimitiveType(PrimitiveType primitiveType) = 0;

		virtual void SetInputLayout(IInputLayout* inputLayout) = 0;
		virtual std::unique_ptr<IInputLayout> CreateInputLayout(std::vector<RendererInputLayoutField> fields, IShader* shader) = 0;

		virtual void CreateDevice() = 0;
		virtual void Initialize() = 0;
		virtual std::unique_ptr<IRenderSurface2D> InitializeSwapChain(int width, int height) = 0;
		virtual std::string GetDefaultAdapterName() = 0;
		virtual AdapterInfo GetAdapterInfo() = 0;
		virtual void ResizeSwapChain(int width, int height) = 0;

		virtual std::unique_ptr<IShader> CreateShader(ShaderCompileRequest& request) = 0;
		virtual void BindShader(ShaderStage shaderStage, IShader* shader, bool forceNull) = 0;

		virtual void Present() = 0;
		virtual void ClearState() = 0;

		virtual std::unique_ptr<ISpriteFont> InitializeSpriteFont(std::wstring fontPath) = 0;
		virtual std::unique_ptr<ISpriteBatch> InitializeSpriteBatch() = 0;
		virtual std::unique_ptr<IPrimitiveBatch> InitializePrimitiveBatch() = 0;

		virtual void SetViewport(RendererViewport viewport) = 0;
		virtual Vector3 Unproject(Vector3 position, Matrix projection, Matrix view, Matrix world) = 0;

		virtual void SaveScreenshot(IRenderTarget2D* renderTarget, std::wstring path) = 0;

		virtual void Flush() = 0;
		virtual void UnbindAllRenderTargets() = 0;

		virtual int GetRefreshRate() = 0;

		virtual int GetScreenWidth() = 0;
		virtual int GetScreenHeight() = 0;

		virtual ~IGraphicsDevice() = default;
	};
}
