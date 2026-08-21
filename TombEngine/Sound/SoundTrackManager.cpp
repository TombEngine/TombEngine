#include "framework.h"
#include "Sound/SoundTrackManager.h"

#include "Specific/configuration.h"
#include "Specific/trutils.h"

#include "Scripting/Include/ScriptInterfaceGame.h"
#include "Sound/sound.h"

using namespace TEN::Utils;

SoundTrackManager* g_SoundTrackManager = nullptr;

static const std::string TRACKS_EXTENSIONS[] = { ".wav", ".ogg", ".mp3" };

// -- Internal helpers -------------------------------------------------------

static TrackChannel MakeBGMChannel()
{
    auto channel = TrackChannel{};
    channel.Flags         = TrackFlags::Loop | TrackFlags::Crossfade | TrackFlags::ShuffleStart;
    channel.FadeOutTime   = SOUND_XFADETIME_BGM;
    channel.CrossfadeTime = SOUND_XFADETIME_BGM;
    channel.Preset        = TrackPreset::BGM;
    return channel;
}

static TrackChannel MakeOneShotChannel()
{
    auto channel = TrackChannel{};
    channel.Flags       = TrackFlags::AutoFree | TrackFlags::DampBGM | TrackFlags::RestoreBGM;
    channel.FadeOutTime = SOUND_XFADETIME_ONESHOT;
    channel.Preset      = TrackPreset::OneShot;
    return channel;
}

static TrackChannel MakeVoiceChannel()
{
    auto channel = TrackChannel{};
    channel.Flags       = TrackFlags::AutoFree;
    channel.FadeOutTime = SOUND_XFADETIME_ONESHOT;
    channel.Preset      = TrackPreset::Voice;
    return channel;
}

// -- SoundTrackManager private methods --------------------------------------

bool SoundTrackManager::IsBuiltinName(const std::string& name)
{
    auto lower = ToLower(name);
    return lower == ToLower(std::string(SOUND_TRACK_CHANNEL_BGM))     ||
           lower == ToLower(std::string(SOUND_TRACK_CHANNEL_ONESHOT)) ||
           lower == ToLower(std::string(SOUND_TRACK_CHANNEL_VOICE));
}

unsigned int SoundTrackManager::HashName(const std::string& name) const
{
    return (unsigned int)GetHash(ToLower(name));
}

TrackChannel* SoundTrackManager::FindChannel(const std::string& name)
{
    auto nameIt = _nameIndex.find(ToLower(name));
    if (nameIt == _nameIndex.end())
        return nullptr;

    auto chanIt = _channels.find(nameIt->second);
    if (chanIt == _channels.end())
        return nullptr;

    return &chanIt->second;
}

const TrackChannel* SoundTrackManager::FindChannel(const std::string& name) const
{
    auto nameIt = _nameIndex.find(ToLower(name));
    if (nameIt == _nameIndex.end())
        return nullptr;

    auto chanIt = _channels.find(nameIt->second);
    if (chanIt == _channels.end())
        return nullptr;

    return &chanIt->second;
}

std::string SoundTrackManager::ResolveTrackPath(const std::string& track) const
{
    auto fullPath = std::filesystem::path(_audioDirectory + track);

    if (!fullPath.has_extension() || !std::filesystem::is_regular_file(fullPath))
    {
        for (const auto& ext : TRACKS_EXTENSIONS)
        {
            fullPath.replace_extension(ext);
            if (std::filesystem::is_regular_file(fullPath))
                return fullPath.string();
        }

        return {};
    }

    return fullPath.string();
}

