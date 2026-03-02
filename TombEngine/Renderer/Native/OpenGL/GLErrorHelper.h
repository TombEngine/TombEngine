#pragma once

#ifdef HAS_OPENGL

#include <glad/glad.h>
#include <string>
#include "Game/Debug/Debug.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Debug;

	void InitGLDebugOutput();

	inline std::string GLErrorToString(GLenum err)
	{
		switch (err)
		{
		case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
		case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
		case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
		default:                               return "GL_ERROR_" + std::to_string(err);
		}
	}

	constexpr int MAX_GL_ERROR_DRAIN = 32;

	inline void DrainGLErrors()
	{
		for (int i = 0; i < MAX_GL_ERROR_DRAIN && glGetError() != GL_NO_ERROR; i++) {}
	}

	inline void CheckGLError(const std::string& context)
	{
		GLenum err;
		for (int i = 0; i < MAX_GL_ERROR_DRAIN && (err = glGetError()) != GL_NO_ERROR; i++)
			TENLog("OpenGL error in " + context + ": " + GLErrorToString(err), LogLevel::Error);
	}

	inline void ThrowIfGLError(const std::string& context)
	{
		GLenum firstErr = GL_NO_ERROR;
		GLenum err;
		for (int i = 0; i < MAX_GL_ERROR_DRAIN && (err = glGetError()) != GL_NO_ERROR; i++)
		{
			if (firstErr == GL_NO_ERROR)
				firstErr = err;
		}

		if (firstErr != GL_NO_ERROR)
			throw std::runtime_error("OpenGL error in " + context + ": " + GLErrorToString(firstErr));
	}
}

#endif
