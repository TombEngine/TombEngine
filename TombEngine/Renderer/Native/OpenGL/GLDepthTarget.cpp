#include "framework.h"

#ifdef USE_OPENGL

#include "Renderer/Native/OpenGL/GLDepthTarget.h"
#include "Renderer/Native/OpenGL/GLUtils.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"

namespace TEN::Renderer::Native::OpenGL
{
	GLDepthTarget::GLDepthTarget(int width, int height, GLenum depthFormat)
	{
		_width = width;
		_height = height;
		_arraySize = 1;
		_format = depthFormat;

		glCreateTextures(GL_TEXTURE_2D, 1, &_texture);
		glTextureStorage2D(_texture, 1, depthFormat, width, height);
		glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		auto sizeStr = std::to_string(width) + "x" + std::to_string(height) + " " + GLFormatToString(depthFormat);
		ThrowIfGLError("CreateDepthTarget (" + sizeStr + ")");

		int vramSize = ComputeTextureSize(width, height, 1, 1, depthFormat);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, vramSize,
			"DepthTarget allocated: " + sizeStr + " (" + BytesToMBString(vramSize) + " MB)");
	}

	GLDepthTarget::GLDepthTarget(int width, int height, int arraySize, GLenum depthFormat)
	{
		_width = width;
		_height = height;
		_arraySize = arraySize;
		_format = depthFormat;

		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &_texture);
		glTextureStorage3D(_texture, 1, depthFormat, width, height, arraySize);
		glTextureParameteri(_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(_texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(_texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		auto sizeStr = std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(arraySize) +
			" " + GLFormatToString(depthFormat);
		ThrowIfGLError("CreateDepthTarget array (" + sizeStr + ")");

		int vramSize = ComputeTextureSize(width, height, arraySize, 1, depthFormat);
		_vram = VRAMAllocation(VRAMCategory::RenderTarget, vramSize,
			"DepthTarget array allocated: " + sizeStr + " (" + BytesToMBString(vramSize) + " MB)");
	}

	GLDepthTarget::~GLDepthTarget()
	{
		if (_texture)
			glDeleteTextures(1, &_texture);
	}
}

#endif
