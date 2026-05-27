#include "framework.h"
#include "Sound/SoundtrackRegistry.h"

using namespace TEN::Sound;

// Accessor defined in sound.cpp.
std::string Sound_GetFullAudioDirectory();

namespace TEN::Sound
{
	// Static registry instance.
	static SoundtrackRegistry g_SoundtrackRegistry;

	std::shared_ptr<SoundtrackObject> SoundtrackRegistry::GetOrCreate(const std::string& trackName, const SoundtrackOptions& options)
	{
		if (trackName.empty())
			return nullptr;

		// Return existing if found.
		auto it = _registry.find(trackName);
		if (it != _registry.end())
			return it->second;

		// Create new.
		auto soundtrack = std::make_shared<SoundtrackObject>(trackName, options);
		_registry[trackName] = soundtrack;
		return soundtrack;
	}

	std::shared_ptr<SoundtrackObject> SoundtrackRegistry::Get(const std::string& trackName) const
	{
		auto it = _registry.find(trackName);
		if (it != _registry.end())
			return it->second;

		return nullptr;
	}

	bool SoundtrackRegistry::Contains(const std::string& trackName) const
	{
		return _registry.find(trackName) != _registry.end();
	}

	void SoundtrackRegistry::Remove(const std::string& trackName)
	{
		auto it = _registry.find(trackName);
		if (it != _registry.end())
		{
			it->second->Stop();
			_registry.erase(it);
		}
	}

	void SoundtrackRegistry::Clear()
	{
		for (auto& [name, soundtrack] : _registry)
			soundtrack->Stop();

		_registry.clear();
	}

	std::vector<std::shared_ptr<SoundtrackObject>> SoundtrackRegistry::GetActiveSoundtracks() const
	{
		std::vector<std::shared_ptr<SoundtrackObject>> result;

		for (auto& [name, soundtrack] : _registry)
		{
			if (soundtrack->IsActive())
				result.push_back(soundtrack);
		}

		return result;
	}

	std::vector<std::shared_ptr<SoundtrackObject>> SoundtrackRegistry::GetPersistentSoundtracks() const
	{
		std::vector<std::shared_ptr<SoundtrackObject>> result;

		for (auto& [name, soundtrack] : _registry)
		{
			if (soundtrack->ShouldPersist())
				result.push_back(soundtrack);
		}

		return result;
	}

	SoundtrackRegistry& GetSoundtrackRegistry()
	{
		return g_SoundtrackRegistry;
	}

	std::string GetFullAudioDirectory()
	{
		return Sound_GetFullAudioDirectory();
	}
}
