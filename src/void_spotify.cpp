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
#include <atomic>
#include <cctype>

namespace VoidGUI {

    struct MusicRgb {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
    };

    struct MusicPalette {
        Color top      = Color(38, 22, 34, 255);
        Color bottom   = Color(16, 12, 22, 255);
        Color accentA  = Color(245, 138, 70, 255);
        Color accentB  = Color(215, 85, 142, 255);
        Color accentC  = Color(140, 90, 210, 255);
        Color progress = Color(245, 145, 85, 255);
    };

    struct StagedMediaState {
        std::mutex mtx;
        std::string title;
        std::string artist;
        std::string album;
        bool is_playing = false;
        bool has_playback_info = false;
        bool repeat = false;
        float total_seconds = 0.0f;
        float current_seconds = 0.0f;
        bool timeline_updated = false;

        std::vector<uint8_t> pixel_data;
        int pixel_w = 0;
        int pixel_h = 0;
        bool new_pixels_ready = false;
    };
    static StagedMediaState s_staged;

    static std::thread s_worker_thread;
    static std::atomic<bool> s_worker_running{ false };
    static uint32_t s_last_thumb_size = 0;

    MediaTrackInfo SpotifyPlayer::s_track;
    float SpotifyPlayer::s_poll_timer = 0.0f;
    float SpotifyPlayer::s_motion = 0.0f;
    bool  SpotifyPlayer::s_is_seeking = false;

    PlayerViewMode SpotifyPlayer::s_view_mode = PlayerViewMode::Lyrics;
    PlayerViewMode SpotifyPlayer::s_prev_view_mode = PlayerViewMode::Compact;
    Vec2  SpotifyPlayer::s_current_size = Vec2(335.0f, 484.0f);
    Vec2  SpotifyPlayer::s_target_size  = Vec2(335.0f, 484.0f);
    bool  SpotifyPlayer::s_mode_transition = false;

    float SpotifyPlayer::s_lyrics_scroll_y = 0.0f;
    float SpotifyPlayer::s_lyrics_scroll_target = 0.0f;
    int   SpotifyPlayer::s_current_lyric_line = -1;
    std::string SpotifyPlayer::s_last_lyrics_track = "";
    bool  SpotifyPlayer::s_manual_scroll = false;
    uint64_t SpotifyPlayer::s_manual_scroll_until_ms = 0;
    bool  SpotifyPlayer::s_position_sync_requested = false;

    static uint64_t s_last_sync_tick = 0;
    static float s_last_known_pos_sec = 0.0f;
    static float s_last_winrt_reported_pos = -1.0f;

    static MusicPalette s_palette_current;
    static MusicPalette s_palette_target;
    static bool s_palette_ready = false;

    static std::mutex s_lyrics_mutex;
    static std::vector<LyricLine> s_synced_lyrics;
    static std::vector<std::string> s_plain_lyrics;
    static bool s_lyrics_available = false;
    static bool s_lyrics_synced = false;
    static bool s_lyrics_fetching = false;

    static ID3D11ShaderResourceView* s_album_art_srv = nullptr;

    static bool s_winrt_initialized = false;
    static winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager s_session_manager{ nullptr };
    static winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession s_session{ nullptr };

    static void FormatTimeSeconds(float sec, char* buf, size_t buf_sz, bool pad_min = false) {
        if (sec < 0.0f) sec = 0.0f;
        int total_s = static_cast<int>(sec);
        int m = total_s / 60;
        int s = total_s % 60;
        if (pad_min) {
            snprintf(buf, buf_sz, "%02d:%02d", m, s);
        } else {
            snprintf(buf, buf_sz, "%d:%02d", m, s);
        }
    }

    static std::string EllipsizeText(const std::string& text, float scale, float max_width) {
        Vec2 sz = Font::CalcTextSize(text.c_str(), scale);
        if (sz.x <= max_width) return text;

        std::string s = text;
        while (!s.empty()) {
            s.pop_back();
            std::string candidate = s + "...";
            if (Font::CalcTextSize(candidate.c_str(), scale).x <= max_width) {
                return candidate;
            }
        }
        return text;
    }

    static std::string WrapText(const std::string& text, float scale, float max_width, int* out_lines = nullptr) {
        if (text.empty()) {
            if (out_lines) *out_lines = 1;
            return "";
        }
        if (Font::CalcTextSize(text.c_str(), scale).x <= max_width) {
            if (out_lines) *out_lines = 1;
            return text;
        }

        std::istringstream iss(text);
        std::string word;
        std::string result;
        std::string cur_line;
        int lines = 1;

        while (iss >> word) {
            std::string test = cur_line.empty() ? word : (cur_line + " " + word);
            if (Font::CalcTextSize(test.c_str(), scale).x <= max_width) {
                cur_line = test;
            } else {
                if (!cur_line.empty()) {
                    if (!result.empty()) result += "\n";
                    result += cur_line;
                    lines++;
                    cur_line = word;
                } else {
                    if (!result.empty()) result += "\n";
                    result += word;
                    lines++;
                    cur_line.clear();
                }
            }
        }
        if (!cur_line.empty()) {
            if (!result.empty()) result += "\n";
            result += cur_line;
        }
        if (out_lines) *out_lines = lines;
        return result;
    }

    static std::string FitLyricText(const std::string& text, float max_width, bool is_active, float& out_scale, int& out_lines) {
        float max_s = is_active ? 1.04f : 0.94f;
        float min_s = is_active ? 0.84f : 0.78f;

        for (float s = max_s; s >= min_s; s -= 0.03f) {
            if (Font::CalcTextSize(text.c_str(), s).x <= max_width) {
                out_scale = s;
                out_lines = 1;
                return text;
            }
        }

        out_scale = is_active ? 0.90f : 0.82f;
        return WrapText(text, out_scale, max_width, &out_lines);
    }

    static float Luma(const MusicRgb& c) {
        return c.r * 0.2126f + c.g * 0.7152f + c.b * 0.0722f;
    }

    static float Saturation(const MusicRgb& c) {
        float hi = (std::max)(c.r, (std::max)(c.g, c.b));
        float lo = (std::min)(c.r, (std::min)(c.g, c.b));
        return hi > 0.0001f ? (hi - lo) / hi : 0.0f;
    }

    static MusicRgb MixRgb(const MusicRgb& a, const MusicRgb& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return {
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t
        };
    }

    static MusicRgb ToneRgb(MusicRgb c, float value, float sat_scale, float sat_floor) {
        float hi = (std::max)(c.r, (std::max)(c.g, c.b));
        float lo = (std::min)(c.r, (std::min)(c.g, c.b));
        float delta = hi - lo;
        float hue = 0.0f;
        if (delta > 0.0001f) {
            if (hi == c.r) hue = std::fmod((c.g - c.b) / delta, 6.0f);
            else if (hi == c.g) hue = (c.b - c.r) / delta + 2.0f;
            else hue = (c.r - c.g) / delta + 4.0f;
            hue /= 6.0f;
            if (hue < 0.0f) hue += 1.0f;
        }
        float sat = hi > 0.0001f ? delta / hi : 0.0f;
        sat = std::clamp(sat * sat_scale + 0.04f, sat_floor, 0.92f);
        value = std::clamp(value, 0.0f, 1.0f);

        float h6 = hue * 6.0f;
        int sector = static_cast<int>(std::floor(h6));
        float frac = h6 - static_cast<float>(sector);
        float p = value * (1.0f - sat);
        float q = value * (1.0f - sat * frac);
        float t = value * (1.0f - sat * (1.0f - frac));
        switch ((sector % 6 + 6) % 6) {
        case 0: return { value, t, p };
        case 1: return { q, value, p };
        case 2: return { p, value, t };
        case 3: return { p, q, value };
        case 4: return { t, p, value };
        default:return { value, p, q };
        }
    }

    static Color RgbToColor(const MusicRgb& c, uint8_t a = 255) {
        uint8_t r = static_cast<uint8_t>(std::clamp(c.r * 255.0f, 0.0f, 255.0f));
        uint8_t g = static_cast<uint8_t>(std::clamp(c.g * 255.0f, 0.0f, 255.0f));
        uint8_t b = static_cast<uint8_t>(std::clamp(c.b * 255.0f, 0.0f, 255.0f));
        return Color(r, g, b, a);
    }

