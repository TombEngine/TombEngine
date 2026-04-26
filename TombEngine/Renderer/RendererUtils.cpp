#include "framework.h"

#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/trutils.h"

namespace TEN::Renderer::Utils
{
	std::wstring GetAssetPath(const wchar_t* fileName)
	{
		return TEN::Utils::ToWString(g_GameFlow->GetGameDir()) + fileName;
	}
}
