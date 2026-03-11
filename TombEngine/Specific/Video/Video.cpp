#include "framework.h"
#include "Specific/Video/Video.h"

#include "Renderer/Renderer.h"
#include "Sound/sound.h"
#include "Specific/EngineMain.h"
#include "Specific/Input/Input.h"
#include "Specific/trutils.h"

using namespace TEN::Input;

// VLC 3.x vs 4.x API compatibility layer.
// On Windows we use vendored VLC 4.x headers+libs. On Linux, system VLC 3.x.
#ifdef SDL_PLATFORM_WIN32
	#define VLC_STATE_ENDING libvlc_Stopping
#else
	#define VLC_STATE_ENDING libvlc_Ended
#endif

namespace
{
	inline void StopVlcPlayer(libvlc_media_player_t* p_mi)
	{
#ifdef SDL_PLATFORM_WIN32
		libvlc_media_player_stop_async(p_mi);
#else
		libvlc_media_player_stop(p_mi);
#endif
	}

	inline void SetVlcPosition(libvlc_media_player_t* p_mi, float pos)
	{
#ifdef SDL_PLATFORM_WIN32
		libvlc_media_player_set_position(p_mi, pos, false);
#else
		libvlc_media_player_set_position(p_mi, pos);
#endif
	}

	inline libvlc_media_t* NewVlcMediaPath(libvlc_instance_t* inst, const char* path)
	{
#ifdef SDL_PLATFORM_WIN32
		(void)inst;
		return libvlc_media_new_path(path);
#else
		return libvlc_media_new_path(inst, path);
#endif
	}
}

namespace TEN::Video
{
	VideoHandler g_VideoPlayer = {};

	static const std::string			  VIDEO_PATH	   = "FMV/";
	static const std::string			  VIDEO_PLUGIN_CACHE_PATH = "plugins/plugins.dat";
	static const std::vector<std::string> VIDEO_EXTENSIONS = { ".mp4", ".avi", ".mkv", ".mov" };

	int VideoHandler::GetPosition() const
	{
		if (_player == nullptr)
			return 0;

		auto* media = libvlc_media_player_get_media(_player);
		long long duration = libvlc_media_get_duration(media);
		float posNormalized = GetNormalizedPosition();

		// Convert and return position in frames.
		long long posMsec = posNormalized * duration;
		return (int)((posMsec / 1000.0f) * FPS);
	}

	float VideoHandler::GetNormalizedPosition() const
	{
		if (_player == nullptr)
			return 0.0f;

		return (float)libvlc_media_player_get_position(_player);
	}

	std::string VideoHandler::GetFileName() const
	{
		if (_player == nullptr)
			return std::string();

		return _fileName;
	}

	Color VideoHandler::GetDominantColor() const
	{
		if (_player == nullptr || _frameBuffer.size() == 0)
			return Color(0.0f, 0.0f, 0.0f);

		unsigned long long accR = 0;
		unsigned long long accG = 0;
		unsigned long long accB = 0;

		int pixelCount = 0;
		int step = 8;

		for (int y = 0; y < _size.y; y += step)
		{
			for (int x = 0; x < _size.x; x += step)
			{
				int index = (y * (x * 4)) + (x * 4);

				unsigned char b = _frameBuffer[index];
				unsigned char g = _frameBuffer[index + 1];
				unsigned char r = _frameBuffer[index + 2];

				accR += r;
				accG += g;
				accB += b;
				pixelCount++;
			}
		}

		if (pixelCount == 0)
			return Color(0.0f, 0.0f, 0.0f);

		unsigned char avgR = (unsigned char)(accR / pixelCount);
		unsigned char avgG = (unsigned char)(accG / pixelCount);
		unsigned char avgB = (unsigned char)(accB / pixelCount);

		auto result = Vector3((float)avgR / 255.0f, (float)avgG / 255.0f, (float)avgB / 255.0f);

		float luma = Luma(result);
		if (luma < 0.3f && luma > 0.0f)
		{
			float boostFactor = 0.3f / luma;
			result *= boostFactor;
			result.Clamp(Vector3::Zero, Vector3::One);
		}
		
		if (luma < 0.5f)
		{
			float desaturationFactor = (0.4f - luma) / 0.4f;
			result.x = (result.x * (1.0f - desaturationFactor)) + (luma * desaturationFactor);
			result.y = (result.y * (1.0f - desaturationFactor)) + (luma * desaturationFactor);
			result.z = (result.z * (1.0f - desaturationFactor)) + (luma * desaturationFactor);
		}

		return Color(result);
	}

