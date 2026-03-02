#pragma once

#ifdef HAS_OPENGL

#include "Renderer/Graphics/AdapterInfo.h"

namespace TEN::Renderer::Native::OpenGL
{
	using GLLoadProc = void* (*)(const char*);

	// Returns the platform-appropriate GL function loader for GLAD.
	GLLoadProc GetPlatformGLLoader();

	// Queries adapter info using the best available platform method.
	TEN::Renderer::Graphics::AdapterInfo GetPlatformAdapterInfo();
}

#endif
