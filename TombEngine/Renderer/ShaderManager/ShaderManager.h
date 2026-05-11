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

		// Shader-group dedup: shaders compiled from the same (file, entry, type, defines)
		// produce byte-identical binaries and don't need a device rebind even when their
		// Shader enum value differs. Each Load() assigns a group id; Bind() skips the
		// device call if the last bound group matches.
		std::array<int, (int)Shader::Count> _shaderGroups = {};
		std::map<std::string, int>          _groupKeyToId;
		int                                 _nextGroupId      = 0;
		int                                 _lastBoundGroup   = -1;

	public:
		ShaderManager() = default;
		~ShaderManager();

		const IShader* Get(Shader shader);

		void Initialize(IGraphicsDevice* graphicsDevice);
		void LoadShaders(int width, int height, bool recompileAAShaders = false);
		void Bind(Shader shader, bool forceNull = false);

		// Invalidates the dedup cache (call at frame boundaries or when external code
		// may have changed the bound shader behind our back).
		void ResetBindCache() { _lastBoundGroup = -1; }

	private:
		void LoadCommonShaders();
		void LoadPostprocessShaders();
		void LoadAAShaders(int width, int height, bool recompile);

		std::unique_ptr<IShader> LoadOrCompile(const std::string& fileName, const std::string& funcName, ShaderType type, std::map<std::string, std::string> defines, bool forceRecompile);
		void		   Load(Shader shader, const std::string& fileName, const std::string& funcName, ShaderType type, std::map<std::string, std::string> defines, bool forceRecompile = false);
		void		   Destroy(Shader shader);

		int AssignShaderGroup(const std::string& fileName, const std::string& funcName, ShaderType type, const std::map<std::string, std::string>& defines);
	};
}