	bool VideoHandler::GetSilent() const
	{
		return _silent;
	}

	bool VideoHandler::GetLooped() const
	{
		return _looped;
	}

	void VideoHandler::SetPosition(int frameCount)
	{
		if (_player == nullptr)
			return;

		auto* media = libvlc_media_player_get_media(_player);
		long long duration = libvlc_media_get_duration(media);

		// Convert frames to time in milliseconds (assuming 30 FPS).
		float posMsec = (frameCount / FPS) * 1000.0f;
		float posNormalized = posMsec / (float)(duration);

		if (posNormalized > 1.0f)
		{
			TENLog("Video position is out of bounds.", LogLevel::Warning);
			return;
		}

		// Set normalized position.
		SetNormalizedPosition(posNormalized);
		HandleError();
	}

	void VideoHandler::SetNormalizedPosition(float pos)
	{
		if (_player == nullptr)
			return;

		SetVlcPosition(_player, std::clamp(pos, 0.0f, 1.0f));
		HandleError();
	}

	bool VideoHandler::IsPlaying() const
	{
		if (_player == nullptr)
			return false;

		auto state = libvlc_media_player_get_state(_player);
		return (state == libvlc_Playing);
	}

	bool VideoHandler::IsBackgroundPlaybackQueued() const
	{
		if (_player == nullptr || _playbackMode != VideoPlaybackMode::Exclusive)
			return false;

		// Using VLC player state is unsafe, because it runs in parallel, so we always return true.
		return true;
	}

	void VideoHandler::Initialize(const std::string& gameDir, IGraphicsDevice* device)
	{
		TENLog("Initializing video player...", LogLevel::Info);

		auto pluginCachePath = g_Platform->GetBinaryPath(false) + VIDEO_PLUGIN_CACHE_PATH;

		std::vector<const char*> vlcArgs;
		vlcArgs.push_back("--vout=none");		 // Disable video output and title because rendering is done to a D3D texture.
		vlcArgs.push_back("--aout=adummy");		 // Disable audio output because audio is routed to BASS.
		vlcArgs.push_back("--no-video-title");	 // Disable video title display.
		vlcArgs.push_back("--no-media-library"); // Disable media library to increase loading speed.

#if !_DEBUG
		vlcArgs.push_back("--quiet");			 // Don't generate excessive VLC warnings in the console.
#endif

		if (!std::filesystem::is_regular_file(pluginCachePath))
		{
			TENLog("Rebuilding video plugin cache", LogLevel::Info);
			vlcArgs.push_back("--reset-plugins-cache");
		}

#ifdef SDL_PLATFORM_LINUX
		// Point VLC at bundled plugins directory next to the executable.
		static std::string pluginPathArg;
		auto binaryDir = g_Platform->GetBinaryPath(false);
		pluginPathArg = "--plugin-path=" + binaryDir + "plugins";
		vlcArgs.push_back(pluginPathArg.c_str());
#endif

		_vlcInstance = libvlc_new(static_cast<int>(vlcArgs.size()), vlcArgs.data());

#if _DEBUG
		//libvlc_log_set(_vlcInstance, OnLog, nullptr);
#endif

		HandleError();

		_device = device;

		_videoDirectory = gameDir + VIDEO_PATH;
		_size = Vector2i::Zero;
		_fileName = {};
		_playbackMode = VideoPlaybackMode::Exclusive;
		_looped = false;
		_silent = false;
	}

