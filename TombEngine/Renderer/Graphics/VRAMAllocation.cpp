#include "framework.h"
#include "Renderer/Graphics/VRAMAllocation.h"

namespace TEN::Renderer::Graphics
{
	VRAMAllocation::VRAMAllocation(VRAMCategory category, int sizeInBytes, const std::string& debugInfo)
		: _category(category), _size(sizeInBytes)
	{
		if (_size > 0)
			VRAMTracker::Get().Add(_category, _size, debugInfo);
	}

	VRAMAllocation::~VRAMAllocation()
	{
		if (_size > 0 && VRAMTracker::IsAlive())
			VRAMTracker::Get().Remove(_category, _size);
	}

	VRAMAllocation::VRAMAllocation(VRAMAllocation&& other) noexcept
		: _category(other._category), _size(other._size)
	{
		other._size = 0;
	}

	VRAMAllocation& VRAMAllocation::operator=(VRAMAllocation&& other) noexcept
	{
		if (this != &other)
		{
			// Release current allocation.
			if (_size > 0 && VRAMTracker::IsAlive())
				VRAMTracker::Get().Remove(_category, _size);

			_category = other._category;
			_size = other._size;
			other._size = 0;
		}

		return *this;
	}

	int VRAMAllocation::GetSize() const
	{
		return _size;
	}
}
