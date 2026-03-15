#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUShader.h"

namespace TEN::Renderer::Native::SDLGPU
{
	SDLGPUShader::~SDLGPUShader()
	{
		if (_device)
		{
			if (_vertexShader)
				SDL_ReleaseGPUShader(_device, _vertexShader);
			if (_fragmentShader)
				SDL_ReleaseGPUShader(_device, _fragmentShader);
		}
	}
}

#endif
