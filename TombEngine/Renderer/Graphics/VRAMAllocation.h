#pragma once

#include <string>
#include "Renderer/Graphics/VRAMTracker.h"

namespace TEN::Renderer::Graphics
{
	class VRAMAllocation
	{
	public:
		VRAMAllocation() = default;
		VRAMAllocation(VRAMCategory category, int sizeInBytes, const std::string& debugInfo);
		~VRAMAllocation();

		// Move support.
		VRAMAllocation(VRAMAllocation&& other) noexcept;
		VRAMAllocation& operator=(VRAMAllocation&& other) noexcept;

		// No copy.
		VRAMAllocation(const VRAMAllocation&) = delete;
		VRAMAllocation& operator=(const VRAMAllocation&) = delete;

		int GetSize() const;

	private:
		VRAMCategory _category = VRAMCategory::Texture;
		int _size = 0;
	};
}
