#include "framework.h"

#ifdef HAS_OPENGL

#include "Renderer/Native/OpenGL/GLIndexBuffer.h"
#include "Renderer/Native/OpenGL/GLUtils.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"

namespace TEN::Renderer::Native::OpenGL
{
	GLIndexBuffer::GLIndexBuffer(int numIndices, int* indices)
	{
		_numIndices = numIndices;

		int bufferSize = sizeof(int) * (numIndices > 0 ? numIndices : 1);

		glCreateBuffers(1, &_ibo);
		glNamedBufferData(_ibo, bufferSize,
			(numIndices > 0 && indices) ? indices : nullptr,
			GL_DYNAMIC_DRAW);

		ThrowIfGLError("CreateIndexBuffer (" + std::to_string(numIndices) + " indices)");

		_vram = VRAMAllocation(VRAMCategory::IndexBuffer, bufferSize,
			"IndexBuffer allocated: " + std::to_string(numIndices) + " indices (" +
			BytesToMBString(bufferSize) + " MB)");
	}

	GLIndexBuffer::~GLIndexBuffer()
	{
		if (_ibo)
			glDeleteBuffers(1, &_ibo);
	}

	bool GLIndexBuffer::Update(int* data, int startIndex, int count)
	{
		void* ptr = glMapNamedBufferRange(_ibo, 0, (GLsizeiptr)count * sizeof(int),
			GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

		if (ptr)
		{
			std::memcpy(ptr, &data[startIndex], (size_t)count * sizeof(int));
			glUnmapNamedBuffer(_ibo);
			return true;
		}

		TENLog("Could not update index buffer!", LogLevel::Error);
		return false;
	}
}

#endif
