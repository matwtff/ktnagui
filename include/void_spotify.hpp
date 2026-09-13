#pragma once

#include "void_types.hpp"
#include "void_draw.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace VoidGUI {

    class Context;

    struct LyricLine {
        uint64_t time_ms = 0;
        std::string text;
    };

    struct MediaTrackInfo {
        std::string title = "No Media Playing";
        std::string artist = "—";
        std::string album = "";
        bool is_playing = false;
        float progress = 0.0f;
        float current_seconds = 0.0f;
        float total_seconds = 0.0f;
        float volume = 1.0f;
        bool shuffle = false;
        bool repeat = false;
        Color dominant_color = Color(30, 215, 96, 255);
        bool lyrics_available = false;
        bool lyrics_synced = false;
    };

    enum class PlayerViewMode {
        Compact,
        Lyrics,
        Artwork
    };

    class SpotifyPlayer {
    public:
        static void Initialize();
        static void Update(float delta_time);
        static void Render(Context* ctx, Vec2* pos, bool* open = nullptr);

        static void TogglePlayPause();
        static void SkipNext();
        static void SkipPrevious();
        static void Seek(float fraction);
        static void SetVolume(float vol);
        static void ToggleShuffle();
        static void ToggleRepeat();

        static PlayerViewMode GetViewMode();
        static void SetViewMode(PlayerViewMode mode);
        static void ToggleLyrics();
        static void ToggleArtwork();

        static const MediaTrackInfo& GetTrackInfo();
        static bool HasLyrics();
        static bool IsLyricsExpanded();
        static void SetLyricsExpanded(bool expanded);
        static bool IsShuffle();
        static bool IsRepeat();

    private:
        static MediaTrackInfo s_track;
        static float s_poll_timer;
        static float s_motion;
        static bool s_is_seeking;

        static PlayerViewMode s_view_mode;
        static PlayerViewMode s_prev_view_mode;
        static Vec2 s_current_size;
        static Vec2 s_target_size;
        static bool s_mode_transition;

        static float s_lyrics_scroll_y;
        static float s_lyrics_scroll_target;
        static int s_current_lyric_line;
        static std::string s_last_lyrics_track;
        static bool s_manual_scroll;
        static uint64_t s_manual_scroll_until_ms;
        static bool s_position_sync_requested;

        static void PollWinRTMedia();
        static void FetchLyricsAsync(const std::string& artist, const std::string& title, int duration_secs);
    };

}
