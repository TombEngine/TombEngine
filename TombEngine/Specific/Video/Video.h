#pragma once
#include <framework.h>

#include <vlc/vlc.h>
#include <d3d11.h>
#include <string>
#include "Renderer/Graphics/Texture2D.h"

using namespace TEN::Math;
using namespace TEN::Renderer::Graphics;

namespace TEN::Video
{
	constexpr auto MIN_VIDEO_WIDTH  = 800;
	constexpr auto MIN_VIDEO_HEIGHT = 600;
	constexpr auto MAX_VIDEO_WIDTH  = 2048;
	constexpr auto MAX_VIDEO_HEIGHT = 2048;

	enum class VideoPlaybackMode
	{
		Exclusive,
		Background
	};

	class VideoHandler
	{
	public:
		VideoHandler() = default;
		~VideoHandler();

		void Initialize(const std::string& gameDir, ID3D11Device* device, ID3D11DeviceContext* context);
		bool Play(const std::string& filename, VideoPlaybackMode mode = VideoPlaybackMode::Exclusive, bool silent = false, bool looped = false);
		bool Pause();
		bool Resume();
		void Stop();
		bool Update();
		void SetVolume(int volume);

		float GetPosition() const;
		void  SetPosition(float position);
		std::string GetFileName() const;
		bool GetSilent() const;
		bool GetLooped() const;
		bool IsPlaying() const;

	private:
		// VLC core components
		libvlc_instance_t* _vlcInstance = nullptr;
		libvlc_media_player_t* _player = nullptr;

		// Video properties
		int  _volume = 100;
		bool _silent = false;
		bool _looped = false;
		VideoPlaybackMode _playbackMode = VideoPlaybackMode::Exclusive;
		Vector2i _videoSize = Vector2i::Zero;
		Vector2i _textureSize = Vector2i::Zero;
		std::string _fileName = {};
		std::string _videoDirectory = {};

		// Render synchronization
		bool _needRender = false;

		// Renderer Resources
		std::vector<char> _frameBuffer = {};
		Texture2D _texture = {};
		ID3D11Texture2D* _videoTexture = nullptr;
		ID3D11Device* _d3dDevice = nullptr;
		ID3D11DeviceContext* _d3dContext = nullptr;
		ID3D11ShaderResourceView* _textureView = nullptr;

		// VLC callbacks
		static void* OnLockFrame(void* data, void** pixels);
		static void  OnUnlockFrame(void* data, void* picture, void* const* pixels);
		static void  OnLog(void* data, int level, const libvlc_log_t* ctx, const char* fmt, va_list args);

		// Update
		void UpdateExclusive();
		void RenderExclusive();
		void UpdateBackground();
		void RenderBackground();

		// Helpers
		bool HandleError();
		bool InitD3DTexture();
		void DeInitD3DTexture();
		void DeInitPlayer();
	};

	extern VideoHandler g_VideoPlayer;
}