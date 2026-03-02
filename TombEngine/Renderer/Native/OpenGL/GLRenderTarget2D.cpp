#include "framework.h"

#ifdef HAS_OPENGL

#include "Renderer/Native/OpenGL/GLRenderTarget2D.h"
#include "Renderer/Native/OpenGL/GLUtils.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"

namespace TEN::Renderer::Native::OpenGL
{
	// Single render target.
	GLRenderTarget2D::GLRenderTarget2D(int width, int height, GLenum internalFormat, bool isTypeless)
	{
		_width = width;
		_height = height;
		_arraySize = 1;
		_internalFormat = internalFormat;

		glCreateTextures(GL_TEXTURE_2D, 1, &_texture);
		glTextureStorage2D(_texture, 1, internalFormat, width, height);
		glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		auto sizeStr = std::to_string(width) + "x" + std::to_string(height) + " " + GLFormatToString(internalFormat);
		ThrowIfGLError("CreateRenderTarget (" + sizeStr + ")");

		int vramSize = ComputeTextureSize(width, height, 1, 1, internalFormat);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, vramSize,
			"RenderTarget allocated: " + sizeStr + " (" + BytesToMBString(vramSize) + " MB)");
	}

	// Texture view from parent (SMAA: different format view of same texture).
	GLRenderTarget2D::GLRenderTarget2D(GLuint parentTexture, int width, int height, GLenum viewFormat)
	{
		_width = width;
		_height = height;
		_arraySize = 1;
		_internalFormat = viewFormat;

		glGenTextures(1, &_texture);
		glTextureView(_texture, GL_TEXTURE_2D, parentTexture, viewFormat, 0, 1, 0, 1);
		glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		ThrowIfGLError("CreateRenderTarget texture view (" + GLFormatToString(viewFormat) + ")");
		// No VRAM allocation — shares parent's storage.
	}

	// Backbuffer wrapper.
	GLRenderTarget2D::GLRenderTarget2D(int width, int height)
	{
		_width = width;
		_height = height;
		_arraySize = 1;
		_internalFormat = GL_RGBA8;

		// Create a real color texture for the backbuffer. We can't use FBO 0
		// with a custom depth texture, so we render to this texture and blit
		// to FBO 0 in Present().
		glCreateTextures(GL_TEXTURE_2D, 1, &_texture);
		glTextureStorage2D(_texture, 1, GL_RGBA8, width, height);
		glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// Texture array render target.
	GLRenderTarget2D::GLRenderTarget2D(int width, int height, int arraySize, GLenum internalFormat)
	{
		_width = width;
		_height = height;
		_arraySize = arraySize;
		_internalFormat = internalFormat;

		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &_texture);
		glTextureStorage3D(_texture, 1, internalFormat, width, height, arraySize);
		glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		auto sizeStr = std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(arraySize) +
			" " + GLFormatToString(internalFormat);
		ThrowIfGLError("CreateRenderTarget array (" + sizeStr + ")");

		int vramSize = ComputeTextureSize(width, height, arraySize, 1, internalFormat);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, vramSize,
			"RenderTarget array allocated: " + sizeStr + " (" + BytesToMBString(vramSize) + " MB)");
	}

	GLRenderTarget2D::~GLRenderTarget2D()
	{
		if (_texture && _ownsTexture)
			glDeleteTextures(1, &_texture);
	}
}

#endif
