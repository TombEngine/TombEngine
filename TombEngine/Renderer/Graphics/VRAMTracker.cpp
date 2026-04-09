#include "framework.h"
#include "Renderer/Graphics/VRAMTracker.h"

#include <iomanip>
#include <sstream>

#include "Game/Debug/Debug.h"

using namespace TEN::Debug;

namespace TEN::Renderer::Graphics
{
	static bool s_trackerAlive = false;

	VRAMTracker::~VRAMTracker()
	{
		s_trackerAlive = false;
	}

	VRAMTracker& VRAMTracker::Get()
	{
		static VRAMTracker instance;
		s_trackerAlive = true;
		return instance;
	}

	bool VRAMTracker::IsAlive()
	{
		return s_trackerAlive;
	}

	void VRAMTracker::SetAdapterInfo(const AdapterInfo& info)
	{
		auto lock = std::lock_guard(_mutex);
		_adapterInfo = info;
	}

	const AdapterInfo& VRAMTracker::GetAdapterInfo() const
	{
		return _adapterInfo;
	}

	void VRAMTracker::Add(VRAMCategory category, int bytes, const std::string& debugInfo)
	{
		auto lock = std::lock_guard(_mutex);
		_usage[category] += bytes;

#if _DEBUG
		if (!debugInfo.empty())
			TENLog("[VRAM] " + debugInfo, LogLevel::Info, LogConfig::Debug);
#endif
	}

	void VRAMTracker::Remove(VRAMCategory category, int bytes)
	{
		auto lock = std::lock_guard(_mutex);
		_usage[category] -= bytes;
	}

	int VRAMTracker::GetTotal() const
	{
		auto lock = std::lock_guard(_mutex);

		int total = 0;
		for (const auto& [cat, bytes] : _usage)
			total += bytes;

		return total;
	}

	int VRAMTracker::GetCategory(VRAMCategory category) const
	{
		auto lock = std::lock_guard(_mutex);

		auto it = _usage.find(category);
		if (it != _usage.end())
			return it->second;

		return 0;
	}

	std::string VRAMTracker::GetSummary() const
	{
		auto lock = std::lock_guard(_mutex);

		auto ss = std::stringstream();

		int total = 0;
		for (const auto& [cat, bytes] : _usage)
			total += bytes;

		auto toMB = [](int bytes) { return bytes / (1024.0f * 1024.0f); };

		auto catName = [](VRAMCategory cat)
		{
			switch (cat)
			{
			case VRAMCategory::Texture:      return "Textures";
			case VRAMCategory::RenderTarget: return "Render Targets";
			case VRAMCategory::VertexBuffer: return "Vertex Buffers";
			case VRAMCategory::IndexBuffer:  return "Index Buffers";
			default:                         return "Unknown";
			}
		};

		ss << "VRAM Total: " << std::fixed << std::setprecision(2) << toMB(total) << " MB";
		for (const auto& [cat, bytes] : _usage)
			ss << " | " << catName(cat) << ": " << std::fixed << std::setprecision(2) << toMB(bytes) << " MB";

		return ss.str();
	}
}