	void VideoHandler::DeInitialize()
	{
		TENLog("Shutting down VLC...", LogLevel::Info);

		// This flag is needed to avoid race conditions with update callbacks.
		_deInitializing = true;

		if (_player != nullptr)
		{
			if (libvlc_media_player_is_playing(_player))
				StopVlcPlayer(_player);

			while (libvlc_media_player_is_playing(_player))
				std::this_thread::sleep_for(std::chrono::milliseconds(10));

			libvlc_media_player_release(_player);

			_player = nullptr;
		}

		if (_vlcInstance)
			libvlc_release(_vlcInstance);

		_vlcInstance = nullptr;
	}

	bool VideoHandler::Play(const std::string& filename, VideoPlaybackMode mode, bool silent, bool loop)
	{
		auto fullVideoName = filename;

		// At first, attempt to load video file with original filename. Then proceed with asset directory.
		// Then, if not found, try all common video file extensions, and only quit if none are found.
		if (!std::filesystem::is_regular_file(fullVideoName))
		{
			fullVideoName = _videoDirectory + filename;

			if (!std::filesystem::is_regular_file(fullVideoName))
			{
				for (const auto& ext : VIDEO_EXTENSIONS)
				{
					if (std::filesystem::is_regular_file(fullVideoName + ext))
					{
						fullVideoName += ext;
						break;
					}
				}

				if (!std::filesystem::is_regular_file(fullVideoName))
				{
					TENLog("Video file " + fullVideoName + " not found.", LogLevel::Warning);
					return false;
				}
			}
		}

		// Don't start playback if same video is already playing.
		if (_player != nullptr)
		{
			if (libvlc_media_player_get_state(_player) == libvlc_Playing &&
				_playbackMode == mode && _fileName == fullVideoName)
			{
				TENLog("Video file " + fullVideoName + " is already playing.", LogLevel::Warning);
				return false;
			}
		}

		// Stop previous player instance if it exists.
		Stop();

		_looped = loop;
		_silent = silent;
		_playbackMode = mode;
		_fileName = fullVideoName;
		_needRender = _updateInput = false;

		auto* media = NewVlcMediaPath(_vlcInstance, _fileName.c_str());
		if (media == nullptr)
		{
			TENLog("Failed to create media from path: " + _fileName, LogLevel::Error);
			return false;
		}

		// VLC requires to initialize media. Load into player and release right away.
#ifdef SDL_PLATFORM_WIN32
		_player = libvlc_media_player_new_from_media(_vlcInstance, media);
#else
		_player = libvlc_media_player_new_from_media(media);
#endif
		libvlc_media_release(media);

		if (_player == nullptr)
		{
			TENLog("Failed to create media player.", LogLevel::Error);
			return false;
		}

		// Route sound data to BASS, if video is not played in silent mode.
		if (!_silent)
		{
			libvlc_audio_set_format_callbacks(_player, OnAudioSetup, nullptr);
			libvlc_audio_set_callbacks(_player, Sound_VideoPlayCallback, nullptr, nullptr, Sound_VideoFlushCallback, nullptr, this);
		}

		// Route video data to D3D texture and play video.
		libvlc_video_set_callbacks(_player, OnLockFrame, OnUnlockFrame, nullptr, this);
		libvlc_video_set_format_callbacks(_player, OnVideoSetup, nullptr);
		libvlc_media_player_play(_player);

		if (!HandleError())
			return false;

		auto filePath = std::filesystem::path(_fileName);
		TENLog("Playing video file: " + filePath.filename().string() + " (" + (mode == VideoPlaybackMode::Exclusive ? "Exclusive" : "Background") + " mode)", LogLevel::Info);

		if (_playbackMode == VideoPlaybackMode::Exclusive)
			PauseAllSounds(SoundPauseMode::Pause);

		// Starting flag is needed to avoid race conditions with asynchronous playback.
		_starting = true;

		return true;
	}

	bool VideoHandler::Pause()
	{
		if (_player == nullptr)
			return false;

		if (libvlc_media_player_get_state(_player) != libvlc_Paused)
		{
			libvlc_media_player_pause(_player);
			HandleError();

			return (_playbackMode == VideoPlaybackMode::Exclusive);
		}

		HandleError();
		return false;
	}

