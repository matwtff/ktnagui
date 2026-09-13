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
        bool is_playing = false;
        float progress = 0.0f;       // 0.0f .. 1.0f
        float current_seconds = 0.0f;
        float total_seconds = 0.0f;
        float volume = 1.0f;
        bool shuffle = false;
        Color dominant_color = Color(30, 215, 96, 255);
        bool lyrics_available = false;
        bool lyrics_synced = false;
    };

    class SpotifyPlayer {
    public:
        static void Initialize();
        static void Update(float delta_time);
        static void Render(Context* ctx, Vec2* pos, bool* open = nullptr);

        // Hardware / OS Media Transport Dispatches
        static void TogglePlayPause();
        static void SkipNext();
        static void SkipPrevious();
        static void Seek(float fraction);
        static void SetVolume(float vol);
        static void ToggleShuffle();

        static const MediaTrackInfo& GetTrackInfo();
        static bool HasLyrics();
        static bool IsLyricsExpanded();
        static void SetLyricsExpanded(bool expanded);

    private:
        static MediaTrackInfo s_track;
        static float s_poll_timer;
        static float s_vinyl_angle;
        static bool s_is_seeking;

        // Lyrics & Expanded UI State
        static bool s_lyrics_expanded;
        static float s_card_height;
        static float s_lyrics_scroll_y;
        static float s_lyrics_scroll_target;
        static int s_current_lyric_line;
        static std::string s_last_lyrics_track;

        static void PollWinRTMedia();
        static void FetchLyricsAsync(const std::string& artist, const std::string& title, int duration_secs);
    };

} // namespace VoidGUI
