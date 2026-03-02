#include "framework.h"

#ifdef USE_OPENGL

#include "Renderer/Native/OpenGL/GLInputLayout.h"
#include "Renderer/Native/OpenGL/GLUtils.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"

namespace TEN::Renderer::Native::OpenGL
{
	GLInputLayout::GLInputLayout(std::vector<RendererInputLayoutField> fields)
		: _fields(std::move(fields))
	{
		_totalStride = 0;
		for (const auto& field : _fields)
			_totalStride += GetVertexAttribByteSize(field.Format);
	}

	void GLInputLayout::Apply(GLuint vao, GLuint vbo) const
	{
		int offset = 0;

		for (GLuint location = 0; location < (GLuint)_fields.size(); location++)
		{
			const auto& field = _fields[location];
			int components = GetVertexAttribComponentCount(field.Format);
			GLenum type = GetVertexAttribGLType(field.Format);
			int byteSize = GetVertexAttribByteSize(field.Format);

			glEnableVertexArrayAttrib(vao, location);
			glVertexArrayVertexBuffer(vao, location, vbo, 0, _totalStride);

			if (IsIntegerFormat(field.Format))
			{
				glVertexArrayAttribIFormat(vao, location, components, type, offset);
			}
			else if (IsNormalizedFormat(field.Format))
			{
				glVertexArrayAttribFormat(vao, location, components, type, GL_TRUE, offset);
			}
			else
			{
				glVertexArrayAttribFormat(vao, location, components, type, GL_FALSE, offset);
			}

			glVertexArrayAttribBinding(vao, location, location);

			offset += byteSize;
		}
	}
}

#endif
