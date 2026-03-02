#include "framework.h"

#ifdef USE_OPENGL

#include "Renderer/Native/OpenGL/GLShader.h"

namespace TEN::Renderer::Native::OpenGL
{
	GLShader::~GLShader()
	{
		if (_vsProgram) glDeleteProgram(_vsProgram);
		if (_fsProgram) glDeleteProgram(_fsProgram);
		if (_gsProgram) glDeleteProgram(_gsProgram);
	}
}

#endif
