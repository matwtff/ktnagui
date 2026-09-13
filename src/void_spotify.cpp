#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winhttp.h>
#include <d3d11.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "../include/stb_image.h"

#include "../include/void_spotify.hpp"
#include "../include/void_gui.hpp"
#include "../include/void_gui_d3d11.hpp"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <cctype>

namespace VoidGUI {

    MediaTrackInfo SpotifyPlayer::s_track;
    float SpotifyPlayer::s_poll_timer = 0.0f;
    float SpotifyPlayer::s_vinyl_angle = 0.0f;
    bool  SpotifyPlayer::s_is_seeking = false;

    bool  SpotifyPlayer::s_lyrics_expanded = true;
    float SpotifyPlayer::s_card_height = 320.0f;
    float SpotifyPlayer::s_lyrics_scroll_y = 0.0f;
    float SpotifyPlayer::s_lyrics_scroll_target = 0.0f;
    int   SpotifyPlayer::s_current_lyric_line = -1;
    std::string SpotifyPlayer::s_last_lyrics_track = "";

    // Monotonic timeline sync state
    static uint64_t s_last_sync_tick = 0;
    static float s_last_known_pos_sec = 0.0f;

    // Internal Lyrics & Threading Data
    static std::mutex s_lyrics_mutex;
    static std::vector<LyricLine> s_synced_lyrics;
    static std::vector<std::string> s_plain_lyrics;
    static bool s_lyrics_available = false;
    static bool s_lyrics_synced = false;
    static bool s_lyrics_fetching = false;

    // Album Artwork D3D11 SRV
    static ID3D11ShaderResourceView* s_album_art_srv = nullptr;
    static uint32_t s_last_thumb_size = 0;

    // WinRT Session Handles
    static bool s_winrt_initialized = false;
    static winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager s_session_manager{ nullptr };
    static winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession s_session{ nullptr };

    // Format seconds as MM:SS
    static void FormatTimeSeconds(float sec, char* buf, size_t buf_sz) {
        if (sec < 0.0f) sec = 0.0f;
        int total_s = static_cast<int>(sec);
        int m = total_s / 60;
        int s = total_s % 60;
        snprintf(buf, buf_sz, "%02d:%02d", m, s);
    }

    // Auto-fit lyric line structure and calculation
    struct FormattedLyricLine {
        std::string text;
        float scale;
        Vec2 size;
    };

    static FormattedLyricLine FitLyricLine(const std::string& raw_text, float base_scale, float max_width, float min_scale = 0.75f) {
        FormattedLyricLine res;
        res.text = raw_text;
        res.scale = base_scale;
        res.size = Font::CalcTextSize(res.text.c_str(), res.scale);

        if (res.size.x <= max_width) {
            return res;
        }

        // Proportional downscale so long lines fit smoothly and remain fully readable
        float target_scale = base_scale * (max_width / res.size.x);
        if (target_scale >= min_scale) {
            res.scale = target_scale;
            res.size = Font::CalcTextSize(res.text.c_str(), res.scale);
            return res;
        }

        // Clamp at min_scale and truncate with ellipsis if line is exceptionally long
        res.scale = min_scale;
        std::string s = raw_text;
        while (!s.empty()) {
            s.pop_back();
            std::string cand = s;
            while (!cand.empty() && (cand.back() == ' ' || cand.back() == ',' || cand.back() == '.')) {
                cand.pop_back();
            }
            std::string candidate = cand + "...";
            Vec2 cand_sz = Font::CalcTextSize(candidate.c_str(), res.scale);
            if (cand_sz.x <= max_width) {
                res.text = candidate;
                res.size = cand_sz;
                return res;
            }
        }

        res.size = Font::CalcTextSize(res.text.c_str(), res.scale);
        return res;
    }

    // Clean track title (strip "feat.", "(Remastered)", etc. for optimal search hit rates)
    static std::string CleanTrackTitle(const std::string& title) {
        std::string clean = title;
        const char* patterns[] = {
            " (feat.", " (ft.", " (with ", " [feat.", " [ft.",
            " (Official", " [Official", " (Remastered", " - Remastered",
            " - Live", " (Live", " - Mono", " (Mono", " (Radio Edit)"
        };
        for (const char* pat : patterns) {
            size_t pos = clean.find(pat);
            if (pos != std::string::npos) {
                clean = clean.substr(0, pos);
            }
        }
        while (!clean.empty() && std::isspace(static_cast<unsigned char>(clean.front()))) clean.erase(clean.begin());
        while (!clean.empty() && std::isspace(static_cast<unsigned char>(clean.back()))) clean.pop_back();
        return clean;
    }

    // Clean artist name
    static std::string CleanArtistName(const std::string& artist) {
        std::string clean = artist;
        size_t pos = clean.find(" feat.");
        if (pos == std::string::npos) pos = clean.find(" ft.");
        if (pos == std::string::npos) pos = clean.find(" & ");
        if (pos == std::string::npos) pos = clean.find(", ");
        if (pos != std::string::npos) clean = clean.substr(0, pos);
        while (!clean.empty() && std::isspace(static_cast<unsigned char>(clean.front()))) clean.erase(clean.begin());
        while (!clean.empty() && std::isspace(static_cast<unsigned char>(clean.back()))) clean.pop_back();
        return clean;
    }

