#include "framework.h"
#include "Scripting/Internal/TEN/Sound/AudioChannel.h"

#include "Specific/clock.h"

#include "Scripting/Internal/ReservedScriptNames.h"
#include "Scripting/Internal/ScriptAssert.h"
#include "Scripting/Internal/TEN/Types/Time/Time.h"
#include "Sound/sound.h"

using namespace TEN::Scripting;

static TrackPreset ToTrackPreset(SoundTrackType type)
{
    switch (type)
    {
    case SoundTrackType::BGM:   return TrackPreset::BGM;
    case SoundTrackType::Voice: return TrackPreset::Voice;
    default:                    return TrackPreset::OneShot;
    }
}

static SoundTrackType ToSoundTrackType(TrackPreset preset)
{
    switch (preset)
    {
    case TrackPreset::BGM:   return SoundTrackType::BGM;
    case TrackPreset::Voice: return SoundTrackType::Voice;
    default:                 return SoundTrackType::OneShot;
    }
}

namespace TEN::Scripting::Sound
{
    AudioChannel::AudioChannel(const std::string& name) : _channelName(name)
    {
    }

    std::unique_ptr<AudioChannel> AudioChannel::Create(
        const std::string& name,
        sol::optional<std::string> track,
        sol::optional<SoundTrackType> type)
    {
        if (!ScriptAssert(!name.empty(), "AudioChannel: name must not be empty."))
            return nullptr;

        if (!g_SoundTrackManager)
            return nullptr;

        g_SoundTrackManager->EnsureChannelExists(name);

        if (type.has_value())
        {
            auto preset = ToTrackPreset(type.value());
            g_SoundTrackManager->SetChannelPreset(name, preset);

            // User channels only loop — no shuffle start, auto-crossfade, or other built-in behaviors.
            g_SoundTrackManager->SetChannelFlags(name, preset == TrackPreset::BGM ? TrackFlags::Loop : TrackFlags::None);
        }

        if (track.has_value())
            g_SoundTrackManager->SetTrack(name, track.value(), 0);

        return std::make_unique<AudioChannel>(name);
    }

    void AudioChannel::Play(sol::optional<std::string> track, sol::optional<SoundTrackType> type)
    {
        if (!g_SoundTrackManager)
            return;

        auto preset = type.has_value()
            ? std::optional<TrackPreset>(ToTrackPreset(type.value()))
            : std::nullopt;

        g_SoundTrackManager->Play(
            _channelName,
            track ? std::optional<std::string>(track.value()) : std::nullopt,
            preset);
    }

    void AudioChannel::SetTrack(const std::string& track, sol::optional<int> crossfadeTime)
    {
        if (!g_SoundTrackManager)
            return;

        g_SoundTrackManager->SetTrack(_channelName, track, crossfadeTime.value_or(0));
    }

    void AudioChannel::Stop(sol::optional<int> fadeOutTime)
    {
        if (!g_SoundTrackManager)
            return;

        g_SoundTrackManager->Stop(_channelName, fadeOutTime ? std::optional<int>(fadeOutTime.value()) : std::nullopt);
    }

    void AudioChannel::Pause()
    {
        if (!g_SoundTrackManager)
            return;

        g_SoundTrackManager->Pause(_channelName);
    }

    void AudioChannel::Resume()
    {
        if (!g_SoundTrackManager)
            return;

        g_SoundTrackManager->Resume(_channelName);
    }

    void AudioChannel::Clear()
    {
        if (!g_SoundTrackManager)
            return;

        g_SoundTrackManager->Clear(_channelName);
    }

    void AudioChannel::SetVolume(float volume)
    {
        if (!g_SoundTrackManager)
            return;

        g_SoundTrackManager->SetChannelVolume(_channelName, volume);
    }

    float AudioChannel::GetVolume() const
    {
        if (!g_SoundTrackManager)
            return 0.0f;

        return g_SoundTrackManager->GetChannelVolume(_channelName);
    }

    void AudioChannel::SetShuffleStart(bool enable)
    {
        if (!g_SoundTrackManager)
            return;

        g_SoundTrackManager->SetShuffleStart(_channelName, enable);
    }

    void AudioChannel::SetPosition(const Time& time)
    {
        if (!g_SoundTrackManager)
            return;

        double seconds = time.GetFrameCount() / (double)FPS;
        g_SoundTrackManager->SetPositionSeconds(_channelName, seconds);
    }

    Time AudioChannel::GetPosition() const
    {
        if (!g_SoundTrackManager)
            return Time(0.0f);

        double seconds = g_SoundTrackManager->GetPositionSeconds(_channelName);
        return Time((float)(seconds * FPS));
    }

    float AudioChannel::GetNormalizedPosition() const
    {
        if (!g_SoundTrackManager)
            return 0.0f;

        return g_SoundTrackManager->GetNormalizedPosition(_channelName);
    }

    bool AudioChannel::IsPlaying() const
    {
        if (!g_SoundTrackManager)
            return false;

        return g_SoundTrackManager->IsPlaying(_channelName);
    }

    std::string AudioChannel::GetName() const
    {
        return _channelName;
    }

    std::string AudioChannel::GetTrack() const
    {
        if (!g_SoundTrackManager)
            return {};

        return g_SoundTrackManager->GetTrackName(_channelName);
    }

    float AudioChannel::GetLoudness() const
    {
        if (!g_SoundTrackManager)
            return 0.0f;

        return g_SoundTrackManager->GetLoudness(_channelName);
    }

