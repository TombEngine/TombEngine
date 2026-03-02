#include "framework.h"

#ifdef HAS_OPENGL

#include "Renderer/Native/OpenGL/GLConstantBuffer.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"

namespace TEN::Renderer::Native::OpenGL
{
	GLConstantBuffer::GLConstantBuffer(int size, std::wstring name)
	{
		_size = size;

		glCreateBuffers(1, &_ubo);
		glNamedBufferData(_ubo, size, nullptr, GL_DYNAMIC_DRAW);

		ThrowIfGLError("CreateConstantBuffer (" + std::to_string(size) + " bytes)");
	}

	GLConstantBuffer::~GLConstantBuffer()
	{
		if (_ubo)
			glDeleteBuffers(1, &_ubo);
	}

	void GLConstantBuffer::UpdateData(void* data)
	{
		void* dataPtr = glMapNamedBufferRange(_ubo, 0, _size,
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		if (dataPtr)
		{
			memcpy(dataPtr, data, _size);
			glUnmapNamedBuffer(_ubo);
		}
		else
		{
			TENLog("Could not update constant buffer.", LogLevel::Error);
		}
	}
}

#endif
