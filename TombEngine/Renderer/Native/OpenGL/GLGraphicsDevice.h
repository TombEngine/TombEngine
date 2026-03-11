#pragma once

#ifdef HAS_OPENGL

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <set>

#include "Renderer/Graphics/IGraphicsDevice.h"
#include "Renderer/Native/OpenGL/GLVertexBuffer.h"
#include "Renderer/Native/OpenGL/GLIndexBuffer.h"
#include "Renderer/Native/OpenGL/GLConstantBuffer.h"
#include "Renderer/Native/OpenGL/GLTexture2D.h"
#include "Renderer/Native/OpenGL/GLRenderTarget2D.h"
#include "Renderer/Native/OpenGL/GLDepthTarget.h"
#include "Renderer/Native/OpenGL/GLInputLayout.h"
#include "Renderer/Native/OpenGL/GLShader.h"
#include "Renderer/Native/OpenGL/GLSpriteBatch.h"
#include "Renderer/Native/OpenGL/GLPrimitiveBatch.h"
#include "Renderer/Native/OpenGL/GLSpriteFont.h"

using namespace TEN::Renderer::Graphics;
using namespace TEN::Renderer::Structures;
using namespace TEN::Math::Library;

namespace TEN::Renderer::Native::OpenGL
{
	class GLGraphicsDevice final : public IGraphicsDevice
	{
	private:
		SDL_GLContext _glContext = nullptr;

		GLuint _defaultVAO   = 0;
		GLuint _pipeline     = 0;
		GLenum _primitiveType = GL_TRIANGLES;

		// Sampler objects, keyed by SamplerStateRegister.
		std::map<SamplerStateRegister, GLuint> _samplers;

		// FBO cache: single-RT keyed by (colorTex << 32 | depthTex), MRT keyed by hash of all attachments.
		std::unordered_map<uint64_t, GLuint> _fboCache;
		std::unordered_map<uint64_t, GLuint> _mrtFboCache;

		// Currently bound input layout.
		GLInputLayout* _currentInputLayout = nullptr;

		int _screenWidth  = 0;
		int _screenHeight = 0;
		int _refreshRate  = 60;

		// Track currently bound FBO for ClearRenderTarget/ClearDepth.
		GLuint _currentFBO = 0;

		// Backbuffer FBO for blitting to default framebuffer in Present().
		GLuint _backbufferFBO = 0;

		GLuint GetOrCreateFBO(GLuint colorTex, GLuint depthTex, bool colorIsArray, int colorArrayIndex, bool depthIsArray, int depthArrayIndex);
		GLuint GetTextureHandle(ITextureBase* texture);
		std::string ReadShaderFile(const std::string& path);
		std::string ResolveIncludes(const std::string& source, const std::string& directory, std::set<std::string>& alreadyIncluded);
		GLuint CompileSeparableProgram(GLenum shaderType, const std::string& source, const std::string& label);

	public:
		~GLGraphicsDevice();

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

		std::unique_ptr<ITexture2D> CreateTexture2D(int width, int height, SurfaceFormat format, void* data) override;
		std::unique_ptr<ITexture2D> CreateTexture2DFromFile(const std::string fileName) override;
		std::unique_ptr<ITexture2D> CreateTexture2DFromFileInMemory(int dataSize, unsigned char* data) override;
		void UpdateTexture2D(ITexture2D* texture, std::vector<char> data) override;

		void SetBlendMode(BlendMode blendMode) override;
		void SetDepthState(DepthState depthState) override;
		void SetCullMode(CullMode cullMode) override;
		void SetScissor(RendererRectangle rectangle) override;
		void SetScissor(RendererViewport viewport) override;

		void BindTexture(TextureRegister registerType, ITextureBase* texture, SamplerStateRegister samplerType) override;

		std::unique_ptr<IConstantBuffer> CreateConstantBuffer(int size, std::string name) override;
		void UpdateConstantBuffer(IConstantBuffer* constantBuffer, void* data) override;
		void BindConstantBuffer(ShaderStage shaderStage, ConstantBufferRegister constantBufferType, IConstantBuffer* buffer) override;

		void DrawIndexedTriangles(int count, int baseIndex, int baseVertex) override;
		void DrawIndexedInstancedTriangles(int count, int instances, int baseIndex, int baseVertex) override;
		void DrawInstancedTriangles(int count, int instances, int baseVertex) override;
		void DrawTriangles(int count, int baseVertex) override;

		void ClearRenderTarget2D(IRenderTarget2D* renderTarget, Vector4 clearColor) override;
		void ClearRenderTarget2D(IRenderTarget2D* renderTarget, int arrayIndex, Vector4 clearColor) override;

		void ClearDepthStencil(IDepthTarget* renderTarget, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil) override;
		void ClearDepthStencil(IDepthTarget* renderTarget, int arrayIndex, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil) override;

		void BindRenderTarget(IRenderTarget2D* renderTarget, IDepthTarget* depthTarget) override;
		void BindRenderTarget(IRenderTargetBinding renderTarget, IDepthTargetBinding depthTarget) override;
		void BindRenderTargets(std::vector<IRenderTarget2D*> renderTargets, IDepthTarget* depthTarget) override;
		void BindRenderTargets(std::vector<IRenderTargetBinding> renderTargets, IDepthTargetBinding depthTarget) override;

		void SetPrimitiveType(PrimitiveType primitiveType) override;

		void SetInputLayout(IInputLayout* inputLayout) override;
		std::unique_ptr<IInputLayout> CreateInputLayout(std::vector<RendererInputLayoutField> fields, IShader* shader) override;

		void CreateDevice() override;
		void Initialize() override;
		void ReleaseContext() override;
		void BindContext() override;
		std::unique_ptr<IRenderSurface2D> InitializeSwapChain(int width, int height) override;
		std::string GetDefaultAdapterName() override;
		AdapterInfo GetAdapterInfo() override;
		void ResizeSwapChain(int width, int height) override;

		std::unique_ptr<IShader> CreateShader(ShaderCompileRequest& request) override;
		void BindShader(ShaderStage shaderStage, IShader* shader, bool forceNull) override;

		void Present() override;
		void ClearState() override;
		void ClearDefaultFramebuffer() override;

		std::unique_ptr<ISpriteFont> InitializeSpriteFont(std::string fontPath) override;
		std::unique_ptr<ISpriteBatch> InitializeSpriteBatch() override;
		std::unique_ptr<IPrimitiveBatch> InitializePrimitiveBatch() override;

		void SetViewport(RendererViewport viewport) override;
		Vector3 Unproject(Vector3 position, Matrix projection, Matrix view, Matrix world) override;

		void SaveScreenshot(IRenderTarget2D* renderTarget, std::string path) override;

		void Flush() override;
		void UnbindAllRenderTargets() override;

		int GetRefreshRate() override;
		int GetScreenWidth() override;
		int GetScreenHeight() override;

		bool NeedsFBOYFlip() const override { return true; }
	};
}

#endif
