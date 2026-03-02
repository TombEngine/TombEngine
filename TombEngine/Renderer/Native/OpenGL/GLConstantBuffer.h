#pragma once

#ifdef HAS_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/IConstantBuffer.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;

	class GLConstantBuffer final : public IConstantBuffer
	{
	private:
		GLuint _ubo  = 0;
		int    _size = 0;

	public:
		GLConstantBuffer() = default;
		~GLConstantBuffer();

		GLuint GetGLBuffer() const { return _ubo; }
		int GetSize() const { return _size; }

		GLConstantBuffer(int size, std::wstring name);
		void UpdateData(void* data);
	};
}

#endif
