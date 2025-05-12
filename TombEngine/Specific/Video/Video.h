#pragma once

#include "Renderer/Graphics/Texture2D.h"

using namespace TEN::Math;
using namespace TEN::Renderer::Graphics;

namespace TEN::Video
{
	enum class VideoPlaybackMode
	{
		Exclusive,
		Background
	};

	class VideoHandler
	{
	private:
		// VLC core components

		libvlc_instance_t*	   _vlcInstance = nullptr;
		libvlc_media_player_t* _player		= nullptr;

		// Video properties

		int				  _volume		  = 100;
		bool			  _silent		  = false;
		bool			  _looped		  = false;
		VideoPlaybackMode _playbackMode	  = VideoPlaybackMode::Exclusive;
		Vector2i		  _size			  = Vector2i::Zero;
		std::string		  _fileName		  = {};
		std::string		  _videoDirectory = {};

		// Render synchronization

		bool _needRender	 = false;
		bool _updateInput	 = false;
		bool _deInitializing = false;

		// Renderer Resources

		std::vector<char>		  _frameBuffer	= {};
		Texture2D				  _texture		= {};
		ID3D11Texture2D*		  _videoTexture = nullptr;
		ID3D11Device*			  _d3dDevice	= nullptr;
		ID3D11DeviceContext*	  _d3dContext	= nullptr;
		ID3D11ShaderResourceView* _textureView	= nullptr;

	public:
		// Constructors

		VideoHandler() = default;

		// Getters

		int			GetPosition() const;
		float		GetNormalizedPosition() const;
		std::string GetFileName() const;
		Color		GetDominantColor() const;
		bool		GetSilent() const;
		bool		GetLooped() const;

		// Setters

		void SetPosition(int frameCount);
		void SetNormalizedPosition(float pos);
		void SetVolume(int volume);

		// Inquirers

		bool IsPlaying() const;

		// Utilties

		void Initialize(const std::string& gameDir, ID3D11Device* device, ID3D11DeviceContext* context);
		void DeInitialize();
		bool Play(const std::string& filename, VideoPlaybackMode mode = VideoPlaybackMode::Exclusive, bool silent = false, bool looped = false);
		bool Pause();
		bool Resume();
		void Stop();
		bool Update();

	private:
		// Update

		void UpdateExclusive();
		void RenderExclusive();
		void UpdateBackground();
		void RenderBackground();

		// Helpers

		bool HandleError();
		bool InitializeD3DTexture();
		void DeinitializeD3DTexture();
		void DeinitializePlayer();

		// VLC callbacks

		static void*		OnLockFrame(void* data, void** pixels);
		static void			OnUnlockFrame(void* data, void* picture, void* const* pixels);
		static void			OnLog(void* data, int level, const libvlc_log_t* ctx, const char* fmt, va_list args);
		static unsigned int OnSetup(void** data, char* chroma, unsigned* width, unsigned* height, unsigned* pitches, unsigned* lines);
	};

	extern VideoHandler g_VideoPlayer;
}