void SoundTrackManager::ConfigureFromPreset(TrackChannel& channel, TrackPreset preset)
{
    switch (preset)
    {
    case TrackPreset::BGM:
        channel.Flags         = TrackFlags::Loop | TrackFlags::Crossfade | TrackFlags::ShuffleStart;
        channel.FadeOutTime   = SOUND_XFADETIME_BGM;
        channel.CrossfadeTime = SOUND_XFADETIME_BGM;
        break;

    case TrackPreset::OneShot:
        channel.Flags       = TrackFlags::AutoFree | TrackFlags::DampBGM | TrackFlags::RestoreBGM;
        channel.FadeOutTime = SOUND_XFADETIME_ONESHOT;
        break;

    case TrackPreset::Voice:
        channel.Flags       = TrackFlags::AutoFree;
        channel.FadeOutTime = SOUND_XFADETIME_ONESHOT;
        break;
    }

    channel.Preset = preset;
}

void SoundTrackManager::RestoreBGMVolume()
{
    auto* bgm = FindChannel(SOUND_TRACK_CHANNEL_BGM);
    if (!bgm || !BASS_ChannelIsActive(bgm->Stream))
        return;

    float masterVol = ((float)_globalVolume / 100.0f) * bgm->Volume;
    BASS_ChannelSlideAttribute(bgm->Stream, BASS_ATTRIB_VOL, masterVol, SOUND_XFADETIME_BGM_START);
}

// NOTE: DWORD here is BASS's own cross-platform type (uint32_t on Linux/macOS, unsigned long on Windows).
void CALLBACK SoundTrackManager::OnTrackFinished(HSYNC, DWORD, DWORD, void*)
{
    if (g_SoundTrackManager)
        g_SoundTrackManager->RestoreBGMVolume();
}

// -- SoundTrackManager public methods ---------------------------------------

SoundTrackManager::SoundTrackManager(const std::string& audioDirectory)
{
    _audioDirectory = audioDirectory;

    auto addBuiltin = [&](const std::string& name, TrackChannel channel)
    {
        channel.Name                      = name;
        auto hash                         = HashName(name);
        _channels[hash]                   = std::move(channel);
        _nameIndex[ToLower(name)]         = hash;
    };

    addBuiltin(SOUND_TRACK_CHANNEL_BGM,     MakeBGMChannel());
    addBuiltin(SOUND_TRACK_CHANNEL_ONESHOT, MakeOneShotChannel());
    addBuiltin(SOUND_TRACK_CHANNEL_VOICE,   MakeVoiceChannel());
}

bool SoundTrackManager::EnsureChannelExists(const std::string& name)
{
    if (name.empty())
    {
        TENLog("Cannot create AudioChannel with empty name.", LogLevel::Warning);
        return false;
    }

    auto hash = HashName(name);
    if (_channels.count(hash) > 0)
        return true;

    if ((int)_channels.size() >= SOUND_TRACK_CHANNEL_LIMIT)
    {
        TENLog("Sound channel limit of " + std::to_string(SOUND_TRACK_CHANNEL_LIMIT) + " reached. Channel \"" + name + "\" was not created.", LogLevel::Warning);
        return false;
    }

    auto channel              = TrackChannel{};
    channel.Name              = name;
    _channels[hash]           = channel;
    _nameIndex[ToLower(name)] = hash;
    return true;
}

