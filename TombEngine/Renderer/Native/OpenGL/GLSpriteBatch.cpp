#include "framework.h"

#ifdef HAS_OPENGL

#include "Renderer/Native/OpenGL/GLSpriteBatch.h"
#include "Renderer/Native/OpenGL/GLErrorHelper.h"

namespace TEN::Renderer::Native::OpenGL
{
	static const char* s_spriteBatchVS = R"(
		#version 450 core
		layout(location=0) in vec2 aPos;
		layout(location=1) in vec2 aUV;
		layout(location=2) in vec4 aColor;
		out vec2 vUV;
		out vec4 vColor;
		uniform vec2 uScreenSize;
		void main() {
			gl_Position = vec4(
				aPos.x / uScreenSize.x * 2.0 - 1.0,
				-(aPos.y / uScreenSize.y * 2.0 - 1.0),
				0.0, 1.0);
			vUV = aUV;
			vColor = aColor;
		}
	)";

	static const char* s_spriteBatchFS = R"(
		#version 450 core
		in vec2 vUV;
		in vec4 vColor;
		layout(binding=0) uniform sampler2D uTexture;
		layout(location=0) out vec4 FragColor;
		layout(location=1) out vec4 FragColor1;
		void main() {
			FragColor = texture(uTexture, vUV) * vColor;
			FragColor1 = vec4(0.0);
		}
	)";

	static GLuint CompileInlineShader(GLenum type, const char* src)
	{
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		GLint ok = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if (!ok)
		{
			char log[512];
			glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
			TENLog(std::string("SpriteBatch shader compile error: ") + log, LogLevel::Error);
		}
		return shader;
	}

	GLSpriteBatch::GLSpriteBatch()
	{
		// Build shader program.
		GLuint vs = CompileInlineShader(GL_VERTEX_SHADER, s_spriteBatchVS);
		GLuint fs = CompileInlineShader(GL_FRAGMENT_SHADER, s_spriteBatchFS);
		_program = glCreateProgram();
		glAttachShader(_program, vs);
		glAttachShader(_program, fs);
		glLinkProgram(_program);

		GLint linkOk = 0;
		glGetProgramiv(_program, GL_LINK_STATUS, &linkOk);
		if (!linkOk)
		{
			char log[512];
			glGetProgramInfoLog(_program, sizeof(log), nullptr, log);
			TENLog(std::string("SpriteBatch program link error: ") + log, LogLevel::Error);
		}

		glDeleteShader(vs);
		glDeleteShader(fs);

		// Create VAO and VBO.
		glCreateVertexArrays(1, &_vao);
		glCreateBuffers(1, &_vbo);
		glNamedBufferData(_vbo, 6 * 8 * sizeof(float) * 256, nullptr, GL_DYNAMIC_DRAW);

		// Vertex format: vec2 pos, vec2 uv, vec4 color = 8 floats.
		int stride = 8 * sizeof(float);
		glVertexArrayVertexBuffer(_vao, 0, _vbo, 0, stride);
		glEnableVertexArrayAttrib(_vao, 0);
		glEnableVertexArrayAttrib(_vao, 1);
		glEnableVertexArrayAttrib(_vao, 2);
		glVertexArrayAttribFormat(_vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
		glVertexArrayAttribFormat(_vao, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
		glVertexArrayAttribFormat(_vao, 2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float));
		glVertexArrayAttribBinding(_vao, 0, 0);
		glVertexArrayAttribBinding(_vao, 1, 0);
		glVertexArrayAttribBinding(_vao, 2, 0);

		// Create sampler.
		glCreateSamplers(1, &_sampler);
		glSamplerParameteri(_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glSamplerParameteri(_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	GLSpriteBatch::~GLSpriteBatch()
	{
		if (_program) glDeleteProgram(_program);
		if (_vao) glDeleteVertexArrays(1, &_vao);
		if (_vbo) glDeleteBuffers(1, &_vbo);
		if (_sampler) glDeleteSamplers(1, &_sampler);
	}

	GLuint GLSpriteBatch::GetTextureHandle(ITextureBase* texture)
	{
		if (auto* tex = dynamic_cast<GLTexture2D*>(texture))
			return tex->GetGLTexture();
		if (auto* rt = dynamic_cast<GLRenderTarget2D*>(texture))
			return rt->GetGLTexture();
		return 0;
	}

	void GLSpriteBatch::Begin(SpriteSortingMode sortingMode, BlendMode blendMode)
	{
		_queue.clear();
		_blendMode = blendMode;
	}

	void GLSpriteBatch::Draw(ITextureBase* texture, RendererRectangle area, Vector4 color)
	{
		SpriteBatchQuad quad;
		quad.TextureHandle = GetTextureHandle(texture);
		quad.Left = (float)area.Left;
		quad.Top = (float)area.Top;
		quad.Right = (float)area.Right;
		quad.Bottom = (float)area.Bottom;
		quad.Color = color;
		_queue.push_back(quad);
	}

	void GLSpriteBatch::DrawWithUV(GLuint textureHandle, RendererRectangle area, float u0, float v0, float u1, float v1, Vector4 color)
	{
		SpriteBatchQuad quad;
		quad.TextureHandle = textureHandle;
		quad.Left = (float)area.Left;
		quad.Top = (float)area.Top;
		quad.Right = (float)area.Right;
		quad.Bottom = (float)area.Bottom;
		quad.U0 = u0;
		quad.V0 = v0;
		quad.U1 = u1;
		quad.V1 = v1;
		quad.Color = color;
		_queue.push_back(quad);
	}

	void GLSpriteBatch::Flush()
	{
		if (_queue.empty())
			return;

		// Build vertex data: 6 vertices per quad (2 triangles).
		std::vector<float> vertices;
		vertices.reserve(_queue.size() * 6 * 8);

		auto addVertex = [&](float x, float y, float u, float v, const Vector4& c)
		{
			vertices.push_back(x); vertices.push_back(y);
			vertices.push_back(u); vertices.push_back(v);
			vertices.push_back(c.x); vertices.push_back(c.y);
			vertices.push_back(c.z); vertices.push_back(c.w);
		};

		GLuint currentTex = 0;

		// Sort by texture for batching. Use stable_sort to preserve draw order
		// (shadow before text) for quads that share the same texture.
		std::stable_sort(_queue.begin(), _queue.end(),
			[](const SpriteBatchQuad& a, const SpriteBatchQuad& b) { return a.TextureHandle < b.TextureHandle; });

		// Save GL state that we modify.
		GLint prevVAO = 0;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
		GLint prevPipeline = 0;
		glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING, &prevPipeline);
		GLint prevProgram = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
		GLboolean prevCullFace = glIsEnabled(GL_CULL_FACE);
		GLboolean prevBlend = glIsEnabled(GL_BLEND);
		GLboolean prevDepth = glIsEnabled(GL_DEPTH_TEST);

		// Unbind any active pipeline so the linked program takes clean precedence.
		glBindProgramPipeline(0);

		// Activate SpriteBatch's own linked program.
		glUseProgram(_program);

		// Query the current viewport for the orthographic projection.
		// This matches DirectXTK's behavior of using the actual viewport dimensions
		// rather than cached values, ensuring correctness even if the render target changes.
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);
		float vpW = (float)viewport[2];
		float vpH = (float)viewport[3];

		// Upload screen size for the vertex shader's orthographic projection.
		glUniform2f(glGetUniformLocation(_program, "uScreenSize"), vpW, vpH);

		glBindVertexArray(_vao);
		glBindSampler(0, _sampler);

		// Set 2D rendering state.
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);

		if (_blendMode == BlendMode::AlphaBlend || _blendMode == BlendMode::FastAlphaBlend)
		{
			glEnable(GL_BLEND);
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBlendEquation(GL_FUNC_ADD);
		}

		for (size_t i = 0; i < _queue.size(); i++)
		{
			const auto& q = _queue[i];

			addVertex(q.Left,  q.Top,    q.U0, q.V0, q.Color);
			addVertex(q.Right, q.Top,    q.U1, q.V0, q.Color);
			addVertex(q.Right, q.Bottom, q.U1, q.V1, q.Color);
			addVertex(q.Left,  q.Top,    q.U0, q.V0, q.Color);
			addVertex(q.Right, q.Bottom, q.U1, q.V1, q.Color);
			addVertex(q.Left,  q.Bottom, q.U0, q.V1, q.Color);
		}

		glNamedBufferData(_vbo, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

		int vertexOffset = 0;
		currentTex = _queue[0].TextureHandle;

		for (size_t i = 0; i <= _queue.size(); i++)
		{
			bool flush = (i == _queue.size()) || (_queue[i].TextureHandle != currentTex);
			if (flush)
			{
				int count = ((int)i * 6) - vertexOffset;
				glBindTextureUnit(0, currentTex);
				glDrawArrays(GL_TRIANGLES, vertexOffset, count);

				if (i < _queue.size())
				{
					currentTex = _queue[i].TextureHandle;
					vertexOffset = (int)i * 6;
				}
			}
		}

		// Restore GL state.
		if (prevCullFace) glEnable(GL_CULL_FACE);
		if (prevDepth) glEnable(GL_DEPTH_TEST);
		if (prevBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);

		glUseProgram(prevProgram);
		glBindProgramPipeline(prevPipeline);
		glBindSampler(0, 0);
		glBindVertexArray(prevVAO);
	}

	void GLSpriteBatch::End()
	{
		Flush();
	}
}

#endif
