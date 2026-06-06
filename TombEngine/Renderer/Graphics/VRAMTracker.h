#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include "Renderer/Graphics/AdapterInfo.h"

namespace TEN::Renderer::Graphics
{
	enum class VRAMCategory
	{
		Texture,
		RenderTarget,
		VertexBuffer,
		IndexBuffer
	};

	class VRAMTracker
	{
	public:
		~VRAMTracker();

		static VRAMTracker& Get();
		static bool IsAlive();

		void SetAdapterInfo(const AdapterInfo& info);
		const AdapterInfo& GetAdapterInfo() const;

		void Add(VRAMCategory category, int bytes, const std::string& debugInfo);
		void Remove(VRAMCategory category, int bytes);

		int GetTotal() const;
		int GetCategory(VRAMCategory category) const;
		std::string GetSummary() const;

	private:
		mutable std::mutex _mutex;
		std::unordered_map<VRAMCategory, int> _usage;
		AdapterInfo _adapterInfo;
	};
}
