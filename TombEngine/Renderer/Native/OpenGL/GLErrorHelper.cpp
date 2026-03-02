#include "framework.h"

#ifdef USE_OPENGL

#include "Renderer/Native/OpenGL/GLErrorHelper.h"

namespace TEN::Renderer::Native::OpenGL
{
	static void APIENTRY GLDebugCallback(
		GLenum source, GLenum type, GLuint id, GLenum severity,
		GLsizei length, const GLchar* message, const void* userParam)
	{
		// Ignore non-significant notifications.
		if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
			return;

		std::string sourceStr;
		switch (source)
		{
		case GL_DEBUG_SOURCE_API:             sourceStr = "API"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   sourceStr = "Window"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceStr = "Shader"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:     sourceStr = "ThirdParty"; break;
		case GL_DEBUG_SOURCE_APPLICATION:      sourceStr = "App"; break;
		default:                              sourceStr = "Other"; break;
		}

		std::string typeStr;
		switch (type)
		{
		case GL_DEBUG_TYPE_ERROR:               typeStr = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeStr = "Deprecated"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  typeStr = "UndefinedBehavior"; break;
		case GL_DEBUG_TYPE_PORTABILITY:         typeStr = "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE:         typeStr = "Performance"; break;
		default:                               typeStr = "Other"; break;
		}

		auto level = (severity == GL_DEBUG_SEVERITY_HIGH) ? LogLevel::Error : LogLevel::Warning;
		TENLog("[GL " + sourceStr + "/" + typeStr + "] " + std::string(message), level);
	}

	void InitGLDebugOutput()
	{
		if (GLAD_GL_KHR_debug || GLAD_GL_VERSION_4_3)
		{
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback(GLDebugCallback, nullptr);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
		}
	}
}

#endif
