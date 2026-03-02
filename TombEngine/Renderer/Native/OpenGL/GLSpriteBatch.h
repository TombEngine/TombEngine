#pragma once

#ifdef USE_OPENGL

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
	};
}

#endif
