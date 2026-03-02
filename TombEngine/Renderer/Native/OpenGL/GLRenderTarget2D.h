#pragma once

#ifdef HAS_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/IRenderTarget2D.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;

	class GLRenderTarget2D : public IRenderTarget2D
	{
	private:
		int            _width          = 0;
		int            _height         = 0;
		int            _arraySize      = 1;
		GLuint         _texture        = 0;
		GLenum         _internalFormat = GL_RGBA8;
		bool           _ownsTexture    = true;
		VRAMAllocation _vram;

	public:
		GLRenderTarget2D() = default;
		~GLRenderTarget2D();

		int GetWidth() override { return _width; }
		int GetHeight() override { return _height; }
		int GetArraySize() override { return _arraySize; }
		bool IsValid() override { return _texture != 0; }

		GLuint GetGLTexture() const { return _texture; }
		GLenum GetInternalFormat() const { return _internalFormat; }
		bool IsArray() const { return _arraySize > 1; }

		// Single render target.
		GLRenderTarget2D(int width, int height, GLenum internalFormat, bool isTypeless);
		// Texture view from parent (SMAA: different format view of same texture).
		GLRenderTarget2D(GLuint parentTexture, int width, int height, GLenum viewFormat);
		// Backbuffer wrapper (no owned texture - just wraps default framebuffer).
		GLRenderTarget2D(int width, int height);
		// Texture array render target.
		GLRenderTarget2D(int width, int height, int arraySize, GLenum internalFormat);
	};
}

#endif
