#pragma once

#ifdef HAS_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/ISpriteBatch.h"
#include "Renderer/Native/OpenGL/GLTexture2D.h"
#include "Renderer/Native/OpenGL/GLRenderTarget2D.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;

	struct SpriteBatchQuad
	{
		GLuint TextureHandle;
		float Left, Top, Right, Bottom;
		float U0 = 0, V0 = 0, U1 = 1, V1 = 1;
		Vector4 Color;
	};

	class GLSpriteBatch final : public ISpriteBatch
	{
	private:
		GLuint _vao       = 0;
		GLuint _vbo       = 0;
		GLuint _program   = 0;
		GLuint _sampler   = 0;
		int    _screenW   = 0;
		int    _screenH   = 0;

		std::vector<SpriteBatchQuad> _queue;
		BlendMode _blendMode = BlendMode::Opaque;

		void Flush();
		GLuint GetTextureHandle(ITextureBase* texture);

	public:
		GLSpriteBatch();
		~GLSpriteBatch();

		void SetScreenSize(int w, int h) { _screenW = w; _screenH = h; }

		void Begin(SpriteSortingMode sortingMode, BlendMode blendMode) override;
		void End() override;
		void Draw(ITextureBase* texture, RendererRectangle area, Vector4 color) override;
		void DrawWithUV(GLuint textureHandle, RendererRectangle area, float u0, float v0, float u1, float v1, Vector4 color);
	};
}

#endif
