#pragma once

#ifdef USE_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/IDepthTarget.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;

	class GLDepthTarget : public IDepthTarget
	{
	private:
		int            _width      = 0;
		int            _height     = 0;
		int            _arraySize  = 1;
		GLuint         _texture    = 0;
		GLenum         _format     = GL_DEPTH_COMPONENT32F;
		VRAMAllocation _vram;

	public:
		GLDepthTarget() = default;
		~GLDepthTarget();

		int GetArraySize() override { return _arraySize; }

		GLuint GetGLTexture() const { return _texture; }
		GLenum GetFormat() const { return _format; }
		bool IsArray() const { return _arraySize > 1; }

		GLDepthTarget(int width, int height, GLenum depthFormat);
		GLDepthTarget(int width, int height, int arraySize, GLenum depthFormat);
	};
}

#endif