    // URL Encoder
    static std::string UrlEncode(const std::string& str) {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;
        for (unsigned char c : str) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
            } else if (c == ' ') {
                escaped << "%20";
            } else {
                escaped << '%' << std::setw(2) << static_cast<int>(c);
            }
        }
        return escaped.str();
    }

    // Decode JSON escaped string
    static std::string DecodeJsonString(const std::string& input) {
        std::string out;
        out.reserve(input.length());
        for (size_t i = 0; i < input.length(); ++i) {
            if (input[i] == '\\' && i + 1 < input.length()) {
                char next = input[i + 1];
                if (next == 'n') { out += '\n'; i++; }
                else if (next == 'r') { i++; }
                else if (next == 't') { out += '\t'; i++; }
                else if (next == '\"') { out += '\"'; i++; }
                else if (next == '\\') { out += '\\'; i++; }
                else if (next == 'u' && i + 5 < input.length()) {
                    std::string hex_str = input.substr(i + 2, 4);
                    try {
                        uint32_t cp = std::stoul(hex_str, nullptr, 16);
                        if (cp < 0x80) {
                            out += static_cast<char>(cp);
                        } else if (cp < 0x800) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (cp >> 12));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                    } catch (...) {}
                    i += 5;
                } else {
                    out += next;
                    i++;
                }
            } else {
                out += input[i];
            }
        }
        return out;
    }

    // WinHTTP GET Request
    static std::string HttpGet(const std::wstring& host, const std::wstring& path) {
        std::string result;
        HINTERNET hSession = WinHttpOpen(L"VoidGUI/2.4", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return result;

        WinHttpSetTimeouts(hSession, 4000, 4000, 4000, 4000);
        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                                    NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                    WINHTTP_FLAG_SECURE);
            if (hRequest) {
                if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(hRequest, NULL)) {
                    
                    DWORD dwSize = 0;
                    do {
                        dwSize = 0;
                        if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                            std::vector<char> buffer(dwSize + 1);
                            DWORD dwDownloaded = 0;
                            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                                buffer[dwDownloaded] = 0;
                                result.append(buffer.data(), dwDownloaded);
                            }
                        }
                    } while (dwSize > 0);
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Parse LRCLIB response
    static void ParseLyricsJson(const std::string& json, std::vector<LyricLine>& out_synced, std::vector<std::string>& out_plain) {
        out_synced.clear();
        out_plain.clear();

        size_t synced_pos = json.find("\"syncedLyrics\":");
        if (synced_pos != std::string::npos) {
            size_t colon = json.find(':', synced_pos);
            if (colon != std::string::npos) {
                size_t val_start = colon + 1;
                while (val_start < json.length() && std::isspace(static_cast<unsigned char>(json[val_start]))) val_start++;
                if (val_start < json.length() && json[val_start] == '\"') {
                    size_t quote_start = val_start;
                    size_t quote_end = quote_start + 1;
                    while (quote_end < json.length()) {
                        if (json[quote_end] == '\"' && json[quote_end - 1] != '\\') break;
                        quote_end++;
                    }
                    if (quote_end > quote_start + 1) {
                        std::string raw_lyrics = json.substr(quote_start + 1, quote_end - quote_start - 1);
                        std::string decoded = DecodeJsonString(raw_lyrics);

                        std::istringstream ss(decoded);
                        std::string line;
                        while (std::getline(ss, line)) {
                            if (line.empty() || line[0] != '[') continue;
                            size_t close_bracket = line.find(']');
                            if (close_bracket == std::string::npos || close_bracket <= 1) continue;

                            std::string timestamp_str = line.substr(1, close_bracket - 1);
                            int min = 0;
                            float sec = 0.0f;
                            if (sscanf_s(timestamp_str.c_str(), "%d:%f", &min, &sec) >= 2) {
                                uint64_t ms = static_cast<uint64_t>((min * 60.0f + sec) * 1000.0f);
                                std::string text = (close_bracket + 1 < line.length()) ? line.substr(close_bracket + 1) : "";
                                
                                while (text.find('[') != std::string::npos && text.find(']') != std::string::npos) {
                                    size_t o = text.find('[');
                                    size_t c = text.find(']');
                                    if (c > o) text.erase(o, c - o + 1);
                                    else break;
                                }
                                while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.erase(text.begin());
                                while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.pop_back();

                                if (!text.empty()) {
                                    out_synced.push_back({ ms, text });
                                }
                            }
                        }
                        if (!out_synced.empty()) {
                            std::sort(out_synced.begin(), out_synced.end(), [](const LyricLine& a, const LyricLine& b) {
                                return a.time_ms < b.time_ms;
                            });
                            return;
                        }
                    }
                }
            }
        }

        // Fallback to plainLyrics
        size_t plain_pos = json.find("\"plainLyrics\":");
        if (plain_pos != std::string::npos) {
            size_t colon = json.find(':', plain_pos);
            if (colon != std::string::npos) {
                size_t val_start = colon + 1;
                while (val_start < json.length() && std::isspace(static_cast<unsigned char>(json[val_start]))) val_start++;
                if (val_start < json.length() && json[val_start] == '\"') {
                    size_t quote_start = val_start;
                    size_t quote_end = quote_start + 1;
                    while (quote_end < json.length()) {
                        if (json[quote_end] == '\"' && json[quote_end - 1] != '\\') break;
                        quote_end++;
                    }
                    if (quote_end > quote_start + 1) {
                        std::string raw_lyrics = json.substr(quote_start + 1, quote_end - quote_start - 1);
                        std::string decoded = DecodeJsonString(raw_lyrics);
                        std::istringstream ss(decoded);
                        std::string line;
                        while (std::getline(ss, line)) {
                            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) line.erase(line.begin());
                            while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) line.pop_back();
                            if (!line.empty()) out_plain.push_back(line);
                        }
                    }
                }
            }
        }
    }

    void SpotifyPlayer::FetchLyricsAsync(const std::string& artist, const std::string& title, int duration_secs) {
        std::string clean_a = CleanArtistName(artist);
        std::string clean_t = CleanTrackTitle(title);

        std::string track_sig = clean_a + " - " + clean_t;
        if (track_sig == s_last_lyrics_track || s_lyrics_fetching) return;
        if (clean_a.empty() || clean_t.empty() || clean_a == "—" || clean_t == "No Media Playing") return;

        s_last_lyrics_track = track_sig;
        s_lyrics_fetching = true;

        {
            std::lock_guard<std::mutex> lock(s_lyrics_mutex);
            s_synced_lyrics.clear();
            s_plain_lyrics.clear();
            s_lyrics_available = false;
            s_lyrics_synced = false;
        }

        std::thread([clean_a, clean_t, duration_secs]() {
            try {
                std::string enc_a = UrlEncode(clean_a);
                std::string enc_t = UrlEncode(clean_t);

                // 1. Exact track search
                std::wstring get_path = L"/api/get?artist_name=" + std::wstring(enc_a.begin(), enc_a.end()) +
                                        L"&track_name=" + std::wstring(enc_t.begin(), enc_t.end());
                if (duration_secs > 0) {
                    get_path += L"&duration=" + std::to_wstring(duration_secs);
                }

                std::string resp = HttpGet(L"lrclib.net", get_path);
                std::vector<LyricLine> parsed_synced;
                std::vector<std::string> parsed_plain;

                if (!resp.empty() && resp.find("syncedLyrics") != std::string::npos) {
                    ParseLyricsJson(resp, parsed_synced, parsed_plain);
                }

                // 2. Fuzzy search fallback if exact match had no synced lyrics
                if (parsed_synced.empty()) {
                    std::string q = UrlEncode(clean_a + " " + clean_t);
                    std::wstring search_path = L"/api/search?q=" + std::wstring(q.begin(), q.end());
                    std::string search_resp = HttpGet(L"lrclib.net", search_path);
                    if (!search_resp.empty()) {
                        ParseLyricsJson(search_resp, parsed_synced, parsed_plain);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(s_lyrics_mutex);
                    if (!parsed_synced.empty()) {
                        s_synced_lyrics = std::move(parsed_synced);
                        s_lyrics_synced = true;
                        s_lyrics_available = true;
                    } else if (!parsed_plain.empty()) {
                        s_plain_lyrics = std::move(parsed_plain);
                        s_lyrics_synced = false;
                        s_lyrics_available = true;
                    } else {
                        s_lyrics_available = false;
                        s_lyrics_synced = false;
                    }
                    s_lyrics_fetching = false;
                }
            } catch (...) {
                s_lyrics_fetching = false;
            }
        }).detach();
    }

    static void InitWinRT() {
        if (s_winrt_initialized) return;
        try {
            winrt::init_apartment();
            s_session_manager = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
            s_winrt_initialized = true;
        } catch (...) {}
    }

    void SpotifyPlayer::Initialize() {
        InitWinRT();
        s_track.title = "No Media Playing";
        s_track.artist = "—";
        s_track.is_playing = false;
        s_track.total_seconds = 0.0f;
        s_track.current_seconds = 0.0f;
        s_track.progress = 0.0f;
        s_track.dominant_color = Color(30, 215, 96, 255);
        s_last_sync_tick = GetTickCount64();
        s_last_known_pos_sec = 0.0f;
        PollWinRTMedia();
    }

    void SpotifyPlayer::PollWinRTMedia() {
        InitWinRT();
        if (!s_session_manager) return;

        try {
            s_session = s_session_manager.GetCurrentSession();
            if (s_session) {
                auto media_props = s_session.TryGetMediaPropertiesAsync().get();
                std::string raw_title = winrt::to_string(media_props.Title());
                std::string raw_artist = winrt::to_string(media_props.Artist());

                auto timeline = s_session.GetTimelineProperties();
                int64_t end_100ns = timeline.EndTime().count();
                int64_t pos_100ns = timeline.Position().count();

                auto playback = s_session.GetPlaybackInfo();
                int status = static_cast<int>(playback.PlaybackStatus());
                bool is_playing = (status == 4); // 4 = Playing

                // Add elapsed playback time since Windows last refreshed the timeline
                if (is_playing) {
                    try {
                        auto last_updated = timeline.LastUpdatedTime();
                        int64_t last_updated_100ns = last_updated.time_since_epoch().count();
                        if (last_updated_100ns > 0) {
                            auto now_100ns = winrt::clock::now().time_since_epoch().count();
                            int64_t delta_100ns = now_100ns - last_updated_100ns;
                            if (delta_100ns > 0 && delta_100ns < 300000000LL) { // < 30s sanity window
                                pos_100ns += delta_100ns;
                            }
                        }
                    } catch (...) {}
                }

                float total_sec = (end_100ns > 0) ? static_cast<float>(end_100ns / 10000000.0) : 0.0f;
                float pos_sec = (pos_100ns > 0) ? static_cast<float>(pos_100ns / 10000000.0) : 0.0f;

                s_track.is_playing = is_playing;
                s_track.total_seconds = total_sec;

                // Sync reference position
                s_last_known_pos_sec = pos_sec;
                s_last_sync_tick = GetTickCount64();

                if (playback.IsShuffleActive()) {
                    s_track.shuffle = playback.IsShuffleActive().Value();
                }

                // Query and decode actual album cover art
                if (media_props.Thumbnail()) {
                    try {
                        auto thumb_stream = media_props.Thumbnail().OpenReadAsync().get();
                        uint64_t thumb_sz = thumb_stream.Size();
                        if (thumb_sz > 0 && thumb_sz != s_last_thumb_size) {
                            s_last_thumb_size = static_cast<uint32_t>(thumb_sz);
                            winrt::Windows::Storage::Streams::Buffer buffer(static_cast<uint32_t>(thumb_sz));
                            thumb_stream.ReadAsync(buffer, buffer.Capacity(), winrt::Windows::Storage::Streams::InputStreamOptions::ReadAhead).get();

                            int w = 0, h = 0, comp = 0;
                            unsigned char* pixels = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.Length()), &w, &h, &comp, 4);
                            if (pixels) {
                                ID3D11Device* dev = D3D11_GetDevice();
                                if (dev) {
                                    D3D11_TEXTURE2D_DESC desc = {};
                                    desc.Width = w;
                                    desc.Height = h;
                                    desc.MipLevels = 1;
                                    desc.ArraySize = 1;
                                    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                                    desc.SampleDesc.Count = 1;
                                    desc.Usage = D3D11_USAGE_DEFAULT;
                                    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                                    D3D11_SUBRESOURCE_DATA init_data = {};
                                    init_data.pSysMem = pixels;
                                    init_data.SysMemPitch = w * 4;

                                    ID3D11Texture2D* tex = nullptr;
                                    if (SUCCEEDED(dev->CreateTexture2D(&desc, &init_data, &tex)) && tex) {
                                        ID3D11ShaderResourceView* srv = nullptr;
                                        if (SUCCEEDED(dev->CreateShaderResourceView(tex, nullptr, &srv))) {
                                            if (s_album_art_srv) {
                                                s_album_art_srv->Release();
                                            }
                                            s_album_art_srv = srv;
                                        }
                                        tex->Release();
                                    }
                                }
                                stbi_image_free(pixels);
                            }
                        }
                    } catch (...) {}
                }

                if (!raw_title.empty() && (raw_title != s_track.title || raw_artist != s_track.artist)) {
                    s_track.title = raw_title;
                    s_track.artist = raw_artist.empty() ? "—" : raw_artist;
                    s_lyrics_scroll_y = 0.0f;
                    s_lyrics_scroll_target = 0.0f;
                    s_current_lyric_line = -1;

                    FetchLyricsAsync(s_track.artist, s_track.title, static_cast<int>(total_sec));
                }
            }
        } catch (...) {}
    }

    void SpotifyPlayer::Update(float delta_time) {
        s_poll_timer += delta_time;
        if (s_poll_timer >= 0.25f) {
            s_poll_timer = 0.0f;
            PollWinRTMedia();
        }

        // Smooth continuous playback progression
        if (s_track.is_playing && !s_is_seeking) {
            float elapsed_since_sync = static_cast<float>(GetTickCount64() - s_last_sync_tick) / 1000.0f;
            s_track.current_seconds = s_last_known_pos_sec + elapsed_since_sync;
            if (s_track.total_seconds > 0.0f) {
                if (s_track.current_seconds > s_track.total_seconds) {
                    s_track.current_seconds = s_track.total_seconds;
                }
                s_track.progress = s_track.current_seconds / s_track.total_seconds;
            }
            s_vinyl_angle += delta_time * 3.0f;
        }

        {
            std::lock_guard<std::mutex> lock(s_lyrics_mutex);
            s_track.lyrics_available = s_lyrics_available;
            s_track.lyrics_synced = s_lyrics_synced;
        }
    }

    void SpotifyPlayer::TogglePlayPause() {
        InitWinRT();
        if (s_session) {
            try { s_session.TryTogglePlayPauseAsync(); } catch (...) {}
        } else {
            keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
            keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
        }
    }

    void SpotifyPlayer::SkipNext() {
        InitWinRT();
        if (s_session) {
            try { s_session.TrySkipNextAsync(); } catch (...) {}
        } else {
            keybd_event(VK_MEDIA_NEXT_TRACK, 0, 0, 0);
            keybd_event(VK_MEDIA_NEXT_TRACK, 0, KEYEVENTF_KEYUP, 0);
        }
        s_track.current_seconds = 0.0f;
        s_track.progress = 0.0f;
    }

    void SpotifyPlayer::SkipPrevious() {
        InitWinRT();
        if (s_session) {
            try { s_session.TrySkipPreviousAsync(); } catch (...) {}
        } else {
            keybd_event(VK_MEDIA_PREV_TRACK, 0, 0, 0);
            keybd_event(VK_MEDIA_PREV_TRACK, 0, KEYEVENTF_KEYUP, 0);
        }
        s_track.current_seconds = 0.0f;
        s_track.progress = 0.0f;
    }

    void SpotifyPlayer::Seek(float fraction) {
        fraction = std::clamp(fraction, 0.0f, 1.0f);
        InitWinRT();
        if (s_session && s_track.total_seconds > 0.0f) {
            try {
                int64_t pos_100ns = static_cast<int64_t>(fraction * s_track.total_seconds * 10000000.0);
                s_session.TryChangePlaybackPositionAsync(pos_100ns);
            } catch (...) {}
        }
        s_track.progress = fraction;
        s_track.current_seconds = fraction * s_track.total_seconds;
        s_last_known_pos_sec = s_track.current_seconds;
        s_last_sync_tick = GetTickCount64();
    }

    void SpotifyPlayer::SetVolume(float vol) {
        s_track.volume = std::clamp(vol, 0.0f, 1.0f);
    }

    void SpotifyPlayer::ToggleShuffle() {
        InitWinRT();
        if (s_session) {
            try {
                s_session.TryChangeShuffleActiveAsync(!s_track.shuffle);
                s_track.shuffle = !s_track.shuffle;
            } catch (...) {}
        }
    }

    const MediaTrackInfo& SpotifyPlayer::GetTrackInfo() {
        return s_track;
    }

    bool SpotifyPlayer::HasLyrics() {
        return s_lyrics_available;
    }

    bool SpotifyPlayer::IsLyricsExpanded() {
        return s_lyrics_expanded;
    }

    void SpotifyPlayer::SetLyricsExpanded(bool expanded) {
        s_lyrics_expanded = expanded;
    }

    void SpotifyPlayer::Render(Context* ctx, Vec2* pos, bool* open) {
        if (!ctx || !pos || (open && !*open)) return;

        float dt = ctx->GetDeltaTime();
        float target_h = s_lyrics_expanded ? 320.0f : 118.0f;
        s_card_height += (target_h - s_card_height) * (1.0f - std::exp(-14.0f * dt));

        Vec2 size(350.0f, s_card_height);
        uint32_t id = ctx->GetId("##VOID_SPOTIFY_PLAYER");

        // Dragging handling
        static Vec2 s_spot_drag_offset(0.0f, 0.0f);
        static bool s_spot_dragging = false;

        const InputState& input = ctx->GetInput();
        Rect header_drag(*pos, *pos + Vec2(size.x - 85.0f, 30.0f));

        if (ctx->GetActiveId() == 0 && input.mouse_clicked && header_drag.Contains(input.mouse_pos)) {
            ctx->SetActiveId(id);
            s_spot_dragging = true;
            s_spot_drag_offset = input.mouse_pos - *pos;
        }
        if (ctx->GetActiveId() == id && s_spot_dragging) {
            if (input.mouse_down) {
                *pos = input.mouse_pos - s_spot_drag_offset;
                if (pos->x < 0.0f) pos->x = 0.0f;
                if (pos->y < 0.0f) pos->y = 0.0f;
            } else {
                ctx->SetActiveId(0);
                s_spot_dragging = false;
            }
        }

        DrawList* dl = ctx->GetDrawList();
        Theme& theme = ctx->GetTheme();

        Vec2 p_min = *pos;
        Vec2 p_max = p_min + size;

        // Background Frame
        if (theme.liquid_glass) {
            dl->AddLiquidGlassPanel(p_min, p_max, 10.0f, theme.liquid_time, theme.card_bg, theme.border_bright, Color(30, 215, 96, 255));
            dl->AddRectFilled(p_min, Vec2(p_max.x, p_min.y + 26.0f), theme.card_header, 10.0f);
            dl->AddRectFilled(p_min + Vec2(0, 20.0f), Vec2(p_max.x, p_min.y + 26.0f), theme.card_header, 0.0f);
            dl->AddLine(p_min + Vec2(10.0f, 0.5f), Vec2(p_max.x - 10.0f, p_min.y + 0.5f), Color(255, 255, 255, 110), 1.0f);
            dl->AddLine(p_min + Vec2(0, 26.0f), Vec2(p_max.x, p_min.y + 26.0f), theme.border_subtle, 1.0f);
        } else {
            dl->AddShadow(p_min, p_max, 10.0f, 8.0f, Color(0, 0, 0, 160));
            dl->AddShadow(p_min, p_max, 10.0f, 3.0f, Color(30, 215, 96, 25));
            dl->AddRectFilled(p_min, p_max, Color(14, 16, 23, 250), 10.0f);
            dl->AddRect(p_min, p_max, Color(36, 41, 58, 255), 10.0f, 1.0f);
            dl->AddRectFilled(p_min, Vec2(p_max.x, p_min.y + 26.0f), Color(18, 21, 30, 255), 10.0f);
            dl->AddRectFilled(p_min + Vec2(0, 20.0f), Vec2(p_max.x, p_min.y + 26.0f), Color(18, 21, 30, 255), 0.0f);
            dl->AddLine(p_min + Vec2(0, 26.0f), Vec2(p_max.x, p_min.y + 26.0f), Color(28, 32, 46, 255), 1.0f);
        }

        // Spotify Pulse Indicator
        Color dot_col = s_track.is_playing ? Color(30, 215, 96, 255) : Color(110, 118, 140, 255);
        dl->AddCircleFilled(p_min + Vec2(14.0f, 13.0f), 4.0f, dot_col);
        dl->AddText(p_min + Vec2(24.0f, 6.0f), Color(230, 235, 245), "Spotify", 0.95f);

        // Lyrics Expand / Collapse Pill Button on Header Right
        Rect exp_rect(p_min.x + size.x - 78.0f, p_min.y + 4.0f, p_min.x + size.x - 8.0f, p_min.y + 22.0f);
        bool exp_hovered = exp_rect.Contains(input.mouse_pos);
        if (exp_hovered && input.mouse_clicked) {
            s_lyrics_expanded = !s_lyrics_expanded;
        }

        Color exp_bg = s_lyrics_expanded ? Color(124, 58, 237, 75) : (exp_hovered ? Color(36, 42, 60, 255) : Color(22, 26, 36, 255));
        Color exp_border = s_lyrics_expanded ? Color(124, 58, 237, 240) : (exp_hovered ? Color(70, 80, 110, 255) : Color(40, 46, 64, 255));
        dl->AddRectFilled(exp_rect.min, exp_rect.max, exp_bg, 3.0f);
        dl->AddRect(exp_rect.min, exp_rect.max, exp_border, 3.0f, 1.0f);
        const char* exp_label = s_lyrics_expanded ? "[v] LYRICS" : "[>] LYRICS";
        dl->AddText(exp_rect.min + Vec2(6.0f, 3.0f), s_lyrics_expanded ? Color(255, 255, 255) : Color(175, 182, 205), exp_label, 0.85f);

        // Album cover art
        Vec2 art_min(p_min.x + 14.0f, p_min.y + 36.0f);
        Vec2 art_max = art_min + Vec2(60.0f, 60.0f);

        // Multi-layer drop shadow behind album cover
        dl->AddShadow(art_min, art_max, 6.0f, 6.0f, Color(0, 0, 0, 160));

        if (s_album_art_srv != nullptr) {
            // Render actual decoded album cover art with smooth rounded corners!
            dl->AddImageRounded(s_album_art_srv, art_min, art_max, 6.0f);
            dl->AddRect(art_min, art_max, Color(255, 255, 255, 30), 6.0f, 1.0f);
        } else {
            // Fallback stylized album cover frame
            dl->AddRectFilled(art_min, art_max, Color(22, 26, 38, 255), 6.0f);
            dl->AddRect(art_min, art_max, Color(45, 52, 74, 255), 6.0f, 1.0f);

            Vec2 art_center = (art_min + art_max) * 0.5f;
            dl->AddCircleFilled(art_center, 12.0f, Color(30, 215, 96, 220), 16);
            dl->AddCircleFilled(art_center, 4.0f, Color(12, 14, 18, 255), 10);
        }

        // Track info
        float info_x = p_min.x + 86.0f;
        float info_w = size.x - 96.0f;

        std::string display_title = s_track.title;
        if (display_title.length() > 36) {
            display_title = display_title.substr(0, 33) + "...";
        }
        dl->AddText(Vec2(info_x, p_min.y + 34.0f), Color(255, 255, 255), display_title.c_str(), 1.05f);

        std::string display_artist = "by " + s_track.artist;
        if (display_artist.length() > 40) {
            display_artist = display_artist.substr(0, 37) + "...";
        }
        dl->AddText(Vec2(info_x, p_min.y + 49.0f), Color(135, 142, 165), display_artist.c_str(), 0.95f);

        // Timeline progress scrub
        Vec2 scrub_min(info_x, p_min.y + 68.0f);
        Vec2 scrub_max(p_min.x + size.x - 14.0f, p_min.y + 73.0f);
        float scrub_w = scrub_max.x - scrub_min.x;

        Rect scrub_hit(scrub_min.x, scrub_min.y - 4.0f, scrub_max.x, scrub_max.y + 4.0f);
        bool scrub_hovered = scrub_hit.Contains(input.mouse_pos);
        uint32_t scrub_id = id + 0x777;

        if (ctx->GetActiveId() == 0 && input.mouse_clicked && scrub_hovered) {
            ctx->SetActiveId(scrub_id);
            s_is_seeking = true;
        }
        if (ctx->GetActiveId() == scrub_id) {
            if (input.mouse_down) {
                float click_pct = (input.mouse_pos.x - scrub_min.x) / scrub_w;
                s_track.progress = std::clamp(click_pct, 0.0f, 1.0f);
                s_track.current_seconds = s_track.progress * s_track.total_seconds;
            } else {
                Seek(s_track.progress);
                s_is_seeking = false;
                ctx->SetActiveId(0);
            }
        }

        // Scrub Bar Background & Fill
        dl->AddRectFilled(scrub_min, scrub_max, Color(28, 32, 46, 255), 2.0f);
        float fill_w = scrub_w * s_track.progress;
        dl->AddRectFilled(scrub_min, Vec2(scrub_min.x + fill_w, scrub_max.y), Color(30, 215, 96, 255), 2.0f);
        if (scrub_hovered || s_is_seeking) {
            dl->AddCircleFilled(Vec2(scrub_min.x + fill_w, (scrub_min.y + scrub_max.y) * 0.5f), 4.0f, Color(255, 255, 255, 255));
        }

        // Time Labels
        char cur_time_str[16];
        char tot_time_str[16];
        FormatTimeSeconds(s_track.current_seconds, cur_time_str, sizeof(cur_time_str));
        FormatTimeSeconds(s_track.total_seconds, tot_time_str, sizeof(tot_time_str));
        dl->AddText(Vec2(info_x, p_min.y + 76.0f), Color(100, 108, 130), cur_time_str, 0.85f);
        Vec2 tot_sz = Font::CalcTextSize(tot_time_str, 0.85f);
        dl->AddText(Vec2(scrub_max.x - tot_sz.x, p_min.y + 76.0f), Color(100, 108, 130), tot_time_str, 0.85f);

        // Media controls
        float btn_y = p_min.y + 90.0f;
        float btn_h = 20.0f;

        // 1. Shuffle
        Rect shuf_rect(info_x, btn_y, info_x + 36.0f, btn_y + btn_h);
        bool shuf_hov = shuf_rect.Contains(input.mouse_pos);
        if (shuf_hov && input.mouse_clicked) ToggleShuffle();
        Color shuf_col = s_track.shuffle ? Color(30, 215, 96, 255) : (shuf_hov ? Color(220, 225, 240) : Color(110, 118, 140));
        dl->AddRectFilled(shuf_rect.min, shuf_rect.max, Color(22, 25, 36, 200), 3.0f);
        dl->AddText(shuf_rect.min + Vec2(6.0f, 3.0f), shuf_col, "SHUF", 0.85f);

        // 2. Previous Track
        float center_x = info_x + (scrub_w * 0.5f);
        Rect prev_rect(center_x - 48.0f, btn_y, center_x - 22.0f, btn_y + btn_h);
        bool prev_hov = prev_rect.Contains(input.mouse_pos);
        if (prev_hov && input.mouse_clicked) SkipPrevious();
        dl->AddRectFilled(prev_rect.min, prev_rect.max, prev_hov ? Color(36, 42, 60, 255) : Color(22, 25, 36, 200), 3.0f);
        dl->AddText(prev_rect.min + Vec2(5.0f, 3.0f), prev_hov ? Color(255, 255, 255) : Color(160, 168, 190), "|<<", 0.9f);

        // 3. Play / Pause
        Rect play_rect(center_x - 16.0f, btn_y - 2.0f, center_x + 16.0f, btn_y + btn_h + 2.0f);
        bool play_hov = play_rect.Contains(input.mouse_pos);
        if (play_hov && input.mouse_clicked) TogglePlayPause();
        dl->AddCircleFilled(Vec2((play_rect.min.x + play_rect.max.x) * 0.5f, (play_rect.min.y + play_rect.max.y) * 0.5f), 12.0f,
                            play_hov ? Color(34, 235, 106, 255) : Color(30, 215, 96, 255));
        const char* play_icon = s_track.is_playing ? "||" : ">";
        Vec2 ic_sz = Font::CalcTextSize(play_icon, 0.95f);
        Vec2 ic_pos((play_rect.min.x + play_rect.max.x - ic_sz.x) * 0.5f, (play_rect.min.y + play_rect.max.y - ic_sz.y) * 0.5f - 1.0f);
        dl->AddText(ic_pos, Color(10, 12, 16, 255), play_icon, 0.95f);

        // 4. Next Track
        Rect next_rect(center_x + 22.0f, btn_y, center_x + 48.0f, btn_y + btn_h);
        bool next_hov = next_rect.Contains(input.mouse_pos);
        if (next_hov && input.mouse_clicked) SkipNext();
        dl->AddRectFilled(next_rect.min, next_rect.max, next_hov ? Color(36, 42, 60, 255) : Color(22, 25, 36, 200), 3.0f);
        dl->AddText(next_rect.min + Vec2(5.0f, 3.0f), next_hov ? Color(255, 255, 255) : Color(160, 168, 190), ">>|", 0.9f);

        // Synced lyrics viewport
        if (s_card_height > 130.0f) {
            Vec2 lyr_min(p_min.x + 10.0f, p_min.y + 118.0f);
            Vec2 lyr_max(p_min.x + size.x - 10.0f, p_min.y + s_card_height - 10.0f);
            float lyr_view_w = lyr_max.x - lyr_min.x;
            float lyr_view_h = lyr_max.y - lyr_min.y;

            // Inset glassmorphic card container
            dl->AddRectFilled(lyr_min, lyr_max, Color(10, 11, 17, 230), 6.0f);
            dl->AddRect(lyr_min, lyr_max, Color(35, 40, 58, 200), 6.0f, 1.0f);

            // Push scissor clip so lyrics never bleed outside the card
            dl->PushClipRect(Rect(lyr_min.x + 2.0f, lyr_min.y + 2.0f, lyr_max.x - 2.0f, lyr_max.y - 2.0f));

            bool local_avail = false;
            bool local_synced = false;
            bool local_fetching = false;
            std::vector<LyricLine> local_synced_lines;
            std::vector<std::string> local_plain_lines;

            {
                std::lock_guard<std::mutex> lock(s_lyrics_mutex);
                local_avail = s_lyrics_available;
                local_synced = s_lyrics_synced;
                local_fetching = s_lyrics_fetching;
                local_synced_lines = s_synced_lyrics;
                local_plain_lines = s_plain_lyrics;
            }

            if (local_fetching) {
                // Animated Loading State
                float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(GetTickCount64()) * 0.006f);
                Color load_col = Color::Lerp(Color(124, 58, 237, 180), Color(30, 215, 96, 255), pulse);
                const char* load_msg = "Fetching synced lyrics from LRCLIB...";
                Vec2 msg_sz = Font::CalcTextSize(load_msg, 0.95f);
                Vec2 msg_pos(lyr_min.x + (lyr_max.x - lyr_min.x - msg_sz.x) * 0.5f,
                             lyr_min.y + (lyr_view_h - msg_sz.y) * 0.5f);
                dl->AddText(msg_pos, load_col, load_msg, 0.95f);
            }
            else if (!local_avail) {
                // No Lyrics Available
                const char* msg = "No lyrics found for this track.";
                Vec2 msg_sz = Font::CalcTextSize(msg, 0.95f);
                Vec2 msg_pos(lyr_min.x + (lyr_max.x - lyr_min.x - msg_sz.x) * 0.5f,
                             lyr_min.y + (lyr_view_h - msg_sz.y) * 0.5f);
                dl->AddText(msg_pos, Color(110, 116, 135), msg, 0.95f);
            }
            else if (local_synced && !local_synced_lines.empty()) {
                // Active Line Detection with gentle vocal anticipation offset (150ms)
                uint64_t cur_ms = static_cast<uint64_t>((s_track.current_seconds + 0.15f) * 1000.0f);
                int active_idx = -1;
                for (int i = static_cast<int>(local_synced_lines.size()) - 1; i >= 0; --i) {
                    if (cur_ms >= local_synced_lines[i].time_ms) {
                        active_idx = i;
                        break;
                    }
                }

                // Dynamic Smooth Auto-Centering Carousel View
                float line_step = 34.0f;
                float target_scroll = static_cast<float>(active_idx >= 0 ? active_idx : 0);

                // Mouse wheel interactive scroll support within lyrics container
                if (Rect(lyr_min, lyr_max).Contains(input.mouse_pos) && std::abs(input.mouse_wheel) > 0.01f) {
                    s_lyrics_scroll_y -= input.mouse_wheel * 1.8f;
                }

                // Smooth exponential easing for cinematic gliding, instant snap on first track sync
                if (s_current_lyric_line == -1) {
                    s_lyrics_scroll_y = target_scroll;
                } else {
                    s_lyrics_scroll_y += (target_scroll - s_lyrics_scroll_y) * (1.0f - std::exp(-8.0f * dt));
                }
                s_current_lyric_line = active_idx;

                float pill_min_x = lyr_min.x + 8.0f;
                float pill_max_x = lyr_max.x - 8.0f;
                float max_text_w = (pill_max_x - pill_min_x) - 24.0f;

                // Exact vertical center slot in the lyrics viewport
                float center_slot_y = lyr_min.y + (lyr_view_h * 0.5f) - (line_step * 0.5f);

                // Render lines
                for (size_t i = 0; i < local_synced_lines.size(); ++i) {
                    float line_y = center_slot_y + (static_cast<float>(i) - s_lyrics_scroll_y) * line_step;

                    // Frustum culling
                    if (line_y + line_step < lyr_min.y - 10.0f || line_y > lyr_max.y + 10.0f) continue;

                    bool is_active = (static_cast<int>(i) == active_idx);
                    Rect line_rect(pill_min_x, line_y + 2.0f, pill_max_x, line_y + line_step - 2.0f);
                    bool line_hovered = line_rect.Contains(input.mouse_pos);

                    // Interactive seek by clicking on any lyric line
                    if (line_hovered && input.mouse_clicked && s_track.total_seconds > 0.0f) {
                        float seek_pct = static_cast<float>(local_synced_lines[i].time_ms) / (s_track.total_seconds * 1000.0f);
                        Seek(seek_pct);
                    }

                    // Auto-fit lyric text so it never extends past pill or card bounds
                    float base_scale = is_active ? 1.05f : 0.95f;
                    FormattedLyricLine fmt = FitLyricLine(local_synced_lines[i].text, base_scale, max_text_w, 0.75f);

                    // Horizontally center text inside the lyrics viewport
                    float text_x = lyr_min.x + (lyr_view_w - fmt.size.x) * 0.5f;

                    // Exact vertical centering inside the pill
                    float pill_center_y = (line_rect.min.y + line_rect.max.y) * 0.5f;
                    float text_y = pill_center_y - (fmt.size.y * 0.5f);

                    if (is_active) {
                        // Spotlight Glow Pill with liquid glass highlight
                        dl->AddGlow(line_rect.min, line_rect.max, 6.0f, 3.0f, Color(124, 58, 237, 75));
                        dl->AddRectFilledGradient(line_rect.min, line_rect.max,
                            Color(56, 189, 248, 45), Color(139, 92, 246, 70), Color(139, 92, 246, 70), Color(56, 189, 248, 45));
                        dl->AddRect(line_rect.min, line_rect.max, Color(168, 85, 247, 210), 6.0f, 1.0f);
                        dl->AddLine(Vec2(line_rect.min.x + 6.0f, line_rect.min.y + 0.5f), Vec2(line_rect.max.x - 6.0f, line_rect.min.y + 0.5f), Color(255, 255, 255, 120), 1.0f);
                        dl->AddText(Vec2(text_x, text_y), Color(255, 255, 255), fmt.text.c_str(), fmt.scale);
                    } else {
                        Color text_col;
                        if (line_hovered) {
                            text_col = Color(245, 248, 255, 255);
                            dl->AddRectFilled(line_rect.min, line_rect.max, Color(255, 255, 255, 12), 4.0f);
                        } else if (active_idx < 0) {
                            // Song intro before first lyric timestamp
                            if (i == 0) {
                                text_col = Color(235, 240, 255, 255);
                                dl->AddRect(line_rect.min, line_rect.max, Color(124, 58, 237, 90), 5.0f, 1.0f);
                            } else if (i == 1) {
                                text_col = Color(185, 192, 212, 220);
                            } else if (i == 2) {
                                text_col = Color(135, 142, 162, 170);
                            } else {
                                text_col = Color(80, 86, 105, 110);
                            }
                        } else {
                            int dist = std::abs(static_cast<int>(i) - active_idx);
                            if (dist == 1) {
                                text_col = Color(185, 192, 212, 230);
                            } else if (dist == 2) {
                                text_col = Color(125, 132, 152, 170);
                            } else {
                                text_col = Color(72, 78, 98, 110);
                            }
                        }
                        dl->AddText(Vec2(text_x, text_y), text_col, fmt.text.c_str(), fmt.scale);
                    }
                }

                // Top & Bottom subtle gradient fade overlays so lines seamlessly emerge and dissolve
                dl->AddRectFilledGradient(lyr_min, Vec2(lyr_max.x, lyr_min.y + 30.0f),
                    Color(10, 11, 17, 245), Color(10, 11, 17, 245), Color(10, 11, 17, 0), Color(10, 11, 17, 0));
                dl->AddRectFilledGradient(Vec2(lyr_min.x, lyr_max.y - 30.0f), lyr_max,
                    Color(10, 11, 17, 0), Color(10, 11, 17, 0), Color(10, 11, 17, 245), Color(10, 11, 17, 245));

            } else if (!local_plain_lines.empty()) {
                // Plain (Unsynced) Lyrics: Interpolated Smooth Carousel
                int plain_active_idx = 0;
                if (s_track.total_seconds > 0.0f) {
                    float prog = std::clamp(s_track.progress, 0.0f, 1.0f);
                    plain_active_idx = static_cast<int>(prog * static_cast<float>(local_plain_lines.size()));
                    if (plain_active_idx >= static_cast<int>(local_plain_lines.size()))
                        plain_active_idx = static_cast<int>(local_plain_lines.size()) - 1;
                }

                float line_step = 32.0f;
                float target_scroll = static_cast<float>(plain_active_idx);

                if (Rect(lyr_min, lyr_max).Contains(input.mouse_pos) && std::abs(input.mouse_wheel) > 0.01f) {
                    s_lyrics_scroll_y -= input.mouse_wheel * 1.8f;
                }

                if (s_current_lyric_line == -1) {
                    s_lyrics_scroll_y = target_scroll;
                } else {
                    s_lyrics_scroll_y += (target_scroll - s_lyrics_scroll_y) * (1.0f - std::exp(-8.0f * dt));
                }
                s_current_lyric_line = plain_active_idx;

                float pill_min_x = lyr_min.x + 8.0f;
                float pill_max_x = lyr_max.x - 8.0f;
                float max_text_w = (pill_max_x - pill_min_x) - 24.0f;
                float center_slot_y = lyr_min.y + (lyr_view_h * 0.5f) - (line_step * 0.5f);

                for (size_t i = 0; i < local_plain_lines.size(); ++i) {
                    float line_y = center_slot_y + (static_cast<float>(i) - s_lyrics_scroll_y) * line_step;
                    if (line_y + line_step < lyr_min.y - 10.0f || line_y > lyr_max.y + 10.0f) continue;

                    bool is_active = (static_cast<int>(i) == plain_active_idx);
                    Rect line_rect(pill_min_x, line_y + 2.0f, pill_max_x, line_y + line_step - 2.0f);
                    bool line_hovered = line_rect.Contains(input.mouse_pos);

                    if (line_hovered && input.mouse_clicked && !local_plain_lines.empty()) {
                        float seek_pct = static_cast<float>(i) / static_cast<float>(local_plain_lines.size());
                        Seek(seek_pct);
                    }

                    float base_scale = is_active ? 1.05f : 0.95f;
                    FormattedLyricLine fmt = FitLyricLine(local_plain_lines[i], base_scale, max_text_w, 0.75f);
                    float text_x = lyr_min.x + (lyr_view_w - fmt.size.x) * 0.5f;
                    float pill_center_y = (line_rect.min.y + line_rect.max.y) * 0.5f;
                    float text_y = pill_center_y - (fmt.size.y * 0.5f);

                    if (is_active) {
                        dl->AddGlow(line_rect.min, line_rect.max, 6.0f, 3.0f, Color(124, 58, 237, 75));
                        dl->AddRectFilledGradient(line_rect.min, line_rect.max,
                            Color(56, 189, 248, 45), Color(139, 92, 246, 70), Color(139, 92, 246, 70), Color(56, 189, 248, 45));
                        dl->AddRect(line_rect.min, line_rect.max, Color(168, 85, 247, 210), 6.0f, 1.0f);
                        dl->AddLine(Vec2(line_rect.min.x + 6.0f, line_rect.min.y + 0.5f), Vec2(line_rect.max.x - 6.0f, line_rect.min.y + 0.5f), Color(255, 255, 255, 120), 1.0f);
                        dl->AddText(Vec2(text_x, text_y), Color(255, 255, 255), fmt.text.c_str(), fmt.scale);
                    } else {
                        Color text_col;
                        if (line_hovered) {
                            text_col = Color(245, 248, 255, 255);
                            dl->AddRectFilled(line_rect.min, line_rect.max, Color(255, 255, 255, 12), 4.0f);
                        } else {
                            int dist = std::abs(static_cast<int>(i) - plain_active_idx);
                            if (dist == 1) text_col = Color(185, 192, 212, 230);
                            else if (dist == 2) text_col = Color(125, 132, 152, 170);
                            else text_col = Color(72, 78, 98, 110);
                        }
                        dl->AddText(Vec2(text_x, text_y), text_col, fmt.text.c_str(), fmt.scale);
                    }
                }

                // Top & Bottom subtle gradient fade overlays
                dl->AddRectFilledGradient(lyr_min, Vec2(lyr_max.x, lyr_min.y + 30.0f),
                    Color(10, 11, 17, 245), Color(10, 11, 17, 245), Color(10, 11, 17, 0), Color(10, 11, 17, 0));
                dl->AddRectFilledGradient(Vec2(lyr_min.x, lyr_max.y - 30.0f), lyr_max,
                    Color(10, 11, 17, 0), Color(10, 11, 17, 0), Color(10, 11, 17, 245), Color(10, 11, 17, 245));
            }

            dl->PopClipRect();
        }
    }

} // namespace VoidGUI