    SoundTrackType AudioChannel::GetType() const
    {
        if (!g_SoundTrackManager)
            return SoundTrackType::OneShot;

        return ToSoundTrackType(g_SoundTrackManager->GetChannelPreset(_channelName));
    }

    void AudioChannel::SetType(SoundTrackType type)
    {
        if (!g_SoundTrackManager)
            return;

        auto preset = ToTrackPreset(type);
        g_SoundTrackManager->SetChannelPreset(_channelName, preset);

        // User channels only loop — no shuffle start, auto-crossfade, or other built-in behaviors.
        g_SoundTrackManager->SetChannelFlags(_channelName, preset == TrackPreset::BGM ? TrackFlags::Loop : TrackFlags::None);
    }

    void AudioChannel::SetCrossFadeLength(int ms)
    {
        if (!g_SoundTrackManager)
            return;

        g_SoundTrackManager->SetCrossfadeTime(_channelName, ms);
    }

    void AudioChannel::Register(sol::state& state, sol::table& parent)
    {
        parent.new_usertype<AudioChannel>(
            ScriptReserved_AudioChannel,
            sol::call_constructor, &AudioChannel::Create,

            /// Play this channel (optionally switching to a new track or type).
            // @function AudioChannel:Play
            // @tparam[opt] string track Filename of the track to play (without extension).
            // @tparam[opt] Sound.SoundTrackType type Playback type to apply.
            ScriptReserved_AudioChannelPlay, &AudioChannel::Play,

            /// Stop this channel.
            // @function AudioChannel:Stop
            // @tparam[opt] int fadeOutTime Fade-out duration in milliseconds.
            ScriptReserved_AudioChannelStop, &AudioChannel::Stop,

            /// Pause this channel.
            // @function AudioChannel:Pause
            ScriptReserved_AudioChannelPause, &AudioChannel::Pause,

            /// Resume this channel.
            // @function AudioChannel:Resume
            ScriptReserved_AudioChannelResume, &AudioChannel::Resume,

            /// Clear this channel (stop and remove track assignment).
            // @function AudioChannel:Clear
            ScriptReserved_AudioChannelClear, &AudioChannel::Clear,

            /// Set the track without playing it.
            // @function AudioChannel:SetTrack
            // @tparam string track Filename (without extension).
            // @tparam[opt] int crossfadeTime Crossfade duration in milliseconds.
            ScriptReserved_AudioChannelSetTrack, &AudioChannel::SetTrack,

            /// Check if the channel is currently playing.
            // @function AudioChannel:IsPlaying
            // @treturn bool True if playing.
            ScriptReserved_AudioChannelIsPlaying, &AudioChannel::IsPlaying,

            /// Get the name of this channel.
            // @function AudioChannel:GetName
            // @treturn string Channel name.
            ScriptReserved_GetName, &AudioChannel::GetName,

            /// Get the current track filename.
            // @function AudioChannel:GetTrack
            // @treturn string Track filename.
            ScriptReserved_AudioChannelGetTrack, &AudioChannel::GetTrack,

            /// Get the current loudness.
            // @function AudioChannel:GetLoudness
            // @treturn float Loudness value.
            ScriptReserved_AudioChannelGetLoudness, &AudioChannel::GetLoudness,

            /// Set the channel volume.
            // @function AudioChannel:SetVolume
            // @tparam float volume Volume (0.0 to 1.0).
            ScriptReserved_AudioChannelSetVolume, &AudioChannel::SetVolume,

            /// Get the channel volume.
            // @function AudioChannel:GetVolume
            // @treturn float Volume.
            ScriptReserved_AudioChannelGetVolume, &AudioChannel::GetVolume,

            /// Get the playback type of this channel.
            // @function AudioChannel:GetType
            // @treturn Sound.SoundTrackType Current channel type.
            ScriptReserved_AudioChannelGetType, &AudioChannel::GetType,

            /// Set the playback type, re-applying its preset defaults (loop, fade, crossfade).
            // @function AudioChannel:SetType
            // @tparam Sound.SoundTrackType type New channel type.
            ScriptReserved_AudioChannelSetType, &AudioChannel::SetType,

            /// Set the crossfade duration for looped (LOOPED) channels.
            // @function AudioChannel:SetCrossFadeLength
            // @tparam int ms Crossfade duration in milliseconds.
            ScriptReserved_AudioChannelSetCrossFadeLength, &AudioChannel::SetCrossFadeLength,

            /// Enable or disable shuffle start.
            // @function AudioChannel:SetShuffleStart
            // @tparam bool enable True to start at a random position.
            ScriptReserved_AudioChannelShuffleStart, &AudioChannel::SetShuffleStart,

            /// Set the playback position.
            // @function AudioChannel:SetPosition
            // @tparam Time time Playback position as a Time value.
            ScriptReserved_AudioChannelSetPosition, &AudioChannel::SetPosition,

            /// Get the current playback position.
            // @function AudioChannel:GetPosition
            // @treturn Time Playback position.
            ScriptReserved_AudioChannelGetPosition, &AudioChannel::GetPosition,

            /// Get the normalized playback position (0.0 to 1.0).
            // @function AudioChannel:GetNormalizedPosition
            // @treturn float Normalized position.
            ScriptReserved_AudioChannelGetNormPos, &AudioChannel::GetNormalizedPosition
        );
    }
}