	bool VideoHandler::Resume()
	{
		if (_player == nullptr)
			return false;

		if (libvlc_media_player_get_state(_player) == libvlc_Paused)
		{
			libvlc_media_player_play(_player);
			HandleError();

			return (_playbackMode == VideoPlaybackMode::Exclusive);
		}

		HandleError();
		return false;
	}

	void VideoHandler::Stop()
	{
		DeinitializePlayer();
		DeinitializeVideoTexture();

		// Don't unset this flag until the native texture is released, otherwise it may crash when trying to render the texture.
		_needRender = false;

		HandleError();
	}

	bool VideoHandler::Update()
	{
		if (_deInitializing || _player == nullptr)
			return false;

		// Attempt to map and render texture only if callback has set frame to be rendered.
		if (_needRender)
		{
			_device->UpdateTexture2D(_videoTexture.get(), _frameBuffer);

			if (_playbackMode == VideoPlaybackMode::Exclusive)
			{
				RenderExclusive();
			}
			else if (_playbackMode == VideoPlaybackMode::Background)
			{
				RenderBackground();
			}

			/*if (_videoTexture && SUCCEEDED(_d3dContext->Map(_videoTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
			{
				// Copy framebuffer row by row, otherwise skewing may occur.
				unsigned char* pData = reinterpret_cast<unsigned char*>(mappedResource.pData);
				for (int row = 0; row < _size.y; row++)
					memcpy(pData + row * mappedResource.RowPitch, _frameBuffer.data() + row * _size.x * 4, _size.x * 4);
				_d3dContext->Unmap(_videoTexture, 0);

				if (_playbackMode == VideoPlaybackMode::Exclusive)
				{
					RenderExclusive();
				}
				else if (_playbackMode == VideoPlaybackMode::Background)
				{
					RenderBackground();
				}
			}
			else
			{
				TENLog("Failed to render video texture", LogLevel::Error);
			}*/

			_needRender = false;
		}

		if (_playbackMode == VideoPlaybackMode::Exclusive)
		{
			UpdateExclusive();
		}
		else if (_playbackMode == VideoPlaybackMode::Background)
		{
			UpdateBackground();
		}

		return (_playbackMode == VideoPlaybackMode::Exclusive);
	}

	void VideoHandler::UpdateExclusive()
	{
		if (_deInitializing || _player == nullptr)
			return;

		bool interruptPlayback = false;

		if (_updateInput)
		{
			ResetClock = true;
			g_Input.Update(*g_Platform->GetSDL3Window(), Vector2::Zero, true); // @inputme
			interruptPlayback = IsHeld(In::Deselect) || IsHeld(In::Look);
			_updateInput = _starting = false;
		}

		auto state = libvlc_media_player_get_state(_player);

		// If player is just opening, buffering, or stopping, always return early and wait for process to end.
		if (state == libvlc_Opening || state == libvlc_Buffering)
			return;

		// Reset playback to start if video is looped.
		if (_looped && !interruptPlayback && (state == VLC_STATE_ENDING || state == libvlc_Stopped))
			libvlc_media_player_play(_player);

		// If user pressed a key to break out from video or video has finished playback or in an error, stop and delete it.
		if (interruptPlayback || state == libvlc_Error || (!_starting && state == libvlc_Stopped))
		{
			Stop();
			ClearAction(In::Pause); // HACK: Otherwise pause key won't work after video ends.
			ResumeAllSounds(SoundPauseMode::Pause);
		}

		HandleError();
	}

	void VideoHandler::RenderExclusive()
	{
		g_Renderer.RenderFullScreenTexture(_videoTexture.get(), (float)_size.x / (float)_size.y);
	}

