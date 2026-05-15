#include "framework.h"
#include "Game/Debug/Debug.h"

#include "Renderer/Renderer.h"

using TEN::Renderer::g_Renderer;

namespace TEN::Debug
{
	constexpr auto MAX_LOG_FILES = 20;

	static auto startTime = std::chrono::high_resolution_clock::time_point{};
	static auto prevString = std::string();

	static std::string GetTimestampString()
	{
		auto now = std::chrono::system_clock::now();
		auto t = std::chrono::system_clock::to_time_t(now);

		std::tm tmBuf{};

#ifdef SDL_PLATFORM_WIN32
		localtime_s(&tmBuf, &t);
#else
		localtime_r(&t, &tmBuf);
#endif

		std::ostringstream oss;
		oss << std::put_time(&tmBuf, "%Y-%m-%d_%H-%M-%S");
		return oss.str();
	}

	static void CleanupOldLogs(const std::filesystem::path& logDir)
	{
		std::vector<std::filesystem::directory_entry> logFiles;

		if (!std::filesystem::exists(logDir))
			return;

		for (const auto& entry : std::filesystem::directory_iterator(logDir))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".txt")
				logFiles.push_back(entry);
		}

		if (logFiles.size() <= MAX_LOG_FILES)
			return;

		std::sort(logFiles.begin(), logFiles.end(),
			[](const auto& a, const auto& b)
			{
				return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
			});

		// Delete oldest until we have MAX_LOG_FILES.
		while (logFiles.size() > MAX_LOG_FILES)
		{
			try
			{
				std::filesystem::remove(logFiles.front());
			}
			catch (...) {}
			logFiles.erase(logFiles.begin());
		}
	}

	void InitTENLog(const std::string& logDirContainingDir)
	{
		auto logDir = std::filesystem::path(logDirContainingDir) / "Logs";
		std::filesystem::create_directories(logDir);

		// Remove oldest logs if over the limit.
		CleanupOldLogs(logDir);

		// Create unique filename.
		std::string filename = "TENLog_" + GetTimestampString() + ".txt";
		auto logPath = logDir / filename;

		auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
		auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

		auto logger = std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{ fileSink, consoleSink });

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
		startTime = std::chrono::high_resolution_clock::now();
	}

	void EndDebugTimer()
	{
		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
		
		PrintDebugMessage("Execution (microseconds): %d", duration);
	}

	void PrintDebugMessage(const char* msg, ...)
	{
		auto args = va_list{};
		va_start(args, msg);
		g_Renderer.PrintDebugMessage(msg, args);
		va_end(args);
	}

	void DrawDebugString(const std::string& string, const Vector3& pos, const Vector4& color, RendererDebugPage page)
	{
		constexpr float MIN_SCALE = 0.2f;
		constexpr float MAX_SCALE = 0.8f;

		float distance = (Camera.pos.ToVector3() - pos).Length();
		float scale = 1.0f / (distance / BLOCK(2));

		if (scale < MIN_SCALE)
			return;

		scale = std::clamp(scale, MIN_SCALE, MAX_SCALE);

		// Get 2D label position.
		auto labelPos = pos - Vector3(0, CLICK(0.75f), 0);
		auto labelPos2D = g_Renderer.Get2DPosition(labelPos);

		// Draw label.
		if (labelPos2D.has_value())
			DrawDebugString(string, *labelPos2D, color, scale, page);
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
