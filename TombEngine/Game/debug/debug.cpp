#include "framework.h"
#include "Game/Debug/Debug.h"

#include <chrono>
#include <spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <stdarg.h>

#include "Renderer/Renderer.h"

using TEN::Renderer::g_Renderer;

namespace TEN::Debug
{
	static auto StartTime = std::chrono::high_resolution_clock::time_point{};

	void InitTENLog(const std::string& logDirContainingDir)
	{
		// "true" means create new log file each time game is run.
		auto logPath = logDirContainingDir + "Logs/TENLog.txt";
		auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath, true);

		auto logger = std::shared_ptr<spdlog::logger>();

		// Set file and console log targets.
		auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		logger = std::make_shared<spdlog::logger>(std::string("multi_sink"), spdlog::sinks_init_list{ fileSink, consoleSink });

		spdlog::initialize_logger(logger);
		logger->set_level(spdlog::level::info);
		logger->flush_on(spdlog::level::info);
		logger->set_pattern("[%Y-%b-%d %T] [%^%l%$] %v");
	}

	void ShutdownTENLog()
	{
		spdlog::shutdown();
	}

	void TENLog(const std::string_view& msg, LogLevel level, LogConfig config, bool allowSpam)
	{
		static auto prevString = std::string();
		if (prevString == msg && !allowSpam)
			return;

		if constexpr (!DEBUG_BUILD)
		{
			if (config == LogConfig::Debug)
				return;
		}

		auto logger = spdlog::get("multi_sink");

		if (!logger)
			return;

		switch (level)
		{
		case LogLevel::Error:
			logger->error(msg);
			break;

		case LogLevel::Warning:
			logger->warn(msg);
			break;

		case LogLevel::Info:
			logger->info(msg);
			break;
		}

		logger->flush();

		prevString = std::string(msg);
	}

	void StartDebugTimer()
	{
		StartTime = std::chrono::high_resolution_clock::now();
	}

	void EndDebugTimer()
	{
		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - StartTime);
		
		PrintDebugMessage("Execution (microseconds): %d", duration);
	}

	void PrintDebugMessage(LPCSTR msg, ...)
	{
		auto args = va_list{};
		va_start(args, msg);
		g_Renderer.PrintDebugMessage(msg, args);
		va_end(args);
	}
	
	void DrawDebugString(const std::string& string, const Vector2& pos, const Color& color, float scale, RendererDebugPage page)
	{
		g_Renderer.AddDebugString(string, pos, color, scale, page);
	}

	void DrawDebug2DLine(const Vector2& origin, const Vector2& target, const Color& color, RendererDebugPage page)
	{
		g_Renderer.AddLine2D(origin, target, color, page);
	}

	void DrawDebugLine(const Vector3& origin, const Vector3& target, const Color& color, RendererDebugPage page)
	{
		g_Renderer.AddDebugLine(origin, target, color, page);
	}

	void DrawDebugTriangle(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Color& color, RendererDebugPage page)
	{
		g_Renderer.AddDebugTriangle(vertex0, vertex1, vertex2, color, page);
	}

	void DrawDebugTarget(const Vector3& center, const Quaternion& orient, float radius, const Color& color, RendererDebugPage page)
	{
		g_Renderer.AddDebugTarget(center, orient, radius, color, page);
	}

	void DrawDebugBox(const std::array<Vector3, BOX_VERTEX_COUNT>& corners, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		g_Renderer.AddDebugBox(corners, color, page, isWireframe);
	}

	void DrawDebugBox(const Vector3& min, const Vector3& max, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		g_Renderer.AddDebugBox(min, max, color, page, isWireframe);
	}

	void DrawDebugBox(const BoundingOrientedBox& box, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		g_Renderer.AddDebugBox(box, color, page, isWireframe);
	}

	void DrawDebugBox(const BoundingBox& box, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		g_Renderer.AddDebugBox(box, color, page, isWireframe);
	}

	void DrawDebugCone(const Vector3& center, const Quaternion& orient, float radius, float length, const Vector4& color, RendererDebugPage page, bool isWireframe)
	{
		g_Renderer.AddDebugCone(center, orient, radius, length, color, page, isWireframe);
	}

	void DrawDebugCylinder(const Vector3& center, const Quaternion& orient, float radius, float length, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		g_Renderer.AddDebugCylinder(center, orient, radius, length, color, page, isWireframe);
	}

	void DrawDebugSphere(const Vector3& center, float radius, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		g_Renderer.AddDebugSphere(center, radius, color, page, isWireframe);
	}

	void DrawDebugSphere(const BoundingSphere& sphere, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		g_Renderer.AddDebugSphere(sphere, color, page, isWireframe);
	}
}