bool SoundTrackManager::Play(const std::string& channelName, std::optional<std::string> track,
                              std::optional<TrackPreset> preset, std::optional<QWORD> startPos,
                              int forceFadeIn)
{
    if (!g_Configuration.EnableSound)
        return false;

    if (!EnsureChannelExists(channelName))
        return false;

    auto* channel = FindChannel(channelName);
    if (!channel)
        return false;

    // Apply preset if provided.
    if (preset.has_value())
        ConfigureFromPreset(*channel, preset.value());

    // Resolve track: argument takes priority over stored default.
    auto trackName = track.has_value() ? track.value() : channel->Track;
    if (trackName.empty())
    {
        TENLog("AudioChannel '" + channelName + "' has no track to play.", LogLevel::Warning);
        return false;
    }

    bool channelActive = (BASS_ChannelIsActive(channel->Stream) != 0);

    // Same-track guard: if same track is already playing, optionally seek.
    if (channelActive && channel->Track.compare(trackName) == 0)
    {
        if (startPos.has_value() && BASS_ChannelGetLength(channel->Stream, BASS_POS_BYTE) > startPos.value())
            BASS_ChannelSetPosition(channel->Stream, startPos.value(), BASS_POS_BYTE);

        return true;
    }

    // Build BASS stream flags.
    unsigned int bassFlags = BASS_UNICODE | BASS_STREAM_AUTOFREE | BASS_SAMPLE_FLOAT | BASS_ASYNCFILE;
    if (HasTrackFlag(channel->Flags, TrackFlags::Loop))
        bassFlags |= BASS_SAMPLE_LOOP;

    // Resolve file path with extension fallback.
    auto fullPath = ResolveTrackPath(trackName);
    if (fullPath.empty())
    {
        TENLog("No soundtrack file found with name '" + trackName + "'.", LogLevel::Warning);
        return false;
    }

    // Fade out the current stream if one is active.
    if (channelActive)
        BASS_ChannelSlideAttribute(channel->Stream, BASS_ATTRIB_VOL, -1.0f, channel->FadeOutTime);

    // Create new BASS stream.
    auto stream = BASS_StreamCreateFile(false, std::filesystem::path(fullPath).c_str(), 0, 0, bassFlags);
    if (Sound_CheckBASSError("Opening soundtrack '%s'", false, trackName.c_str()))
        return false;

    float masterVol = ((float)_globalVolume / 100.0f) * channel->Volume;

    // Determine fade-in duration.
    int fadeInDuration = 0;
    if (forceFadeIn > 0)
    {
        fadeInDuration = forceFadeIn;
    }
    else if (HasTrackFlag(channel->Flags, TrackFlags::Crossfade))
    {
        fadeInDuration = channelActive ? channel->CrossfadeTime : SOUND_XFADETIME_BGM_START;
    }

    // DampBGM: lower BGM volume while this channel is playing.
    if (HasTrackFlag(channel->Flags, TrackFlags::DampBGM))
    {
        auto* bgm = FindChannel(SOUND_TRACK_CHANNEL_BGM);
        if (bgm && BASS_ChannelIsActive(bgm->Stream))
        {
            float dampedVol = ((float)_globalVolume / 100.0f) * bgm->Volume * SOUND_BGM_DAMP_COEFFICIENT;
            BASS_ChannelSlideAttribute(bgm->Stream, BASS_ATTRIB_VOL, dampedVol, SOUND_XFADETIME_BGM_START);
        }
    }

    // RestoreBGM: register a one-shot callback to restore BGM volume when this stream frees.
    if (HasTrackFlag(channel->Flags, TrackFlags::RestoreBGM))
        BASS_ChannelSetSync(stream, BASS_SYNC_FREE | BASS_SYNC_ONETIME | BASS_SYNC_MIXTIME, 0, OnTrackFinished, nullptr);

    // Set initial volume with optional fade-in.
    if (fadeInDuration > 0)
    {
        BASS_ChannelSetAttribute(stream, BASS_ATTRIB_VOL, 0.0f);
        BASS_ChannelSlideAttribute(stream, BASS_ATTRIB_VOL, masterVol, fadeInDuration);
    }
    else
    {
        BASS_ChannelSetAttribute(stream, BASS_ATTRIB_VOL, masterVol);
    }

    // ShuffleStart: randomize playhead for looped tracks when no explicit position is given.
    if (HasTrackFlag(channel->Flags, TrackFlags::ShuffleStart) && !startPos.has_value())
    {
        auto trackLen = BASS_ChannelGetLength(stream, BASS_POS_BYTE);
        auto newPos   = (QWORD)(trackLen * ((float)GetRandomControl() / (float)RAND_MAX));
        BASS_ChannelSetPosition(stream, newPos, BASS_POS_BYTE);
    }

    BASS_ChannelPlay(stream, false);

    // Apply explicit start position, which overrides the shuffle above.
    if (startPos.has_value() && BASS_ChannelGetLength(stream, BASS_POS_BYTE) > startPos.value())
        BASS_ChannelSetPosition(stream, startPos.value(), BASS_POS_BYTE);

    if (Sound_CheckBASSError("Playing soundtrack '%s'", true, trackName.c_str()))
        return false;

    channel->Stream = stream;
    channel->Track  = trackName;
    channel->Active = true;

    // Load subtitles for the voice channel.
    if (ToLower(channel->Name) == ToLower(std::string(SOUND_TRACK_CHANNEL_VOICE)))
        LoadSubtitles(trackName);

    // Fire audio channel callbacks for transient channels (not BGM, which never ends on its own).
    if (channel->Preset != TrackPreset::BGM && g_GameScript)
        g_GameScript->OnAudioChannelPlaying(channel->Name);

    return true;
}

