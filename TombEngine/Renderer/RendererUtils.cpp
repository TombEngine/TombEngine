#include "framework.h"

#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"

namespace TEN::Renderer::Utils
{
	std::string GetAssetPath(const char* fileName)
	{
		return g_GameFlow->GetGameDir() + fileName;
	}
}
