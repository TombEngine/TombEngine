#pragma once

constexpr auto SOUND_TRACK_CHANNEL_BGM     = "BGM";
constexpr auto SOUND_TRACK_CHANNEL_ONESHOT = "OneShot";
constexpr auto SOUND_TRACK_CHANNEL_VOICE   = "Voice";

constexpr auto SOUND_TRACK_CHANNEL_LIMIT   = 32;

enum class TrackFlags : int
{
    None         = 0,
    Loop         = 1 << 0,
    Crossfade    = 1 << 1,
    AutoFree     = 1 << 2,
    ShuffleStart = 1 << 3,
    DampBGM      = 1 << 4,
    RestoreBGM   = 1 << 5
};

inline TrackFlags  operator| (TrackFlags a, TrackFlags b) { return (TrackFlags)((int)a | (int)b); }
inline TrackFlags& operator|=(TrackFlags& a, TrackFlags b) { return a = a | b; }
inline TrackFlags  operator& (TrackFlags a, TrackFlags b) { return (TrackFlags)((int)a & (int)b); }
inline TrackFlags  operator~ (TrackFlags a)               { return (TrackFlags)~(int)a; }

inline bool HasTrackFlag(TrackFlags flags, TrackFlags flag)
{
    return ((int)(flags & flag)) != 0;
}

enum class TrackPreset
{
    OneShot,
    Voice,
    BGM
};

struct TrackChannel
{
    HSTREAM     Stream         = 0;
    QWORD       SavedPosition  = 0;
    std::string Name           = {};
    std::string Track          = {};
    TrackPreset Preset         = TrackPreset::OneShot;
    TrackFlags  Flags          = TrackFlags::None;
    int         FadeInTime     = 0;
    int         FadeOutTime    = 200;
    int         CrossfadeTime  = 0;
    float       Volume         = 1.0f;
    bool        Active         = false;
};

class SoundTrackManager
{
private:
    std::unordered_map<unsigned int, TrackChannel> _channels  = {};
    std::unordered_map<std::string, unsigned int>  _nameIndex = {};
    int         _globalVolume   = 100;
    std::string _audioDirectory = {};

    static bool         IsBuiltinName(const std::string& name);
    unsigned int        HashName(const std::string& name) const;
    TrackChannel*       FindChannel(const std::string& name);
    const TrackChannel* FindChannel(const std::string& name) const;
    std::string         ResolveTrackPath(const std::string& track) const;
    void                ConfigureFromPreset(TrackChannel& channel, TrackPreset preset);
    void                RestoreBGMVolume();

    static void CALLBACK OnTrackFinished(HSYNC handle, DWORD channel, DWORD data, void* userData);

public:
    SoundTrackManager(const std::string& audioDirectory);

    bool EnsureChannelExists(const std::string& name);

    bool Play(
        const std::string& channelName,
        std::optional<std::string> track    = std::nullopt,
        std::optional<TrackPreset> preset   = std::nullopt,
        std::optional<QWORD> startPos       = std::nullopt,
        int forceFadeIn                     = 0);

    void SetTrack(const std::string& channelName, const std::string& track, int crossfadeTimeMs);

    void Stop(const std::string& channelName, std::optional<int> fadeOutTime = std::nullopt);
    void StopAll(std::optional<int> fadeOutTime = std::nullopt);
    void Pause(const std::string& channelName);
    void PauseAll(bool excludeVoice = false);
    void Resume(const std::string& channelName);
    void ResumeAll();

    void  SetGlobalVolume(int volume);
    int   GetGlobalVolume() const;
    void  SetChannelVolume(const std::string& channelName, float volume);
    float GetChannelVolume(const std::string& channelName) const;
    void  SetShuffleStart(const std::string& channelName, bool enable);
    void  SetDampBGM(const std::string& channelName, bool enable);
    bool  GetDampBGM(const std::string& channelName) const;
    void       SetChannelFlags(const std::string& channelName, TrackFlags flags);
    TrackFlags  GetChannelFlags(const std::string& channelName) const;
    void        SetChannelPreset(const std::string& channelName, TrackPreset preset);
    TrackPreset GetChannelPreset(const std::string& channelName) const;
    void        SetCrossfadeTime(const std::string& channelName, int ms);

    bool   SetPositionSeconds(const std::string& channelName, double seconds);
    double GetPositionSeconds(const std::string& channelName) const;
    QWORD  GetPositionBytes(const std::string& channelName) const;
    float  GetNormalizedPosition(const std::string& channelName) const;

    bool        IsPlaying(const std::string& channelName) const;
    bool        IsPlayingTrack(const std::string& trackName) const;
    bool        IsPlayingTrack(const std::string& trackName, const std::string& channelName) const;
    std::string GetTrackName(const std::string& channelName) const;
    float       GetLoudness(const std::string& channelName) const;

    void Clear(const std::string& channelName);
    void ClearAll();
    void Update();

    std::vector<HSTREAM>      GetActiveStreams() const;
    std::vector<TrackChannel> GetAllChannelStates() const;
    void                      RestoreFromSave(const std::vector<TrackChannel>& states);
};

extern SoundTrackManager* g_SoundTrackManager;
