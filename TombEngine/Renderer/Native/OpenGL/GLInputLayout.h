#pragma once

#ifdef HAS_OPENGL

#include <glad/glad.h>
#include "Renderer/Graphics/IInputLayout.h"
#include "Renderer/Structures/RendererInputLayout.h"

namespace TEN::Renderer::Native::OpenGL
{
	using namespace TEN::Renderer::Graphics;
	using namespace TEN::Renderer::Structures;

	class GLInputLayout final : public IInputLayout
	{
	private:
		std::vector<RendererInputLayoutField> _fields;
		int _totalStride = 0;

	public:
		GLInputLayout() = default;
		~GLInputLayout() = default;

		GLInputLayout(std::vector<RendererInputLayoutField> fields);

		const std::vector<RendererInputLayoutField>& GetFields() const { return _fields; }
		int GetTotalStride() const { return _totalStride; }

		void Apply(GLuint vao, GLuint vbo) const;
	};
}

#endif