	void VideoHandler::UpdateBackground()
	{
		if (_deInitializing || _player == nullptr)
			return;

		auto state = libvlc_media_player_get_state(_player);

		// If video has finished playback, stop and delete it.
		if (!_looped && (state == libvlc_Error || state == libvlc_Stopped))
		{
			Stop();
			return;
		}

		// Reset playback to start if video is looped.
		if (_looped && (state == VLC_STATE_ENDING || state == libvlc_Stopped))
			libvlc_media_player_play(_player);

		HandleError();
	}

	void VideoHandler::RenderBackground()
	{
		g_Renderer.UpdateVideoTexture(_videoTexture.get());
	}

	bool VideoHandler::HandleError()
	{
		if (_vlcInstance == nullptr)
			return false;

		const char* vlcMsg = libvlc_errmsg();
		if (vlcMsg && strlen(vlcMsg) > 0)
		{
			TENLog("Video player error: " + std::string(vlcMsg), LogLevel::Error);
			return false;
		}

		return true;
	}

	bool VideoHandler::InitializeVideoTexture()
	{
		if (_videoTexture != nullptr)
		{
			TENLog("Video texture already exists", LogLevel::Error);
			return false;
		}

		_frameBuffer.resize(_size.x * _size.y * 4);

		_videoTexture = _device->CreateTexture2D(_size.x, _size.y, SurfaceFormat::SF_BGRA8_Unorm, nullptr);

		return true;
	}

	void VideoHandler::DeinitializeVideoTexture()
	{
		if (_videoTexture != nullptr)
		{
			_videoTexture.release();
			_videoTexture = nullptr;
		}

		_frameBuffer.clear();

		_size = Vector2i::Zero;
	}

	void VideoHandler::DeinitializePlayer()
	{
		if (_deInitializing || _player == nullptr)
			return;

		DeinitializeVideoTexture();

		StopVlcPlayer(_player);
		libvlc_media_player_release(_player);

		_player = nullptr;
		_fileName = {};

		HandleError();
	}

	void* VideoHandler::OnLockFrame(void* data, void** pixels)
	{
		auto* player = static_cast<VideoHandler*>(data);
		*pixels = player->_frameBuffer.data();
		return nullptr;
	}

	void VideoHandler::OnUnlockFrame(void* data, void* picture, void* const* pixels)
	{
		auto* player = static_cast<VideoHandler*>(data);
		player->_needRender = true;

		if (player->_playbackMode == VideoPlaybackMode::Exclusive)
			player->_updateInput = true;
	}

	void VideoHandler::OnLog(void* data, int level, const libvlc_log_t* ctx, const char* fmt, va_list args)
	{
		LogLevel logLevel = LogLevel::Info;

		switch (level)
		{
			case LIBVLC_ERROR:
				logLevel = LogLevel::Error;
				break;

			case LIBVLC_WARNING:
				logLevel = LogLevel::Warning;
				break;

			case LIBVLC_NOTICE:
				logLevel = LogLevel::Info;
				break;

			case LIBVLC_DEBUG:
				logLevel = LogLevel::Info;
				break;

			default:
				logLevel = LogLevel::Info;
				break;
		}

		char logMgs[1024];
		vsnprintf(logMgs, sizeof(logMgs), fmt, args);

		TENLog("VLC: " + std::string(logMgs), logLevel);
	}

	int VideoHandler::OnAudioSetup(void** data, char* format, unsigned* rate, unsigned* channels)
	{
		strncpy(format, "FL32", 4); // 32-bit float, little-endian (default format)
		*rate = SOUND_SAMPLE_RATE;
		*channels = SOUND_CHANNEL_COUNT;

		return 0;
	}

	unsigned int VideoHandler::OnVideoSetup(void** data, char* chroma, unsigned* width, unsigned* height, unsigned* pitches, unsigned* lines)
	{
		strncpy(chroma, "BGRA", 4);

		*pitches = *width * 4;
		*lines = *height;

		auto* player = static_cast<VideoHandler*>(*data);

		// Fetch video size only once when playback is just started.
		if (player->_size == Vector2i::Zero)
		{
			player->_size = Vector2i(*width, *height);
			player->InitializeVideoTexture();
		}

		return 1;
	}
}