void SoundTrackManager::SetTrack(const std::string& channelName, const std::string& track, int crossfadeTimeMs)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    channel->Track = track;

    // Only overwrite crossfade time when the channel is already playing (real crossfade request)
    // or when an explicit non-zero duration is given. This preserves the preset-configured
    // crossfade time when SetTrack is called before playback begins.
    bool channelActive = BASS_ChannelIsActive(channel->Stream) != 0;
    if (channelActive || crossfadeTimeMs > 0)
        channel->CrossfadeTime = crossfadeTimeMs;

    if (channelActive)
        Play(channelName, track, std::nullopt, std::nullopt, crossfadeTimeMs);
}

void SoundTrackManager::Stop(const std::string& channelName, std::optional<int> fadeOutTime)
{
    auto* channel = FindChannel(channelName);
    if (!channel || channel->Stream == 0)
        return;

    int fadeTime = fadeOutTime.has_value() ? fadeOutTime.value() : channel->FadeOutTime;
    BASS_ChannelSlideAttribute(channel->Stream, BASS_ATTRIB_VOL | BASS_SLIDE_LOG, -1.0f, fadeTime);

    channel->Track  = {};
    channel->Stream = 0;
    channel->Active = false;
}

void SoundTrackManager::StopAll(std::optional<int> fadeOutTime)
{
    for (auto& [hash, channel] : _channels)
        Stop(channel.Name, fadeOutTime);
}

void SoundTrackManager::Pause(const std::string& channelName)
{
    auto* channel = FindChannel(channelName);
    if (!channel || channel->Stream == 0)
        return;

    if (BASS_ChannelIsActive(channel->Stream) == BASS_ACTIVE_PLAYING)
        BASS_ChannelPause(channel->Stream);
}

void SoundTrackManager::PauseAll(bool excludeVoice)
{
    auto voiceLower = ToLower(std::string(SOUND_TRACK_CHANNEL_VOICE));
    for (auto& [hash, channel] : _channels)
    {
        if (excludeVoice && ToLower(channel.Name) == voiceLower)
            continue;

        Pause(channel.Name);
    }
}

void SoundTrackManager::Resume(const std::string& channelName)
{
    auto* channel = FindChannel(channelName);
    if (!channel || channel->Stream == 0)
        return;

    if (BASS_ChannelIsActive(channel->Stream) == BASS_ACTIVE_PAUSED)
        BASS_ChannelStart(channel->Stream);
}

void SoundTrackManager::ResumeAll()
{
    for (auto& [hash, channel] : _channels)
        Resume(channel.Name);
}

void SoundTrackManager::SetGlobalVolume(int volume)
{
    _globalVolume = volume;

    for (auto& [hash, channel] : _channels)
    {
        if (!BASS_ChannelIsActive(channel.Stream))
            continue;

        float vol = ((float)volume / 100.0f) * channel.Volume;
        BASS_ChannelSetAttribute(channel.Stream, BASS_ATTRIB_VOL, vol);
    }
}

int SoundTrackManager::GetGlobalVolume() const
{
    return _globalVolume;
}

