#pragma once

#ifdef USE_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/ITexture2D.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;

	class GLTexture2D : public ITexture2D
	{
	protected:
		int            _width   = 0;
		int            _height  = 0;
		GLuint         _texture = 0;
		GLenum         _internalFormat = GL_RGBA8;
		bool           _ownsTexture = true;
		VRAMAllocation _vram;

	public:
		GLTexture2D() = default;
		~GLTexture2D();

		int GetWidth() override { return _width; }
		int GetHeight() override { return _height; }
		bool IsValid() override { return _texture != 0; }

		GLuint GetGLTexture() const { return _texture; }
		GLenum GetInternalFormat() const { return _internalFormat; }

		// From raw pixel data.
		GLTexture2D(int width, int height, GLenum internalFormat, GLenum pixelFormat, GLenum pixelType, void* data);
		// From file on disk.
		GLTexture2D(const std::string& fileName);
		// From file data in memory (DDS or PNG).
		GLTexture2D(int dataSize, unsigned char* data);
	};
}

#endif