    static MusicPalette ExtractPalette(const uint8_t* rgba, int width, int height) {
        MusicPalette fallback;
        if (!rgba || width <= 0 || height <= 0) return fallback;

        constexpr int kBits = 5;
        constexpr int kLevels = 1 << kBits;
        constexpr int kBucketCount = kLevels * kLevels * kLevels;

        struct Bucket { float r = 0, g = 0, b = 0, weight = 0; };
        std::vector<Bucket> buckets(kBucketCount);

        int step_x = (std::max)(1, width / 180);
        int step_y = (std::max)(1, height / 180);
        MusicRgb overall = {};
        float overall_weight = 0.0f;

        for (int y = 0; y < height; y += step_y) {
            for (int x = 0; x < width; x += step_x) {
                const uint8_t* px = rgba + (static_cast<size_t>(y) * width + x) * 4;
                float alpha = px[3] / 255.0f;
                if (alpha < 0.25f) continue;
                MusicRgb c = { px[0] / 255.0f, px[1] / 255.0f, px[2] / 255.0f };
                float val = (std::max)(c.r, (std::max)(c.g, c.b));
                float sat = Saturation(c);

                overall.r += c.r * alpha;
                overall.g += c.g * alpha;
                overall.b += c.b * alpha;
                overall_weight += alpha;

                if (val < 0.10f || (val > 0.96f && sat < 0.10f)) continue;

                int ri = (std::min)(kLevels - 1, static_cast<int>(c.r * kLevels));
                int gi = (std::min)(kLevels - 1, static_cast<int>(c.g * kLevels));
                int bi = (std::min)(kLevels - 1, static_cast<int>(c.b * kLevels));
                Bucket& bkt = buckets[static_cast<size_t>(ri * kLevels + gi) * kLevels + bi];
                float w = alpha * (0.85f + sat * 0.35f);
                bkt.r += c.r * w;
                bkt.g += c.g * w;
                bkt.b += c.b * w;
                bkt.weight += w;
            }
        }

        if (overall_weight > 0.0f) {
            overall.r /= overall_weight;
            overall.g /= overall_weight;
            overall.b /= overall_weight;
        }

        struct Candidate { MusicRgb color; float weight; float score; };
        std::vector<Candidate> candidates;
        candidates.reserve(64);
        for (const Bucket& b : buckets) {
            if (b.weight <= 0.0f) continue;
            MusicRgb c = { b.r / b.weight, b.g / b.weight, b.b / b.weight };
            float sat = Saturation(c);
            float lum = Luma(c);
            float lum_window = std::exp(-((lum - 0.58f) * (lum - 0.58f)) / 0.10f);
            float score = std::sqrt(b.weight) * (0.80f + sat * 0.45f) * (0.30f + lum_window);
            candidates.push_back({ c, b.weight, score });
        }
        if (candidates.empty()) return fallback;

        std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) { return a.score > b.score; });

        MusicRgb picks[3];
        int picked = 0;
        for (const Candidate& cand : candidates) {
            bool too_close = false;
            for (int i = 0; i < picked && !too_close; ++i) {
                float dr = cand.color.r - picks[i].r;
                float dg = cand.color.g - picks[i].g;
                float db = cand.color.b - picks[i].b;
                too_close = (dr * dr + dg * dg + db * db) < 0.055f;
            }
            if (too_close) continue;
            picks[picked++] = cand.color;
            if (picked == 3) break;
        }
        while (picked < 3) {
            picks[picked] = (picked == 0) ? overall : picks[picked - 1];
            ++picked;
        }

        MusicRgb a = picks[0], b = picks[1], c = picks[2];
        MusicPalette res;
        res.top = RgbToColor(ToneRgb(MixRgb(overall, a, 0.40f), 0.32f, 0.72f, 0.18f));
        res.bottom = RgbToColor(ToneRgb(MixRgb(overall, b, 0.35f), 0.16f, 0.65f, 0.14f));
        res.accentA = RgbToColor(ToneRgb(a, 0.85f, 1.18f, 0.36f));
        res.accentB = RgbToColor(ToneRgb(b, 0.80f, 1.14f, 0.32f));
        res.accentC = RgbToColor(ToneRgb(c, 0.76f, 1.10f, 0.28f));
        res.progress = res.accentA;
        return res;
    }

    static void DrawAtmosphericBlob(DrawList* dl, Vec2 center, float radius, Color col, float max_alpha) {
        constexpr int layers = 10;
        for (int i = layers; i >= 1; --i) {
            float scale = static_cast<float>(i) / static_cast<float>(layers);
            float layer_a = max_alpha * (1.15f - scale) / static_cast<float>(layers);
            uint8_t a = static_cast<uint8_t>(std::clamp(layer_a * 255.0f, 0.0f, 255.0f));
            dl->AddCircleFilled(center, radius * scale, col.WithAlpha(a), 32);
        }
    }

    static void DrawMediaPlay(DrawList* dl, Vec2 c, float r, Color col) {
        Vec2 p1(c.x - r * 0.45f, c.y - r * 0.62f);
        Vec2 p2(c.x + r * 0.65f, c.y);
        Vec2 p3(c.x - r * 0.45f, c.y + r * 0.62f);
        dl->AddTriangleFilled(p1, p2, p3, col);
    }

    static void DrawMediaPause(DrawList* dl, Vec2 c, float r, Color col) {
        float bar_w = (std::max)(1.5f, r * 0.32f);
        float bar_h = r * 1.25f;
        float gap = r * 0.30f;
        dl->AddRectFilled(Vec2(c.x - gap - bar_w, c.y - bar_h * 0.5f),
                          Vec2(c.x - gap, c.y + bar_h * 0.5f), col, 1.0f);
        dl->AddRectFilled(Vec2(c.x + gap, c.y - bar_h * 0.5f),
                          Vec2(c.x + gap + bar_w, c.y + bar_h * 0.5f), col, 1.0f);
    }

    static void DrawMediaPrev(DrawList* dl, Vec2 c, float r, Color col) {
        float w = r * 0.60f;
        float h = r * 0.58f;
        Vec2 p1(c.x, c.y - h);
        Vec2 p2(c.x - w, c.y);
        Vec2 p3(c.x, c.y + h);
        dl->AddTriangleFilled(p1, p2, p3, col);

        Vec2 p4(c.x + w, c.y - h);
        Vec2 p5(c.x, c.y);
        Vec2 p6(c.x + w, c.y + h);
        dl->AddTriangleFilled(p4, p5, p6, col);
    }

    static void DrawMediaNext(DrawList* dl, Vec2 c, float r, Color col) {
        float w = r * 0.60f;
        float h = r * 0.58f;
        Vec2 p1(c.x - w, c.y - h);
        Vec2 p2(c.x, c.y);
        Vec2 p3(c.x - w, c.y + h);
        dl->AddTriangleFilled(p1, p2, p3, col);

        Vec2 p4(c.x, c.y - h);
        Vec2 p5(c.x + w, c.y);
        Vec2 p6(c.x, c.y + h);
        dl->AddTriangleFilled(p4, p5, p6, col);
    }

    static void DrawLucideShuffle(DrawList* dl, Vec2 c, float r, Color col) {
        float k = r * 0.78f;
        float th = (std::max)(1.4f, r * 0.18f);

        dl->AddLine(Vec2(c.x + k * 0.35f, c.y - k * 0.85f), Vec2(c.x + k, c.y - k * 0.5f), col, th);
        dl->AddLine(Vec2(c.x + k * 0.35f, c.y - k * 0.15f), Vec2(c.x + k, c.y - k * 0.5f), col, th);
        dl->AddLine(Vec2(c.x + k * 0.35f, c.y + k * 0.15f), Vec2(c.x + k, c.y + k * 0.5f), col, th);
        dl->AddLine(Vec2(c.x + k * 0.35f, c.y + k * 0.85f), Vec2(c.x + k, c.y + k * 0.5f), col, th);

        dl->AddLine(Vec2(c.x - k, -k * 0.5f + c.y), Vec2(c.x - k * 0.2f, -k * 0.5f + c.y), col, th);
        dl->AddLine(Vec2(c.x - k * 0.2f, -k * 0.5f + c.y), Vec2(c.x + k * 0.5f, k * 0.5f + c.y), col, th);
        dl->AddLine(Vec2(c.x + k * 0.5f, k * 0.5f + c.y), Vec2(c.x + k, k * 0.5f + c.y), col, th);

        dl->AddLine(Vec2(c.x - k, k * 0.5f + c.y), Vec2(c.x - k * 0.2f, k * 0.5f + c.y), col, th);
        dl->AddLine(Vec2(c.x - k * 0.2f, k * 0.5f + c.y), Vec2(c.x + k * 0.5f, -k * 0.5f + c.y), col, th);
        dl->AddLine(Vec2(c.x + k * 0.5f, -k * 0.5f + c.y), Vec2(c.x + k, -k * 0.5f + c.y), col, th);
    }

    static void DrawLucideRepeat(DrawList* dl, Vec2 c, float r, Color col) {
        float k = r * 0.75f;
        float th = (std::max)(1.4f, r * 0.18f);

        dl->AddLine(Vec2(c.x - k * 0.7f, c.y - k * 0.5f), Vec2(c.x + k * 0.6f, c.y - k * 0.5f), col, th);
        dl->AddLine(Vec2(c.x + k * 0.6f, c.y - k * 0.5f), Vec2(c.x + k, c.y - k * 0.1f), col, th);
        dl->AddLine(Vec2(c.x + k, c.y - k * 0.1f), Vec2(c.x + k, c.y + k * 0.2f), col, th);

        dl->AddLine(Vec2(c.x + k * 0.4f, c.y - k * 0.9f), Vec2(c.x + k * 0.8f, c.y - k * 0.5f), col, th);
        dl->AddLine(Vec2(c.x + k * 0.4f, c.y - k * 0.1f), Vec2(c.x + k * 0.8f, c.y - k * 0.5f), col, th);

        dl->AddLine(Vec2(c.x + k * 0.7f, c.y + k * 0.5f), Vec2(c.x - k * 0.6f, c.y + k * 0.5f), col, th);
        dl->AddLine(Vec2(c.x - k * 0.6f, c.y + k * 0.5f), Vec2(c.x - k, c.y + k * 0.1f), col, th);
        dl->AddLine(Vec2(c.x - k, c.y + k * 0.1f), Vec2(c.x - k, c.y - k * 0.2f), col, th);

        dl->AddLine(Vec2(c.x - k * 0.4f, c.y + k * 0.9f), Vec2(c.x - k * 0.8f, c.y + k * 0.5f), col, th);
        dl->AddLine(Vec2(c.x - k * 0.4f, c.y + k * 0.1f), Vec2(c.x - k * 0.8f, c.y + k * 0.5f), col, th);
    }

    static void DrawChatQuoteBubble(DrawList* dl, Vec2 c, float r, Color col, bool filled) {
        Vec2 b_min(c.x - r * 0.80f, c.y - r * 0.70f);
        Vec2 b_max(c.x + r * 0.80f, c.y + r * 0.40f);
        float round = 3.5f;

        if (filled) {
            dl->AddRectFilled(b_min, b_max, col, round);
            Vec2 t1(b_min.x + 2.0f, b_max.y);
            Vec2 t2(b_min.x + 7.0f, b_max.y);
            Vec2 t3(b_min.x - 1.5f, b_max.y + r * 0.40f);
            dl->AddTriangleFilled(t1, t2, t3, col);

            Color ink = Color(20, 24, 36, col.a);
            dl->AddCircleFilled(Vec2(c.x - r * 0.30f, c.y - r * 0.12f), 1.4f, ink, 8);
            dl->AddCircleFilled(Vec2(c.x + r * 0.30f, c.y - r * 0.12f), 1.4f, ink, 8);
        } else {
            dl->AddRect(b_min, b_max, col, round, 1.4f);
            Vec2 t1(b_min.x + 2.0f, b_max.y);
            Vec2 t2(b_min.x + 7.0f, b_max.y);
            Vec2 t3(b_min.x - 1.5f, b_max.y + r * 0.40f);
            dl->AddLine(t1, t3, col, 1.4f);
            dl->AddLine(t3, t2, col, 1.4f);

            dl->AddCircleFilled(Vec2(c.x - r * 0.30f, c.y - r * 0.12f), 1.3f, col, 8);
            dl->AddCircleFilled(Vec2(c.x + r * 0.30f, c.y - r * 0.12f), 1.3f, col, 8);
        }
    }

    static void DrawChevronBack(DrawList* dl, Vec2 c, float r, Color col) {
        float sx = r * 0.50f;
        float sy = r * 0.70f;
        dl->AddLine(Vec2(c.x + sx * 0.4f, c.y - sy), Vec2(c.x - sx * 0.6f, c.y), col, 2.0f);
        dl->AddLine(Vec2(c.x - sx * 0.6f, c.y), Vec2(c.x + sx * 0.4f, c.y + sy), col, 2.0f);
    }

    static std::string CleanTrackTitle(const std::string& title, bool aggressive = false) {
        std::string clean = title;
        if (aggressive) {
            size_t p = clean.find_first_of("([ -");
            if (p != std::string::npos && p > 2) {
                clean = clean.substr(0, p);
            }
        } else {
            const char* patterns[] = {
                " (feat.", " (ft.", " (with ", " [feat.", " [ft.",
                " (Official", " [Official", " (Remastered", " - Remastered",
                " - Live", " (Live", " - Mono", " (Mono", " (Radio Edit)",
                " (Strings Version)", " - Strings Version", " [Strings Version]",
                " (Explicit)", " [Explicit]", " (Deluxe)", " [Deluxe]",
                " (Deluxe Edition)", " [Deluxe Edition]", " - Deluxe",
                " - Single", " (Single Version)", " - Explicit", " - Clean", " (Clean)"
            };
            for (const char* pat : patterns) {
                size_t pos = clean.find(pat);
                if (pos != std::string::npos) {
                    clean = clean.substr(0, pos);
                }
            }
        }
        while (!clean.empty() && std::isspace(static_cast<unsigned char>(clean.front()))) clean.erase(clean.begin());
        while (!clean.empty() && std::isspace(static_cast<unsigned char>(clean.back()))) clean.pop_back();
        return clean;
    }

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

    static bool ParseLrcString(const std::string& lrc_content, std::vector<LyricLine>& out_lines) {
        out_lines.clear();
        std::istringstream stream(lrc_content);
        std::string line;

        while (std::getline(stream, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.empty()) continue;

            size_t open_pos = line.find('[');
            size_t close_pos = line.find(']');
            if (open_pos != std::string::npos && close_pos != std::string::npos && close_pos > open_pos) {
                std::string tag = line.substr(open_pos + 1, close_pos - open_pos - 1);
                std::string lyric_text = line.substr(close_pos + 1);

                while (!lyric_text.empty() && (lyric_text.front() == ' ' || lyric_text.front() == '\t')) lyric_text.erase(lyric_text.begin());
                while (!lyric_text.empty() && (lyric_text.back() == ' ' || lyric_text.back() == '\t')) lyric_text.pop_back();

                int min = 0;
                float sec = 0.0f;
                if (sscanf_s(tag.c_str(), "%d:%f", &min, &sec) == 2) {
                    uint64_t total_ms = static_cast<uint64_t>(min * 60000 + static_cast<int>(sec * 1000.0f));
                    if (!lyric_text.empty() && lyric_text != "...") {
                        LyricLine item;
                        item.time_ms = total_ms;
                        item.text = lyric_text;
                        out_lines.push_back(item);
                    }
                }
            }
        }

        std::sort(out_lines.begin(), out_lines.end(), [](const LyricLine& a, const LyricLine& b) {
            return a.time_ms < b.time_ms;
        });

        return !out_lines.empty();
    }

    static std::string FetchUrlString(const std::wstring& domain, const std::wstring& path) {
        std::string response_data;
        HINTERNET hSession = WinHttpOpen(L"KtnaGUI/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return response_data;

        WinHttpSetTimeouts(hSession, 1500, 1500, 2000, 2000);

        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
        protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
        WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

        HINTERNET hConnect = WinHttpConnect(hSession, domain.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return response_data;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                               NULL, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               WINHTTP_FLAG_SECURE);
        if (hRequest) {
            DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));

            const wchar_t* headers = L"Accept: application/json\r\n";
            if (WinHttpSendRequest(hRequest, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                if (WinHttpReceiveResponse(hRequest, NULL)) {
                    DWORD bytes_available = 0;
                    while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
                        std::vector<char> buffer(bytes_available);
                        DWORD bytes_read = 0;
                        if (WinHttpReadData(hRequest, buffer.data(), bytes_available, &bytes_read) && bytes_read > 0) {
                            response_data.append(buffer.data(), bytes_read);
                        }
                    }
                }
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response_data;
    }

    static std::string UrlEncode(const std::string& value) {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;
        for (char c : value) {
            if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
            } else {
                escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
            }
        }
        return escaped.str();
    }

    static std::string ExtractJsonStringField(const std::string& json, const std::string& field) {
        std::string search = "\"" + field + "\":";
        size_t search_pos = 0;
        while ((search_pos = json.find(search, search_pos)) != std::string::npos) {
            size_t pos = search_pos + search.length();
            while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
            if (pos < json.length() && json[pos] == '"') {
                pos++;
                std::string result;
                bool escaped = false;
                while (pos < json.length()) {
                    char c = json[pos++];
                    if (escaped) {
                        if (c == 'n') result += '\n';
                        else if (c == 'r') result += '\r';
                        else if (c == 't') result += '\t';
                        else if (c == '"') result += '"';
                        else if (c == '\\') result += '\\';
                        else result += c;
                        escaped = false;
                    } else if (c == '\\') {
                        escaped = true;
                    } else if (c == '"') {
                        if (!result.empty()) return result;
                        break;
                    } else {
                        result += c;
                    }
                }
            }
            search_pos = pos;
        }
        return "";
    }

    static std::atomic<uint32_t> s_lyrics_request_id{ 0 };

    void SpotifyPlayer::FetchLyricsAsync(const std::string& raw_artist, const std::string& raw_title, int duration_secs) {
        std::string artist = CleanArtistName(raw_artist);
        std::string title = CleanTrackTitle(raw_title, false);

        std::string track_id = artist + " - " + title;
        if (track_id == s_last_lyrics_track) return;
        s_last_lyrics_track = track_id;

        uint32_t req_id = ++s_lyrics_request_id;

        {
            std::lock_guard<std::mutex> lock(s_lyrics_mutex);
            s_lyrics_available = false;
            s_lyrics_synced = false;
            s_lyrics_fetching = true;
            s_synced_lyrics.clear();
            s_plain_lyrics.clear();
        }

        std::thread([artist, title, raw_title, duration_secs, req_id]() {
            std::string domain = "lrclib.net";
            std::wstring wdomain(domain.begin(), domain.end());

            std::string path_search = "/api/search?q=" + UrlEncode(artist + " " + title);
            std::wstring wpath_search(path_search.begin(), path_search.end());
            std::string json = FetchUrlString(wdomain, wpath_search);

            std::string synced_lrc = ExtractJsonStringField(json, "syncedLyrics");
            std::string plain_lrc = ExtractJsonStringField(json, "plainLyrics");

            if (synced_lrc.empty() && plain_lrc.empty()) {
                std::string path_exact = "/api/get?artist_name=" + UrlEncode(artist) + "&track_name=" + UrlEncode(title);
                std::wstring wpath_exact(path_exact.begin(), path_exact.end());
                json = FetchUrlString(wdomain, wpath_exact);

                synced_lrc = ExtractJsonStringField(json, "syncedLyrics");
                plain_lrc = ExtractJsonStringField(json, "plainLyrics");
            }

            if (synced_lrc.empty() && plain_lrc.empty()) {
                std::string agg_title = CleanTrackTitle(raw_title, true);
                if (!agg_title.empty() && agg_title != title) {
                    std::string path_agg = "/api/search?q=" + UrlEncode(artist + " " + agg_title);
                    std::wstring wpath_agg(path_agg.begin(), path_agg.end());
                    json = FetchUrlString(wdomain, wpath_agg);

                    synced_lrc = ExtractJsonStringField(json, "syncedLyrics");
                    plain_lrc = ExtractJsonStringField(json, "plainLyrics");
                }
            }

            if (req_id != s_lyrics_request_id.load()) return;

            std::vector<LyricLine> parsed_synced;
            std::vector<std::string> parsed_plain;

            if (!synced_lrc.empty()) {
                ParseLrcString(synced_lrc, parsed_synced);
            }

            if (!plain_lrc.empty()) {
                std::istringstream p_stream(plain_lrc);
                std::string p_line;
                while (std::getline(p_stream, p_line)) {
                    while (!p_line.empty() && (p_line.back() == '\r' || p_line.back() == ' ')) p_line.pop_back();
                    if (!p_line.empty()) parsed_plain.push_back(p_line);
                }
            }

            {
                std::lock_guard<std::mutex> lock(s_lyrics_mutex);
                if (req_id != s_lyrics_request_id.load()) return;
                s_lyrics_fetching = false;
                if (!parsed_synced.empty()) {
                    s_synced_lyrics = std::move(parsed_synced);
                    s_lyrics_available = true;
                    s_lyrics_synced = true;
                } else if (!parsed_plain.empty()) {
                    s_plain_lyrics = std::move(parsed_plain);
                    s_lyrics_available = true;
                    s_lyrics_synced = false;
                } else {
                    s_lyrics_available = false;
                    s_lyrics_synced = false;
                }
            }
        }).detach();
    }

    static void InitWinRT() {
        if (s_winrt_initialized) return;
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            s_session_manager = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
            s_winrt_initialized = true;
        } catch (...) {
            s_winrt_initialized = false;
        }
    }

    static void MediaWorkerLoop() {
        try {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
        } catch (...) {}

        while (s_worker_running.load()) {
            try {
                if (!s_session_manager) {
                    try {
                        s_session_manager = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
                    } catch (...) {}
                }

                if (s_session_manager) {
                    auto session = s_session_manager.GetCurrentSession();
                    if (session) {
                        s_session = session;
                        auto playback_info = session.GetPlaybackInfo();
                        bool playing = false;
                        bool repeat = false;
                        if (playback_info) {
                            playing = (playback_info.PlaybackStatus() == winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
                            auto repeat_mode = playback_info.AutoRepeatMode();
                            if (repeat_mode) {
                                repeat = (repeat_mode.Value() != winrt::Windows::Media::MediaPlaybackAutoRepeatMode::None);
                            }
                        }

                        float total_sec = 0.0f;
                        float cur_sec = 0.0f;
                        auto timeline = session.GetTimelineProperties();
                        if (timeline) {
                            total_sec = static_cast<float>(timeline.EndTime().count()) / 10000000.0f;
                            cur_sec = static_cast<float>(timeline.Position().count()) / 10000000.0f;
                            if (playing) {
                                auto last_updated = timeline.LastUpdatedTime();
                                auto now = winrt::clock::now();
                                int64_t diff_100ns = now.time_since_epoch().count() - last_updated.time_since_epoch().count();
                                if (diff_100ns > 0) {
                                    float elapsed = static_cast<float>(diff_100ns) / 10000000.0f;
                                    if (elapsed > 0.0f && elapsed < 30.0f) {
                                        cur_sec += elapsed;
                                    }
                                }
                            }
                        }

                        std::string raw_title, raw_artist, raw_album;
                        auto media_props = session.TryGetMediaPropertiesAsync().get();
                        std::vector<uint8_t> new_pixels;
                        int pw = 0, ph = 0;
                        bool has_pixels = false;

                        if (media_props) {
                            raw_title = winrt::to_string(media_props.Title());
                            raw_artist = winrt::to_string(media_props.Artist());
                            raw_album = winrt::to_string(media_props.AlbumTitle());

                            auto thumb_ref = media_props.Thumbnail();
                            if (thumb_ref) {
                                try {
                                    auto thumb_stream = thumb_ref.OpenReadAsync().get();
                                    if (thumb_stream) {
                                        uint32_t stream_size = static_cast<uint32_t>(thumb_stream.Size());
                                        if (stream_size > 0 && stream_size != s_last_thumb_size) {
                                            s_last_thumb_size = stream_size;
                                            winrt::Windows::Storage::Streams::Buffer buffer(stream_size);
                                            thumb_stream.ReadAsync(buffer, stream_size, winrt::Windows::Storage::Streams::InputStreamOptions::None).get();

                                            int w = 0, h = 0, ch = 0;
                                            unsigned char* px = stbi_load_from_memory(buffer.data(), static_cast<int>(stream_size), &w, &h, &ch, 4);
                                            if (px && w > 0 && h > 0) {
                                                new_pixels.assign(px, px + (w * h * 4));
                                                pw = w;
                                                ph = h;
                                                has_pixels = true;
                                                stbi_image_free(px);
                                            }
                                        }
                                    }
                                } catch (...) {}
                            }
                        }

                        {
                            std::lock_guard<std::mutex> lock(s_staged.mtx);
                            s_staged.title = raw_title;
                            s_staged.artist = raw_artist;
                            s_staged.album = raw_album;
                            s_staged.is_playing = playing;
                            s_staged.has_playback_info = true;
                            s_staged.repeat = repeat;
                            if (total_sec > 0.0f) {
                                s_staged.total_seconds = total_sec;
                                s_staged.current_seconds = cur_sec;
                                s_staged.timeline_updated = true;
                            }
                            if (has_pixels) {
                                s_staged.pixel_data = std::move(new_pixels);
                                s_staged.pixel_w = pw;
                                s_staged.pixel_h = ph;
                                s_staged.new_pixels_ready = true;
                            }
                        }
                    }
                }
            } catch (...) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
        }
    }

    void SpotifyPlayer::Initialize() {
        InitWinRT();

        s_track.title = "Mrs Magic (Strings Version)";
        s_track.artist = "Strawberry Guy";
        s_track.album = "Mrs Magic";
        s_track.is_playing = true;
        s_track.total_seconds = 210.0f;
        s_track.current_seconds = 61.0f;
        s_track.progress = 61.0f / 210.0f;
        s_track.lyrics_available = true;
        s_track.lyrics_synced = true;

        {
            std::lock_guard<std::mutex> lock(s_lyrics_mutex);
            s_synced_lyrics = {
                { 38840,  "Mrs Magic to and fro" },
                { 46000,  "Please give me one last show" },
                { 53260,  "Loosen my mind from within" },
                { 60390,  "Before it starts to wear and thin" },
                { 68870,  "I don't know" },
                { 72490,  "I don't know what I'm doing here" },
                { 83340,  "I don't know" },
                { 86900,  "I don't know what I'm doing here" },
                { 103810, "Mrs Magic radio" },
                { 111290, "Give me one last chance to show" },
                { 118410, "Tell you what lurks deep inside" },
                { 125820, "Deep inside my battered mind" },
                { 134200, "I don't know" },
                { 137830, "I don't know what I'm doing here" },
                { 148850, "I don't know" },
                { 152500, "I don't know what I'm doing here" },
                { 164620, "Leaving me outside" },
                { 169920, "No, I can't get back in" },
                { 173650, "No, I can't get back in" },
                { 179320, "Leaving me outside" },
                { 184660, "No, I can't get back in" },
                { 188200, "No, I can't get back in" },
                { 200890, "Mrs. Magic to and fro" },
                { 206650, "Just let me be myself" }
            };
            s_lyrics_available = true;
            s_lyrics_synced = true;
        }

        s_palette_current = MusicPalette{};
        s_palette_target = s_palette_current;
        s_palette_ready = true;

        s_last_sync_tick = GetTickCount64();
        s_last_known_pos_sec = s_track.current_seconds;
        s_last_winrt_reported_pos = s_track.current_seconds;

        if (!s_worker_running.load()) {
            s_worker_running.store(true);
            s_worker_thread = std::thread(MediaWorkerLoop);
            s_worker_thread.detach();
        }

    }

    void SpotifyPlayer::PollWinRTMedia() {
    }

    void SpotifyPlayer::Update(float delta_time) {
        std::string new_title, new_artist, new_album;
        bool has_new_track = false;
        bool has_new_pixels = false;
        std::vector<uint8_t> pixels_to_upload;
        int pw = 0, ph = 0;

        {
            std::lock_guard<std::mutex> lock(s_staged.mtx);
            if (s_staged.has_playback_info) {
                s_track.is_playing = s_staged.is_playing;
                s_track.repeat = s_staged.repeat;
            }
            if (!s_staged.title.empty() && (s_staged.title != s_track.title || s_staged.artist != s_track.artist)) {
                new_title = s_staged.title;
                new_artist = s_staged.artist;
                new_album = s_staged.album;
                has_new_track = true;
            }
            if (s_staged.timeline_updated) {
                s_staged.timeline_updated = false;
                s_track.total_seconds = s_staged.total_seconds;
                float drift = std::abs(s_track.current_seconds - s_staged.current_seconds);
                if (drift > 0.45f || s_last_sync_tick == 0) {
                    s_last_winrt_reported_pos = s_staged.current_seconds;
                    s_last_known_pos_sec = s_staged.current_seconds;
                    s_last_sync_tick = GetTickCount64();
                    s_track.current_seconds = s_staged.current_seconds;
                }
            }
            if (s_staged.new_pixels_ready) {
                s_staged.new_pixels_ready = false;
                pixels_to_upload = std::move(s_staged.pixel_data);
                pw = s_staged.pixel_w;
                ph = s_staged.pixel_h;
                has_new_pixels = true;
            }
        }

        if (has_new_track) {
            s_track.title = new_title;
            s_track.artist = new_artist.empty() ? "-" : new_artist;
            s_track.album = new_album;
            s_lyrics_scroll_y = 0.0f;
            s_lyrics_scroll_target = 0.0f;
            s_current_lyric_line = -1;
            FetchLyricsAsync(s_track.artist, s_track.title, static_cast<int>(s_track.total_seconds));
        }

        if (has_new_pixels && pw > 0 && ph > 0) {
            s_palette_target = ExtractPalette(pixels_to_upload.data(), pw, ph);
            ID3D11Device* dev = VoidGUI::D3D11_GetDevice();
            if (dev) {
                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = pw;
                desc.Height = ph;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                D3D11_SUBRESOURCE_DATA init_data = {};
                init_data.pSysMem = pixels_to_upload.data();
                init_data.SysMemPitch = pw * 4;

                ID3D11Texture2D* tex = nullptr;
                if (SUCCEEDED(dev->CreateTexture2D(&desc, &init_data, &tex)) && tex) {
                    ID3D11ShaderResourceView* srv = nullptr;
                    if (SUCCEEDED(dev->CreateShaderResourceView(tex, nullptr, &srv))) {
                        if (s_album_art_srv) s_album_art_srv->Release();
                        s_album_art_srv = srv;
                    }
                    tex->Release();
                }
            }
        }

        if (s_track.is_playing && !s_is_seeking) {
            float elapsed_since_sync = static_cast<float>(GetTickCount64() - s_last_sync_tick) / 1000.0f;
            s_track.current_seconds = s_last_known_pos_sec + elapsed_since_sync;
            if (s_track.total_seconds > 0.0f) {
                if (s_track.current_seconds > s_track.total_seconds) {
                    s_track.current_seconds = s_track.total_seconds;
                }
                s_track.progress = std::clamp(s_track.current_seconds / s_track.total_seconds, 0.0f, 1.0f);
            }
            s_motion += delta_time * 0.45f;
        }

        if (s_palette_ready) {
            float blend = 1.0f - std::exp(-4.0f * delta_time);
            s_palette_current.top      = Color::Lerp(s_palette_current.top, s_palette_target.top, blend);
            s_palette_current.bottom   = Color::Lerp(s_palette_current.bottom, s_palette_target.bottom, blend);
            s_palette_current.accentA  = Color::Lerp(s_palette_current.accentA, s_palette_target.accentA, blend);
            s_palette_current.accentB  = Color::Lerp(s_palette_current.accentB, s_palette_target.accentB, blend);
            s_palette_current.accentC  = Color::Lerp(s_palette_current.accentC, s_palette_target.accentC, blend);
            s_palette_current.progress = Color::Lerp(s_palette_current.progress, s_palette_target.progress, blend);
        }

        {
            std::lock_guard<std::mutex> lock(s_lyrics_mutex);
            s_track.lyrics_available = s_lyrics_available;
            s_track.lyrics_synced = s_lyrics_synced;
        }
    }

    void SpotifyPlayer::TogglePlayPause() {
        s_track.is_playing = !s_track.is_playing;
        s_last_known_pos_sec = s_track.current_seconds;
        s_last_sync_tick = GetTickCount64();

        InitWinRT();
        if (s_session) {
            try { s_session.TryTogglePlayPauseAsync(); } catch (...) {}
        } else {
            keybd_event(VK_MEDIA_PLAY_PAUSE, 0, 0, 0);
            keybd_event(VK_MEDIA_PLAY_PAUSE, 0, KEYEVENTF_KEYUP, 0);
        }
    }

    void SpotifyPlayer::SkipNext() {
        s_track.current_seconds = 0.0f;
        s_track.progress = 0.0f;
        s_last_known_pos_sec = 0.0f;
        s_last_sync_tick = GetTickCount64();

        InitWinRT();
        if (s_session) {
            try { s_session.TrySkipNextAsync(); } catch (...) {}
        } else {
            keybd_event(VK_MEDIA_NEXT_TRACK, 0, 0, 0);
            keybd_event(VK_MEDIA_NEXT_TRACK, 0, KEYEVENTF_KEYUP, 0);
        }
    }

    void SpotifyPlayer::SkipPrevious() {
        s_track.current_seconds = 0.0f;
        s_track.progress = 0.0f;
        s_last_known_pos_sec = 0.0f;
        s_last_sync_tick = GetTickCount64();

        InitWinRT();
        if (s_session) {
            try { s_session.TrySkipPreviousAsync(); } catch (...) {}
        } else {
            keybd_event(VK_MEDIA_PREV_TRACK, 0, 0, 0);
            keybd_event(VK_MEDIA_PREV_TRACK, 0, KEYEVENTF_KEYUP, 0);
        }
    }

    void SpotifyPlayer::Seek(float fraction) {
        fraction = std::clamp(fraction, 0.0f, 1.0f);
        s_track.progress = fraction;
        s_track.current_seconds = fraction * s_track.total_seconds;
        s_last_known_pos_sec = s_track.current_seconds;
        s_last_sync_tick = GetTickCount64();

        InitWinRT();
        if (s_session && s_track.total_seconds > 0.0f) {
            try {
                int64_t pos_100ns = static_cast<int64_t>(fraction * s_track.total_seconds * 10000000.0);
                s_session.TryChangePlaybackPositionAsync(pos_100ns);
            } catch (...) {}
        }
    }

    void SpotifyPlayer::SetVolume(float vol) {
        s_track.volume = std::clamp(vol, 0.0f, 1.0f);
    }

    void SpotifyPlayer::ToggleShuffle() {
        s_track.shuffle = !s_track.shuffle;
        InitWinRT();
        if (s_session) {
            try {
                s_session.TryChangeShuffleActiveAsync(s_track.shuffle);
            } catch (...) {}
        }
    }

    void SpotifyPlayer::ToggleRepeat() {
        s_track.repeat = !s_track.repeat;
        InitWinRT();
        if (s_session) {
            try {
                auto mode = s_track.repeat ?
                    winrt::Windows::Media::MediaPlaybackAutoRepeatMode::List :
                    winrt::Windows::Media::MediaPlaybackAutoRepeatMode::None;
                s_session.TryChangeAutoRepeatModeAsync(mode);
            } catch (...) {}
        }
    }

    PlayerViewMode SpotifyPlayer::GetViewMode() {
        return s_view_mode;
    }

    void SpotifyPlayer::SetViewMode(PlayerViewMode mode) {
        if (s_view_mode != mode) {
            s_prev_view_mode = s_view_mode;
            s_view_mode = mode;
            s_mode_transition = true;
        }
    }

    void SpotifyPlayer::ToggleLyrics() {
        if (s_view_mode == PlayerViewMode::Lyrics) {
            SetViewMode(PlayerViewMode::Compact);
        } else {
            SetViewMode(PlayerViewMode::Lyrics);
        }
    }

    void SpotifyPlayer::ToggleArtwork() {
        if (s_view_mode == PlayerViewMode::Artwork) {
            SetViewMode(s_prev_view_mode == PlayerViewMode::Artwork ? PlayerViewMode::Compact : s_prev_view_mode);
        } else {
            SetViewMode(PlayerViewMode::Artwork);
        }
    }

    const MediaTrackInfo& SpotifyPlayer::GetTrackInfo() {
        return s_track;
    }

    bool SpotifyPlayer::HasLyrics() {
        return s_lyrics_available;
    }

    bool SpotifyPlayer::IsLyricsExpanded() {
        return s_view_mode == PlayerViewMode::Lyrics;
    }

    void SpotifyPlayer::SetLyricsExpanded(bool expanded) {
        SetViewMode(expanded ? PlayerViewMode::Lyrics : PlayerViewMode::Compact);
    }

    bool SpotifyPlayer::IsShuffle() {
        return s_track.shuffle;
    }

    bool SpotifyPlayer::IsRepeat() {
        return s_track.repeat;
    }

    void SpotifyPlayer::Render(Context* ctx, Vec2* pos, bool* open) {
        if (!ctx || !pos || (open && !*open)) return;

        if (!s_album_art_srv) {
            ID3D11Device* dev = VoidGUI::D3D11_GetDevice();
            if (dev) {
                const int art_w = 128, art_h = 128;
                std::vector<uint32_t> art_pixels(art_w * art_h);
                for (int y = 0; y < art_h; ++y) {
                    float fy = static_cast<float>(y) / static_cast<float>(art_h);
                    for (int x = 0; x < art_w; ++x) {
                        float fx = static_cast<float>(x) / static_cast<float>(art_w);
                        float r = 0.14f + 0.55f * (1.0f - fy) * (0.7f + 0.3f * fx);
                        float g = 0.10f + 0.22f * (1.0f - fx) * (1.0f - fy);
                        float b = 0.28f + 0.55f * fy * fx;
                        float dx = (fx - 0.5f) * 2.0f;
                        float dy = (fy - 0.5f) * 2.0f;
                        float d = std::sqrt(dx * dx + dy * dy);
                        if (d < 0.30f) {
                            float glow = (1.0f - d / 0.30f);
                            r += glow * 0.35f;
                            g += glow * 0.30f;
                            b += glow * 0.45f;
                        }
                        uint8_t ur = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
                        uint8_t ug = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
                        uint8_t ub = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));
                        art_pixels[y * art_w + x] = (255u << 24) | (ub << 16) | (ug << 8) | ur;
                    }
                }
                s_palette_target = ExtractPalette(reinterpret_cast<const uint8_t*>(art_pixels.data()), art_w, art_h);
                s_palette_current = s_palette_target;
                s_palette_ready = true;

                D3D11_TEXTURE2D_DESC desc = {};
                desc.Width = art_w;
                desc.Height = art_h;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                D3D11_SUBRESOURCE_DATA init_data = {};
                init_data.pSysMem = art_pixels.data();
                init_data.SysMemPitch = art_w * 4;

                ID3D11Texture2D* tex = nullptr;
                if (SUCCEEDED(dev->CreateTexture2D(&desc, &init_data, &tex)) && tex) {
                    ID3D11ShaderResourceView* srv = nullptr;
                    if (SUCCEEDED(dev->CreateShaderResourceView(tex, nullptr, &srv))) {
                        s_album_art_srv = srv;
                    }
                    tex->Release();
                }
            }
        }

        float dt = ctx->GetDeltaTime();

        if (s_view_mode == PlayerViewMode::Lyrics) {
            s_target_size = Vec2(335.0f, 484.0f);
        } else if (s_view_mode == PlayerViewMode::Artwork) {
            s_target_size = Vec2(310.0f, 310.0f);
        } else {
            s_target_size = Vec2(320.0f, 141.0f);
        }

        float blend = 1.0f - std::exp(-22.0f * dt);
        s_current_size.x += (s_target_size.x - s_current_size.x) * blend;
        s_current_size.y += (s_target_size.y - s_current_size.y) * blend;
        if (std::abs(s_current_size.x - s_target_size.x) < 0.4f) s_current_size.x = s_target_size.x;
        if (std::abs(s_current_size.y - s_target_size.y) < 0.4f) s_current_size.y = s_target_size.y;

        Vec2 size = s_current_size;
        uint32_t id = ctx->GetId("##VOID_SPOTIFY_PLAYER");

        static Vec2 s_spot_drag_offset(0.0f, 0.0f);
        static bool s_spot_dragging = false;

        const InputState& input = ctx->GetInput();
        Rect header_drag;
        if (s_view_mode == PlayerViewMode::Artwork) {
            header_drag = Rect(pos->x + 50.0f, pos->y, pos->x + size.x - 50.0f, pos->y + 44.0f);
        } else {
            header_drag = Rect(pos->x + 65.0f, pos->y + 8.0f, pos->x + size.x - 20.0f, pos->y + 55.0f);
        }

        if (ctx->GetActiveId() == 0 && input.mouse_clicked && header_drag.Contains(input.mouse_pos)) {
            ctx->SetActiveId(id);
            s_spot_dragging = true;
            s_spot_drag_offset = input.mouse_pos - *pos;
        }
        if (s_spot_dragging) {
            if (input.mouse_down) {
                *pos = input.mouse_pos - s_spot_drag_offset;
                if (pos->x < 0.0f) pos->x = 0.0f;
                if (pos->y < 0.0f) pos->y = 0.0f;
            } else {
                s_spot_dragging = false;
                if (ctx->GetActiveId() == id) ctx->SetActiveId(0);
            }
        }

        DrawList* dl = ctx->GetDrawList();
        Theme& theme = ctx->GetTheme();
        Vec2 p_min = *pos;
        Vec2 p_max = p_min + size;
        const float rounding = 15.0f;

        dl->AddShadow(p_min, p_max, rounding, 16.0f, Color(0, 0, 0, 180));
        dl->AddGlow(p_min, p_max, rounding, 10.0f, s_palette_current.accentA.WithAlpha(42));
        dl->AddGlow(p_min, p_max, rounding, 18.0f, s_palette_current.accentB.WithAlpha(22));

        Color dark_glass_base = Color(10, 12, 16, 248);
        Color surf_top = Color::Lerp(dark_glass_base, s_palette_current.top, 0.68f);
        Color surf_bot = Color::Lerp(dark_glass_base, s_palette_current.bottom, 0.58f);

        dl->AddRectFilled(p_min, p_max, Color(6, 7, 10, 255), rounding);
        dl->AddRectFilledGradient(p_min, p_max, surf_top, surf_top, surf_bot, surf_bot);

        float sweep = std::fmod(theme.liquid_time * 0.25f, 1.0f);
        float wave_x = p_min.x - size.y * 0.4f + sweep * (size.x + size.y * 0.8f);
        float wave_w = (std::min)(size.x * 0.40f, 130.0f);
        Vec2 w_min((std::max)(p_min.x, wave_x), p_min.y);
        Vec2 w_max((std::min)(p_max.x, wave_x + wave_w), p_min.y + size.y * 0.45f);
        if (w_max.x > w_min.x && w_max.y > w_min.y) {
            Color wave_center = Color(255, 255, 255, 18);
            Color wave_edge = Color(255, 255, 255, 0);
            dl->AddRectFilledGradient(w_min, w_max, wave_edge, wave_center, wave_edge, wave_edge);
        }

        float waveA = std::sin(s_motion * 1.07f);
        float waveB = std::cos(s_motion * 0.83f);
        float waveC = std::sin(s_motion * 0.61f + 1.8f);
        float waveD = std::cos(s_motion * 0.49f + 0.7f);
        float blob_radius = (std::min)(size.x, size.y) * 0.58f;

        DrawAtmosphericBlob(dl,
            Vec2(p_min.x + size.x * (0.16f + waveA * 0.06f), p_min.y + size.y * (0.16f + waveB * 0.05f)),
            blob_radius, s_palette_current.accentA, 0.22f);

        DrawAtmosphericBlob(dl,
            Vec2(p_min.x + size.x * (0.86f - waveB * 0.055f), p_min.y + size.y * (0.82f + waveA * 0.05f)),
            blob_radius * 0.90f, s_palette_current.accentB, 0.20f);

        if (s_view_mode == PlayerViewMode::Lyrics) {
            DrawAtmosphericBlob(dl,
                Vec2(p_min.x + size.x * (0.50f + waveC * 0.085f), p_min.y + size.y * (0.48f + waveD * 0.065f)),
                blob_radius * 0.82f, s_palette_current.accentC, 0.18f);
        }

        if (s_view_mode == PlayerViewMode::Artwork && s_album_art_srv != nullptr) {
            dl->AddImageRounded(s_album_art_srv, p_min, p_max, rounding);
            dl->AddRectFilledGradient(
                Vec2(p_min.x, p_min.y + size.y - 145.0f), p_max,
                Color(6, 8, 14, 0), Color(6, 8, 14, 0),
                Color(6, 8, 14, 245), Color(6, 8, 14, 245)
            );
        }

        Color rim_col = Color::Lerp(Color(255, 255, 255, 45), s_palette_current.accentA, 0.40f).WithAlpha(130);
        dl->AddRect(p_min, p_max, rim_col, rounding, 1.0f);
        if (size.x > rounding * 2.0f) {
            dl->AddLine(Vec2(p_min.x + rounding, p_min.y + 0.5f), Vec2(p_max.x - rounding, p_min.y + 0.5f), Color(255, 255, 255, 95), 1.0f);
        }
        dl->AddRect(p_min + Vec2(1, 1), p_max - Vec2(1, 1), s_palette_current.accentB.WithAlpha(28), rounding - 1.0f, 1.0f);

        std::string title_text = s_track.title.empty() ? "No Media Playing" : s_track.title;
        std::string artist_text = s_track.artist.empty() ? "-" : s_track.artist;
        if (!s_track.album.empty() && s_track.album != s_track.artist) {
            artist_text += " - " + s_track.album;
        }

        if (s_view_mode == PlayerViewMode::Artwork) {
            Vec2 back_center(p_min.x + 25.0f, p_min.y + 25.0f);
            float back_r = 16.0f;
            Rect back_hit(back_center.x - back_r, back_center.y - back_r, back_center.x + back_r, back_center.y + back_r);
            bool back_hov = back_hit.Contains(input.mouse_pos);
            if (back_hov && input.mouse_clicked) {
                ToggleArtwork();
            }
            dl->AddCircleFilled(back_center, back_r, Color(8, 10, 16, back_hov ? 170 : 120), 20);
            dl->AddCircle(back_center, back_r, Color(255, 255, 255, back_hov ? 80 : 30), 20, 1.0f);
            DrawChevronBack(dl, back_center, 7.0f, Color(255, 255, 255, back_hov ? 255 : 220));

            Vec2 aa_center(p_max.x - 25.0f, p_min.y + 25.0f);
            Rect aa_hit(aa_center.x - back_r, aa_center.y - back_r, aa_center.x + back_r, aa_center.y + back_r);
            bool aa_hov = aa_hit.Contains(input.mouse_pos);
            if (aa_hov && input.mouse_clicked) {
                ToggleLyrics();
            }
            dl->AddCircleFilled(aa_center, back_r, Color(8, 10, 16, aa_hov ? 170 : 120), 20);
            dl->AddCircle(aa_center, back_r, Color(255, 255, 255, aa_hov ? 80 : 30), 20, 1.0f);
            Vec2 aa_sz = Font::CalcTextSize("Aa", 0.90f);
            dl->AddText(Vec2(aa_center.x - aa_sz.x * 0.5f, aa_center.y - aa_sz.y * 0.5f),
                        Color(255, 255, 255, aa_hov ? 255 : 220), "Aa", 0.90f);

            float text_x = p_min.x + 14.0f;
            float max_text_w = size.x - 28.0f;

            float info_y = p_min.y + size.y - 128.0f;
            std::string disp_title = EllipsizeText(title_text, 1.10f, max_text_w);
            std::string disp_artist = EllipsizeText(artist_text, 0.90f, max_text_w);

            dl->AddText(Vec2(text_x + 1.0f, info_y + 1.0f), Color(0, 0, 0, 160), disp_title.c_str(), 1.10f);
            dl->AddText(Vec2(text_x, info_y), Color(255, 255, 255, 245), disp_title.c_str(), 1.10f);

            dl->AddText(Vec2(text_x + 1.0f, info_y + 18.0f), Color(0, 0, 0, 140), disp_artist.c_str(), 0.90f);
            dl->AddText(Vec2(text_x, info_y + 17.0f), Color(255, 255, 255, 175), disp_artist.c_str(), 0.90f);

            if (s_track.lyrics_available) {
                std::string active_snippet = "";
                {
                    std::lock_guard<std::mutex> lock(s_lyrics_mutex);
                    if (s_lyrics_synced && !s_synced_lyrics.empty() && s_current_lyric_line >= 0 && s_current_lyric_line < static_cast<int>(s_synced_lyrics.size())) {
                        active_snippet = s_synced_lyrics[s_current_lyric_line].text;
                    }
                }
                if (!active_snippet.empty()) {
                    std::string disp_lyric = EllipsizeText(active_snippet, 0.95f, max_text_w);
                    float lyric_y = info_y + 35.0f;
                    dl->AddText(Vec2(text_x + 1.0f, lyric_y + 1.0f), Color(0, 0, 0, 160), disp_lyric.c_str(), 0.95f);
                    dl->AddText(Vec2(text_x, lyric_y), Color(255, 255, 255, 235), disp_lyric.c_str(), 0.95f);
                }
            }

            float bar_y = p_min.y + size.y - 64.0f;
            Vec2 bar_min(p_min.x + 14.0f, bar_y);
            Vec2 bar_max(p_max.x - 14.0f, bar_y + 3.5f);
            float bar_w = bar_max.x - bar_min.x;

            Rect scrub_hit(bar_min.x, bar_y - 6.0f, bar_max.x, bar_y + 10.0f);
            bool scrub_hovered = scrub_hit.Contains(input.mouse_pos);
            uint32_t scrub_id = id + 0x777;

            if (ctx->GetActiveId() == 0 && input.mouse_clicked && scrub_hovered) {
                ctx->SetActiveId(scrub_id);
                s_is_seeking = true;
            }
            if (s_is_seeking) {
                if (input.mouse_down) {
                    float pct = (input.mouse_pos.x - bar_min.x) / bar_w;
                    s_track.progress = std::clamp(pct, 0.0f, 1.0f);
                    s_track.current_seconds = s_track.progress * s_track.total_seconds;
                } else {
                    Seek(s_track.progress);
                    s_is_seeking = false;
                    if (ctx->GetActiveId() == scrub_id) ctx->SetActiveId(0);
                }
            }

            dl->AddRectFilled(bar_min, bar_max, Color(18, 20, 28, 220), 2.0f);
            dl->AddRect(bar_min, bar_max, Color(255, 255, 255, 30), 2.0f, 1.0f);

            float fill_x = bar_min.x + bar_w * s_track.progress;
            Color scrub_c1 = s_palette_current.accentA;
            Color scrub_c2 = s_palette_current.accentB;
            dl->AddRectFilledGradient(bar_min, Vec2(fill_x, bar_max.y), scrub_c1, scrub_c2, scrub_c2, scrub_c1);
            if (scrub_hovered || s_is_seeking) {
                dl->AddCircleFilled(Vec2(fill_x, bar_y + 1.75f), 3.5f, Color(255, 255, 255, 255));
                dl->AddCircle(Vec2(fill_x, bar_y + 1.75f), 5.5f, s_palette_current.accentA.WithAlpha(160), 16, 1.2f);
            }

            char pb[16], rb[20];
            FormatTimeSeconds(s_track.current_seconds, pb, sizeof(pb));
            char rem[16];
            FormatTimeSeconds((std::max)(0.0f, s_track.total_seconds - s_track.current_seconds), rem, sizeof(rem));
            snprintf(rb, sizeof(rb), "-%s", rem);

            float time_y = bar_y + 6.0f;
            dl->AddText(Vec2(bar_min.x, time_y), Color(255, 255, 255, 145), pb, 0.85f);
            Vec2 rb_sz = Font::CalcTextSize(rb, 0.85f);
            dl->AddText(Vec2(bar_max.x - rb_sz.x, time_y), Color(255, 255, 255, 145), rb, 0.85f);

            Vec2 loss_sz = Font::CalcTextSize("Lossless", 0.85f);
            dl->AddText(Vec2((bar_min.x + bar_max.x - loss_sz.x) * 0.5f, time_y), Color(255, 255, 255, 185), "Lossless", 0.85f);

            float cy = p_min.y + size.y - 24.0f;
            float cx = p_min.x + size.x * 0.5f;
            float spacing = 42.0f;

            Vec2 prev_c(cx - spacing, cy);
            Rect prev_hit(prev_c.x - 14.0f, prev_c.y - 14.0f, prev_c.x + 14.0f, prev_c.y + 14.0f);
            bool prev_hov = prev_hit.Contains(input.mouse_pos);
            if (prev_hov) dl->AddCircleFilled(prev_c, 15.0f, s_palette_current.accentA.WithAlpha(50));
            DrawMediaPrev(dl, prev_c, 9.0f, Color(255, 255, 255, prev_hov ? 255 : 210));
            if (prev_hov && input.mouse_clicked) SkipPrevious();

            Vec2 play_c(cx, cy);
            Rect play_hit(play_c.x - 16.0f, play_c.y - 16.0f, play_c.x + 16.0f, play_c.y + 16.0f);
            bool play_hov = play_hit.Contains(input.mouse_pos);
            dl->AddCircleFilled(play_c, 16.0f, play_hov ? s_palette_current.accentA.WithAlpha(80) : Color(255, 255, 255, 26));
            dl->AddCircle(play_c, 16.0f, Color(255, 255, 255, play_hov ? 80 : 35), 24, 1.0f);
            if (s_track.is_playing) {
                DrawMediaPause(dl, play_c, 9.0f, Color(255, 255, 255, 250));
            } else {
                DrawMediaPlay(dl, play_c, 9.0f, Color(255, 255, 255, 250));
            }
            if (play_hov && input.mouse_clicked) TogglePlayPause();

            Vec2 next_c(cx + spacing, cy);
            Rect next_hit(next_c.x - 14.0f, next_c.y - 14.0f, next_c.x + 14.0f, next_c.y + 14.0f);
            bool next_hov = next_hit.Contains(input.mouse_pos);
            if (next_hov) dl->AddCircleFilled(next_c, 15.0f, s_palette_current.accentA.WithAlpha(50));
            DrawMediaNext(dl, next_c, 9.0f, Color(255, 255, 255, next_hov ? 255 : 210));
            if (next_hov && input.mouse_clicked) SkipNext();

            Vec2 shuf_c(p_min.x + 28.0f, cy);
            Rect shuf_hit(shuf_c.x - 12.0f, shuf_c.y - 12.0f, shuf_c.x + 12.0f, shuf_c.y + 12.0f);
            bool shuf_hov = shuf_hit.Contains(input.mouse_pos);
            if (shuf_hov || s_track.shuffle) dl->AddCircleFilled(shuf_c, 13.0f, s_palette_current.accentA.WithAlpha(s_track.shuffle ? 65 : 30));
            Color shuf_col = s_track.shuffle ? s_palette_current.accentA : Color(255, 255, 255, shuf_hov ? 240 : 135);
            DrawLucideShuffle(dl, shuf_c, 7.5f, shuf_col);
            if (shuf_hov && input.mouse_clicked) ToggleShuffle();

            Vec2 rep_c(p_max.x - 28.0f, cy);
            Rect rep_hit(rep_c.x - 12.0f, rep_c.y - 12.0f, rep_c.x + 12.0f, rep_c.y + 12.0f);
            bool rep_hov = rep_hit.Contains(input.mouse_pos);
            if (rep_hov || s_track.repeat) dl->AddCircleFilled(rep_c, 13.0f, s_palette_current.accentB.WithAlpha(s_track.repeat ? 65 : 30));
            Color rep_col = s_track.repeat ? s_palette_current.accentB : Color(255, 255, 255, rep_hov ? 240 : 135);
            DrawLucideRepeat(dl, rep_c, 7.5f, rep_col);
            if (rep_hov && input.mouse_clicked) ToggleRepeat();

            return;
        }

        float art_sz = (s_view_mode == PlayerViewMode::Compact) ? 48.0f : 44.0f;
        Vec2 art_min(p_min.x + 12.0f, p_min.y + 12.0f);
        Vec2 art_max = art_min + Vec2(art_sz, art_sz);
        float art_round = 10.0f;

        Rect art_hit(art_min, art_max);
        bool art_hovered = art_hit.Contains(input.mouse_pos);
        if (art_hovered && input.mouse_clicked) {
            ToggleArtwork();
        }

        dl->AddShadow(art_min, art_max, art_round, 6.0f, Color(0, 0, 0, 120));
        if (s_album_art_srv != nullptr) {
            dl->AddImageRounded(s_album_art_srv, art_min, art_max, art_round);
            dl->AddRect(art_min, art_max, Color(255, 255, 255, art_hovered ? 120 : 45), art_round, 1.0f);
            if (art_hovered) {
                dl->AddRectFilled(art_min, art_max, Color(0, 0, 0, 60), art_round);
            }
        } else {
            dl->AddRectFilled(art_min, art_max, Color(22, 26, 38, 220), art_round);
            dl->AddRect(art_min, art_max, Color(255, 255, 255, art_hovered ? 100 : 35), art_round, 1.0f);
            Vec2 note_c = (art_min + art_max) * 0.5f;
            dl->AddCircleFilled(note_c, 10.0f, Color(255, 255, 255, 40), 16);
            dl->AddCircleFilled(note_c, 3.5f, Color(255, 255, 255, 120), 10);
        }

        float tx = art_max.x + 10.0f;
        float avail_w = p_max.x - tx - 12.0f;
        std::string disp_title = EllipsizeText(title_text, 1.05f, avail_w);
        std::string disp_artist = EllipsizeText(artist_text, 0.88f, avail_w);

        dl->AddText(Vec2(tx, p_min.y + 14.0f), Color(255, 255, 255, 245), disp_title.c_str(), 1.05f);
        dl->AddText(Vec2(tx, p_min.y + 31.0f), Color(165, 175, 200, 220), disp_artist.c_str(), 0.88f);

        if (s_track.is_playing) {
            float eq_x = p_max.x - 28.0f;
            float eq_base_y = p_min.y + 33.0f;
            for (int b = 0; b < 4; ++b) {
                float wave = 0.5f + 0.5f * std::sin(theme.liquid_time * (7.0f + b * 2.5f) + b * 1.3f);
                float h = 4.0f + 11.0f * wave;
                Color bar_c = (b % 2 == 0) ? s_palette_current.accentA : s_palette_current.accentB;
                dl->AddLine(Vec2(eq_x + b * 4.5f, eq_base_y), Vec2(eq_x + b * 4.5f, eq_base_y - h), bar_c, 2.0f);
            }
        }

        if (s_view_mode == PlayerViewMode::Lyrics) {
            float lyr_top = p_min.y + 68.0f;
            float lyr_bot = p_min.y + size.y - 74.0f;
            float lyr_h = lyr_bot - lyr_top;
            Vec2 lyr_min(p_min.x + 12.0f, lyr_top);
            Vec2 lyr_max(p_max.x - 12.0f, lyr_bot);

            dl->PushClipRect(Rect(p_min.x + 4.0f, lyr_top, p_max.x - 4.0f, lyr_bot));

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
                const char* msg = "Finding synced lyrics...";
                Vec2 msg_sz = Font::CalcTextSize(msg, 0.95f);
                dl->AddText(Vec2(p_min.x + (size.x - msg_sz.x) * 0.5f, lyr_top + (lyr_h - msg_sz.y) * 0.5f),
                            Color(255, 255, 255, 160), msg, 0.95f);
            } else if (!local_avail) {
                const char* msg = "No lyrics found for this track";
                Vec2 msg_sz = Font::CalcTextSize(msg, 0.95f);
                dl->AddText(Vec2(p_min.x + (size.x - msg_sz.x) * 0.5f, lyr_top + (lyr_h - msg_sz.y) * 0.5f),
                            Color(255, 255, 255, 140), msg, 0.95f);
            } else if (local_synced && !local_synced_lines.empty()) {
                uint64_t cur_ms = static_cast<uint64_t>((s_track.current_seconds + 0.12f) * 1000.0f);
                int active_idx = -1;
                for (int i = static_cast<int>(local_synced_lines.size()) - 1; i >= 0; --i) {
                    if (cur_ms >= local_synced_lines[i].time_ms) {
                        active_idx = i;
                        break;
                    }
                }
                s_current_lyric_line = active_idx;

                float line_step = 38.0f;
                float follow_target = static_cast<float>(active_idx >= 0 ? active_idx : 0);

                uint64_t now_ms = GetTickCount64();
                bool lyrics_hovered = Rect(lyr_min.x, lyr_top, lyr_max.x, lyr_bot).Contains(input.mouse_pos);
                if (lyrics_hovered && std::abs(input.mouse_wheel) > 0.01f) {
                    s_manual_scroll = true;
                    s_manual_scroll_until_ms = now_ms + 4000;
                    s_lyrics_scroll_target -= input.mouse_wheel * 2.0f;
                    float max_lines = static_cast<float>(local_synced_lines.size());
                    s_lyrics_scroll_target = std::clamp(s_lyrics_scroll_target, 0.0f, max_lines);
                }

                if (s_manual_scroll && now_ms >= s_manual_scroll_until_ms) {
                    s_manual_scroll = false;
                }

                if (!s_manual_scroll) {
                    s_lyrics_scroll_target = follow_target;
                }

                float scroll_diff = std::abs(s_lyrics_scroll_target - s_lyrics_scroll_y);
                float scroll_speed = scroll_diff > 2.0f ? 26.0f : 16.0f;
                s_lyrics_scroll_y += (s_lyrics_scroll_target - s_lyrics_scroll_y) * (1.0f - std::exp(-scroll_speed * dt));
                if (scroll_diff < 0.015f) s_lyrics_scroll_y = s_lyrics_scroll_target;

                float center_slot_y = lyr_top + (lyr_h * 0.46f) - (line_step * 0.5f);

                for (size_t i = 0; i < local_synced_lines.size(); ++i) {
                    float line_y = center_slot_y + (static_cast<float>(i) - s_lyrics_scroll_y) * line_step;
                    if (line_y + line_step < lyr_top - 20.0f || line_y > lyr_bot + 20.0f) continue;

                    bool is_active = (static_cast<int>(i) == active_idx);
                    int dist = (active_idx >= 0) ? std::abs(static_cast<int>(i) - active_idx) : 3;

                    float scale = 1.0f;
                    int line_count = 1;
                    float avail_text_w = lyr_max.x - lyr_min.x - 22.0f;
                    std::string disp_line = FitLyricText(local_synced_lines[i].text, avail_text_w, is_active, scale, line_count);

                    Vec2 text_sz = Font::CalcTextSize(disp_line.c_str(), scale);
                    float pill_h = std::max(line_step - 4.0f, text_sz.y + 12.0f);
                    float pill_y = line_y + (line_step - pill_h) * 0.5f;

                    Rect line_rect(lyr_min.x, pill_y, lyr_max.x, pill_y + pill_h);
                    bool line_hov = line_rect.Contains(input.mouse_pos);

                    if (line_hov && input.mouse_clicked && s_track.total_seconds > 0.0f) {
                        float seek_pct = static_cast<float>(local_synced_lines[i].time_ms) / (s_track.total_seconds * 1000.0f);
                        Seek(seek_pct);
                        s_manual_scroll = false;
                        s_lyrics_scroll_target = static_cast<float>(i);
                    }

                    if (is_active) {
                        dl->AddGlow(line_rect.min + Vec2(0, 1), line_rect.max - Vec2(0, 1), 7.0f, 3.0f, s_palette_current.accentA.WithAlpha(55));
                        dl->AddRectFilledGradient(line_rect.min + Vec2(0, 2), line_rect.max - Vec2(0, 2),
                            s_palette_current.accentA.WithAlpha(40), s_palette_current.accentB.WithAlpha(55),
                            s_palette_current.accentB.WithAlpha(55), s_palette_current.accentA.WithAlpha(40));
                        dl->AddRect(line_rect.min + Vec2(0, 2), line_rect.max - Vec2(0, 2), s_palette_current.accentA.WithAlpha(170), 6.0f, 1.0f);
                        dl->AddLine(Vec2(line_rect.min.x + 6.0f, line_rect.min.y + 2.5f), Vec2(line_rect.max.x - 6.0f, line_rect.min.y + 2.5f), Color(255, 255, 255, 100), 1.0f);
                    } else if (line_hov) {
                        dl->AddRectFilled(line_rect.min + Vec2(0, 2), line_rect.max - Vec2(0, 2), Color(255, 255, 255, 14), 5.0f);
                    }

                    uint8_t alpha = 255;
                    if (!is_active) {
                        if (dist == 1) alpha = 195;
                        else if (dist == 2) alpha = 135;
                        else alpha = 85;
                        if (line_hov) alpha = 235;
                    }

                    Color text_col = is_active ? Color(255, 255, 255, 255) : Color(225, 230, 245, alpha);
                    float text_y = line_rect.min.y + (pill_h - text_sz.y) * 0.5f;
                    dl->AddText(Vec2(lyr_min.x + (is_active ? 8.0f : 4.0f), text_y), text_col, disp_line.c_str(), scale);
                }

                dl->AddRectFilledGradient(
                    Vec2(p_min.x + 4.0f, lyr_top), Vec2(p_max.x - 4.0f, lyr_top + 28.0f),
                    surf_top, surf_top, surf_top.WithAlpha(0), surf_top.WithAlpha(0)
                );
                dl->AddRectFilledGradient(
                    Vec2(p_min.x + 4.0f, lyr_bot - 28.0f), Vec2(p_max.x - 4.0f, lyr_bot),
                    surf_bot.WithAlpha(0), surf_bot.WithAlpha(0), surf_bot, surf_bot
                );

                if (s_manual_scroll && std::abs(s_lyrics_scroll_y - follow_target) > 0.8f) {
                    Vec2 sync_sz = Font::CalcTextSize("Sync", 0.85f);
                    Vec2 sync_btn_sz(sync_sz.x + 18.0f, 22.0f);
                    Vec2 sync_pos(p_min.x + (size.x - sync_btn_sz.x) * 0.5f, lyr_bot - 28.0f);
                    Rect sync_hit(sync_pos, sync_pos + sync_btn_sz);
                    bool sync_hov = sync_hit.Contains(input.mouse_pos);
                    dl->AddShadow(sync_pos, sync_pos + sync_btn_sz, 11.0f, 6.0f, Color(0, 0, 0, 120));
                    dl->AddRectFilled(sync_pos, sync_pos + sync_btn_sz, Color(16, 18, 24, sync_hov ? 240 : 215), 11.0f);
                    dl->AddRect(sync_pos, sync_pos + sync_btn_sz, s_palette_current.accentA.WithAlpha(sync_hov ? 200 : 120), 11.0f, 1.0f);
                    dl->AddText(Vec2(sync_pos.x + 9.0f, sync_pos.y + 3.0f), Color(255, 255, 255, sync_hov ? 255 : 220), "Sync", 0.85f);
                    if (sync_hov && input.mouse_clicked) {
                        s_manual_scroll = false;
                        s_lyrics_scroll_target = follow_target;
                    }
                }
            }

            dl->PopClipRect();
        }

        float bar_y = (s_view_mode == PlayerViewMode::Compact) ? p_min.y + 78.0f : p_min.y + size.y - 64.0f;
        Vec2 bar_min(p_min.x + 14.0f, bar_y);
        Vec2 bar_max(p_max.x - 14.0f, bar_y + 3.5f);
        float bar_w = bar_max.x - bar_min.x;

        Rect scrub_hit(bar_min.x, bar_y - 6.0f, bar_max.x, bar_y + 10.0f);
        bool scrub_hovered = scrub_hit.Contains(input.mouse_pos);
        uint32_t scrub_id = id + 0x777;

        if (ctx->GetActiveId() == 0 && input.mouse_clicked && scrub_hovered) {
            ctx->SetActiveId(scrub_id);
            s_is_seeking = true;
        }
        if (s_is_seeking) {
            if (input.mouse_down) {
                float pct = (input.mouse_pos.x - bar_min.x) / bar_w;
                s_track.progress = std::clamp(pct, 0.0f, 1.0f);
                s_track.current_seconds = s_track.progress * s_track.total_seconds;
            } else {
                Seek(s_track.progress);
                s_is_seeking = false;
                if (ctx->GetActiveId() == scrub_id) ctx->SetActiveId(0);
            }
        }

        dl->AddRectFilled(bar_min, bar_max, Color(18, 20, 28, 220), 2.0f);
        dl->AddRect(bar_min, bar_max, Color(255, 255, 255, 30), 2.0f, 1.0f);

        float fill_x = bar_min.x + bar_w * s_track.progress;
        Color scrub_c1 = s_palette_current.accentA;
        Color scrub_c2 = s_palette_current.accentB;
        dl->AddRectFilledGradient(bar_min, Vec2(fill_x, bar_max.y), scrub_c1, scrub_c2, scrub_c2, scrub_c1);
        if (scrub_hovered || s_is_seeking) {
            dl->AddCircleFilled(Vec2(fill_x, bar_y + 1.75f), 3.5f, Color(255, 255, 255, 255));
            dl->AddCircle(Vec2(fill_x, bar_y + 1.75f), 5.5f, s_palette_current.accentA.WithAlpha(160), 16, 1.2f);
        }

        char pb[16], rb[20];
        FormatTimeSeconds(s_track.current_seconds, pb, sizeof(pb));
        char rem[16];
        FormatTimeSeconds((std::max)(0.0f, s_track.total_seconds - s_track.current_seconds), rem, sizeof(rem));
        snprintf(rb, sizeof(rb), "-%s", rem);

        float time_y = bar_y + 5.5f;
        dl->AddText(Vec2(bar_min.x, time_y), Color(255, 255, 255, 145), pb, 0.85f);
        Vec2 rb_sz = Font::CalcTextSize(rb, 0.85f);
        dl->AddText(Vec2(bar_max.x - rb_sz.x, time_y), Color(255, 255, 255, 145), rb, 0.85f);

        Vec2 loss_sz = Font::CalcTextSize("Lossless", 0.85f);
        dl->AddText(Vec2((bar_min.x + bar_max.x - loss_sz.x) * 0.5f, time_y), Color(255, 255, 255, 185), "Lossless", 0.85f);

        float cy = (s_view_mode == PlayerViewMode::Compact) ? p_min.y + 118.0f : p_min.y + size.y - 24.0f;
        float cx = p_min.x + size.x * 0.5f;
        float spacing = (s_view_mode == PlayerViewMode::Compact) ? 38.0f : 36.0f;

        Vec2 shuf_c(p_min.x + 22.0f, cy);
        Rect shuf_hit(shuf_c.x - 14.0f, shuf_c.y - 14.0f, shuf_c.x + 14.0f, shuf_c.y + 14.0f);
        bool shuf_hov = shuf_hit.Contains(input.mouse_pos);
        if (shuf_hov || s_track.shuffle) dl->AddCircleFilled(shuf_c, 14.0f, s_palette_current.accentA.WithAlpha(s_track.shuffle ? 65 : 30));
        Color shuf_col = s_track.shuffle ? s_palette_current.accentA : Color(255, 255, 255, shuf_hov ? 240 : 135);
        DrawLucideShuffle(dl, shuf_c, 7.5f, shuf_col);
        if (shuf_hov && input.mouse_clicked) ToggleShuffle();

        Vec2 prev_c(cx - spacing, cy);
        Rect prev_hit(prev_c.x - 16.0f, prev_c.y - 16.0f, prev_c.x + 16.0f, prev_c.y + 16.0f);
        bool prev_hov = prev_hit.Contains(input.mouse_pos);
        if (prev_hov) dl->AddCircleFilled(prev_c, 15.0f, s_palette_current.accentA.WithAlpha(50));
        DrawMediaPrev(dl, prev_c, 9.0f, Color(255, 255, 255, prev_hov ? 255 : 205));
        if (prev_hov && input.mouse_clicked) SkipPrevious();

        Vec2 play_c(cx, cy);
        Rect play_hit(play_c.x - 18.0f, play_c.y - 18.0f, play_c.x + 18.0f, play_c.y + 18.0f);
        bool play_hov = play_hit.Contains(input.mouse_pos);
        dl->AddCircleFilled(play_c, 16.0f, play_hov ? s_palette_current.accentA.WithAlpha(80) : Color(255, 255, 255, 26));
        dl->AddCircle(play_c, 16.0f, Color(255, 255, 255, play_hov ? 80 : 35), 24, 1.0f);
        if (s_track.is_playing) {
            DrawMediaPause(dl, play_c, 9.0f, Color(255, 255, 255, 250));
        } else {
            DrawMediaPlay(dl, play_c, 9.0f, Color(255, 255, 255, 250));
        }
        if (play_hov && input.mouse_clicked) TogglePlayPause();

        Vec2 next_c(cx + spacing, cy);
        Rect next_hit(next_c.x - 16.0f, next_c.y - 16.0f, next_c.x + 16.0f, next_c.y + 16.0f);
        bool next_hov = next_hit.Contains(input.mouse_pos);
        if (next_hov) dl->AddCircleFilled(next_c, 15.0f, s_palette_current.accentA.WithAlpha(50));
        DrawMediaNext(dl, next_c, 9.0f, Color(255, 255, 255, next_hov ? 255 : 205));
        if (next_hov && input.mouse_clicked) SkipNext();

        Vec2 rep_c(cx + spacing * 2.0f, cy);
        Rect rep_hit(rep_c.x - 14.0f, rep_c.y - 14.0f, rep_c.x + 14.0f, rep_c.y + 14.0f);
        bool rep_hov = rep_hit.Contains(input.mouse_pos);
        if (rep_hov || s_track.repeat) dl->AddCircleFilled(rep_c, 14.0f, s_palette_current.accentB.WithAlpha(s_track.repeat ? 65 : 30));
        Color rep_col = s_track.repeat ? s_palette_current.accentB : Color(255, 255, 255, rep_hov ? 240 : 135);
        DrawLucideRepeat(dl, rep_c, 7.5f, rep_col);
        if (rep_hov && input.mouse_clicked) ToggleRepeat();

        Vec2 aa_c(p_max.x - 42.0f, cy);
        Rect aa_hit(aa_c.x - 14.0f, aa_c.y - 14.0f, aa_c.x + 14.0f, aa_c.y + 14.0f);
        bool aa_hov = aa_hit.Contains(input.mouse_pos);
        if (aa_hov) dl->AddCircleFilled(aa_c, 13.0f, s_palette_current.accentA.WithAlpha(50));
        Vec2 aa_sz = Font::CalcTextSize("Aa", 0.85f);
        dl->AddText(Vec2(aa_c.x - aa_sz.x * 0.5f, aa_c.y - aa_sz.y * 0.5f),
                    Color(255, 255, 255, aa_hov ? 255 : 170), "Aa", 0.85f);
        if (aa_hov && input.mouse_clicked) {
            ToggleArtwork();
        }

        Vec2 lyr_btn_c(p_max.x - 18.0f, cy);
        Rect lyr_btn_hit(lyr_btn_c.x - 14.0f, lyr_btn_c.y - 14.0f, lyr_btn_c.x + 14.0f, lyr_btn_c.y + 14.0f);
        bool lyr_btn_hov = lyr_btn_hit.Contains(input.mouse_pos);
        bool is_lyrics_mode = (s_view_mode == PlayerViewMode::Lyrics);
        if (lyr_btn_hov || is_lyrics_mode) {
            dl->AddCircleFilled(lyr_btn_c, 14.0f, is_lyrics_mode ? s_palette_current.accentA.WithAlpha(85) : Color(255, 255, 255, 25));
        }
        Color lyr_btn_col = is_lyrics_mode ? Color(255, 255, 255, 255) : Color(255, 255, 255, lyr_btn_hov ? 240 : 170);
        DrawChatQuoteBubble(dl, lyr_btn_c, 8.0f, lyr_btn_col, is_lyrics_mode);
        if (lyr_btn_hov && input.mouse_clicked) {
            ToggleLyrics();
        }
    }

}