void SoundTrackManager::SetChannelVolume(const std::string& channelName, float volume)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    channel->Volume = std::clamp(volume, 0.0f, 1.0f);

    if (BASS_ChannelIsActive(channel->Stream))
    {
        float vol = ((float)_globalVolume / 100.0f) * channel->Volume;
        BASS_ChannelSetAttribute(channel->Stream, BASS_ATTRIB_VOL, vol);
    }
}

float SoundTrackManager::GetChannelVolume(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return 0.0f;

    return channel->Volume;
}

void SoundTrackManager::SetShuffleStart(const std::string& channelName, bool enable)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    if (enable)
        channel->Flags |= TrackFlags::ShuffleStart;
    else
        channel->Flags = channel->Flags & ~TrackFlags::ShuffleStart;
}

void SoundTrackManager::SetDampBGM(const std::string& channelName, bool enable)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    if (enable)
        channel->Flags |= TrackFlags::DampBGM;
    else
        channel->Flags = channel->Flags & ~TrackFlags::DampBGM;
}

bool SoundTrackManager::GetDampBGM(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return false;

    return HasTrackFlag(channel->Flags, TrackFlags::DampBGM);
}

void SoundTrackManager::SetChannelFlags(const std::string& channelName, TrackFlags flags)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    channel->Flags = flags;
}

TrackFlags SoundTrackManager::GetChannelFlags(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return TrackFlags::None;

    return channel->Flags;
}

void SoundTrackManager::SetChannelPreset(const std::string& channelName, TrackPreset preset)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    ConfigureFromPreset(*channel, preset);
}

TrackPreset SoundTrackManager::GetChannelPreset(const std::string& channelName) const
{
    const auto* channel = FindChannel(channelName);
    if (!channel)
        return TrackPreset::OneShot;

    return channel->Preset;
}

void SoundTrackManager::SetCrossfadeTime(const std::string& channelName, int ms)
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    channel->CrossfadeTime = ms;
}

bool SoundTrackManager::SetPositionSeconds(const std::string& channelName, double seconds)
{
    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return false;

    auto bytePos = BASS_ChannelSeconds2Bytes(channel->Stream, seconds);
    BASS_ChannelSetPosition(channel->Stream, bytePos, BASS_POS_BYTE);
    return true;
}

double SoundTrackManager::GetPositionSeconds(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return 0.0;

    auto bytePos = BASS_ChannelGetPosition(channel->Stream, BASS_POS_BYTE);
    return BASS_ChannelBytes2Seconds(channel->Stream, bytePos);
}

QWORD SoundTrackManager::GetPositionBytes(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return 0;

    return BASS_ChannelGetPosition(channel->Stream, BASS_POS_BYTE);
}

float SoundTrackManager::GetNormalizedPosition(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return 0.0f;

    auto totalBytes = BASS_ChannelGetLength(channel->Stream, BASS_POS_BYTE);
    if (totalBytes == 0)
        return 0.0f;

    auto curBytes = BASS_ChannelGetPosition(channel->Stream, BASS_POS_BYTE);
    return (float)curBytes / (float)totalBytes;
}

bool SoundTrackManager::IsPlaying(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return false;

    return BASS_ChannelIsActive(channel->Stream) == BASS_ACTIVE_PLAYING;
}

bool SoundTrackManager::IsPlayingTrack(const std::string& trackName) const
{
    auto lowerTrack = ToLower(trackName);
    for (const auto& [hash, channel] : _channels)
    {
        if (!BASS_ChannelIsActive(channel.Stream))
            continue;

        if (ToLower(channel.Track) == lowerTrack)
            return true;
    }

    return false;
}

bool SoundTrackManager::IsPlayingTrack(const std::string& trackName, const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return false;

    return ToLower(channel->Track) == ToLower(trackName);
}

std::string SoundTrackManager::GetTrackName(const std::string& channelName) const
{
    auto* channel = FindChannel(channelName);
    if (!channel)
        return {};

    return channel->Track;
}

