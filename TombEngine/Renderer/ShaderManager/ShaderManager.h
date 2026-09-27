#pragma once

#include "Renderer/Graphics/IGraphicsDevice.h"

using namespace TEN::Renderer::Graphics;

namespace TEN::Renderer::Utils
{
	class ShaderManager
	{
	private:
		IGraphicsDevice* _graphicsDevice							   = nullptr;

		int											_compileCounter	   = 0;
		std::array<std::unique_ptr<IShader>, (int)Shader::Count>	_shaders		   = {};

	public:
		ShaderManager() = default;
		~ShaderManager();

		const IShader* Get(Shader shader);

		void Initialize(IGraphicsDevice* graphicsDevice);
		void LoadShaders(int width, int height, bool recompileAAShaders = false);
		void Bind(Shader shader, bool forceNull = false);

	private:
		void LoadCommonShaders();
		void LoadPostprocessShaders();
		void LoadAAShaders(int width, int height, bool recompile);

		std::unique_ptr<IShader> LoadOrCompile(const std::string& fileName, const std::string& funcName, ShaderType type, std::map<std::string, std::string> defines, bool forceRecompile);
		void		   Load(Shader shader, const std::string& fileName, const std::string& funcName, ShaderType type, std::map<std::string, std::string> defines, bool forceRecompile = false);
		void		   Destroy(Shader shader);
	};
}
