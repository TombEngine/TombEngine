#pragma once

#include <iostream>
#include <stdexcept>
#include <string_view>

#include "Renderer/RendererEnums.h"

namespace TEN::Debug
{
#if _DEBUG
	constexpr auto DEBUG_BUILD = true;
#else
	constexpr auto DEBUG_BUILD = false;
#endif

	enum class LogLevel
	{
		Error,
		Warning,
		Info
	};

	enum class LogConfig
	{
		Debug,
		All
	};

	class TENScriptException : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	// Logs

	void InitTENLog(const std::string& logDirContainingDir);
	void ShutdownTENLog();
	void TENLog(const std::string_view& msg, LogLevel level = LogLevel::Info, LogConfig config = LogConfig::All, bool allowSpam = false);

	inline void TENAssert(const bool& cond, const std::string& msg)
	{
		if constexpr (DEBUG_BUILD)
		{
			if (!cond)
			{
				TENLog(msg, LogLevel::Error);
				throw std::runtime_error(msg);
			}
		}
	};

	// Timers

	void StartDebugTimer();
	void EndDebugTimer();

	// Objects

	void PrintDebugMessage(LPCSTR msg, ...);
	void DrawDebugString(const std::string& string, const Vector2& pos, const Color& color, float scale, RendererDebugPage page = RendererDebugPage::None);
	void DrawDebug2DLine(const Vector2& origin, const Vector2& target, const Color& color, RendererDebugPage page = RendererDebugPage::None);
	void DrawDebugLine(const Vector3& origin, const Vector3& target, const Color& color, RendererDebugPage page = RendererDebugPage::None);
	void DrawDebugTriangle(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Color& color, RendererDebugPage page = RendererDebugPage::None);
	void DrawDebugTarget(const Vector3& center, const Quaternion& orient, float radius, const Color& color, RendererDebugPage page = RendererDebugPage::None);
	void DrawDebugBox(const std::array<Vector3, BOX_VERTEX_COUNT>& corners, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
	void DrawDebugBox(const Vector3& min, const Vector3& max, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
	void DrawDebugBox(const BoundingOrientedBox& box, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
	void DrawDebugBox(const BoundingBox& box, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
	void DrawDebugCone(const Vector3& center, const Quaternion& orient, float radius, float length, const Vector4& color, RendererDebugPage page, bool isWireframe = true);
	void DrawDebugCylinder(const Vector3& center, const Quaternion& orient, float radius, float length, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
	void DrawDebugSphere(const Vector3& center, float radius, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
	void DrawDebugSphere(const BoundingSphere& sphere, const Color& color, RendererDebugPage page = RendererDebugPage::None, bool isWireframe = true);
}