float SoundTrackManager::GetLoudness(const std::string& channelName) const
{
    float result = 0.0f;

    if (!g_Configuration.EnableSound)
        return result;

    auto* channel = FindChannel(channelName);
    if (!channel || !BASS_ChannelIsActive(channel->Stream))
        return result;

    BASS_ChannelGetLevelEx(channel->Stream, &result, 0.1f, BASS_LEVEL_MONO | BASS_LEVEL_RMS);
    return std::clamp(result * 2.0f, 0.0f, 1.0f);
}

void SoundTrackManager::Clear(const std::string& channelName)
{
    if (IsBuiltinName(channelName))
    {
        TENLog("Cannot clear built-in audio channel '" + channelName + "'.", LogLevel::Warning);
        return;
    }

    auto* channel = FindChannel(channelName);
    if (!channel)
        return;

    if (channel->Stream != 0)
        BASS_ChannelStop(channel->Stream);

    auto hash = HashName(channelName);
    _nameIndex.erase(ToLower(channelName));
    _channels.erase(hash);
}

void SoundTrackManager::ClearAll()
{
    for (auto& [hash, channel] : _channels)
    {
        if (channel.Stream != 0)
            BASS_ChannelStop(channel.Stream);
    }

    _channels.clear();
    _nameIndex.clear();

    // Re-create built-in channels with default state.
    auto addBuiltin = [&](const std::string& name, TrackChannel channel)
    {
        channel.Name                  = name;
        auto hash                     = HashName(name);
        _channels[hash]               = std::move(channel);
        _nameIndex[ToLower(name)]     = hash;
    };

    addBuiltin(SOUND_TRACK_CHANNEL_BGM,     MakeBGMChannel());
    addBuiltin(SOUND_TRACK_CHANNEL_ONESHOT, MakeOneShotChannel());
    addBuiltin(SOUND_TRACK_CHANNEL_VOICE,   MakeVoiceChannel());
}

void SoundTrackManager::Update()
{
    for (auto& [hash, channel] : _channels)
    {
        if (!channel.Active || channel.Stream == 0)
            continue;

        auto status = BASS_ChannelIsActive(channel.Stream);
        if (status == BASS_ACTIVE_STOPPED || status == BASS_ACTIVE_STALLED)
        {
            channel.Active = false;
            channel.Stream = 0;
        }
    }
}

std::vector<HSTREAM> SoundTrackManager::GetActiveStreams() const
{
    auto streams = std::vector<HSTREAM>{};
    for (const auto& [hash, channel] : _channels)
    {
        if (channel.Stream != 0)
            streams.push_back(channel.Stream);
    }

    return streams;
}

std::vector<TrackChannel> SoundTrackManager::GetAllChannelStates() const
{
    auto states = std::vector<TrackChannel>{};
    for (const auto& [hash, channel] : _channels)
    {
        if (channel.Track.empty())
            continue;

        auto state = channel;
        if (state.Stream != 0 && BASS_ChannelIsActive(state.Stream))
            state.SavedPosition = BASS_ChannelGetPosition(state.Stream, BASS_POS_BYTE);

        state.Stream = 0;
        states.push_back(state);
    }

    return states;
}

void SoundTrackManager::RestoreFromSave(const std::vector<TrackChannel>& states)
{
    ClearAll();

    for (const auto& state : states)
    {
        if (!EnsureChannelExists(state.Name))
            continue;

        auto* channel = FindChannel(state.Name);
        if (!channel)
            continue;

        channel->Preset        = state.Preset;
        channel->Flags         = state.Flags;
        channel->FadeOutTime   = state.FadeOutTime;
        channel->CrossfadeTime = state.CrossfadeTime;
        channel->Volume        = state.Volume;

        if (!state.Track.empty())
        {
            auto startPos = (state.SavedPosition > 0)
                ? std::optional<QWORD>(state.SavedPosition)
                : std::nullopt;
            Play(state.Name, state.Track, std::nullopt, startPos, SOUND_XFADETIME_LEVELJUMP);
        }
    }
}
