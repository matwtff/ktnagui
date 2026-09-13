#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace VoidGUI {

    struct Vec2 {
        float x = 0.0f;
        float y = 0.0f;

        Vec2() = default;
        Vec2(float _x, float _y) : x(_x), y(_y) {}

        inline Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
        inline Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
        inline Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
        inline Vec2 operator/(float s) const { return Vec2(x / s, y / s); }
        inline Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
        inline Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    };

    struct Rect {
        Vec2 min;
        Vec2 max;

        Rect() = default;
        Rect(Vec2 _min, Vec2 _max) : min(_min), max(_max) {}
        Rect(float x1, float y1, float x2, float y2) : min(x1, y1), max(x2, y2) {}

        inline float Width() const { return max.x - min.x; }
        inline float Height() const { return max.y - min.y; }
        inline bool Contains(const Vec2& p) const {
            return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
        }
        inline Rect Expand(float amt) const {
            return Rect(min.x - amt, min.y - amt, max.x + amt, max.y + amt);
        }
    };

    struct Color {
        uint8_t r = 255;
        uint8_t g = 255;
        uint8_t b = 255;
        uint8_t a = 255;

        Color() = default;
        Color(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 255)
            : r(_r), g(_g), b(_b), a(_a) {}

        static Color FromHex(uint32_t hex) {
            return Color(
                static_cast<uint8_t>((hex >> 16) & 0xFF),
                static_cast<uint8_t>((hex >> 8) & 0xFF),
                static_cast<uint8_t>(hex & 0xFF),
                static_cast<uint8_t>((hex >> 24) & 0xFF ? (hex >> 24) & 0xFF : 255)
            );
        }

        inline uint32_t ToU32() const {
            return (static_cast<uint32_t>(a) << 24) |
                   (static_cast<uint32_t>(b) << 16) |
                   (static_cast<uint32_t>(g) << 8)  |
                   static_cast<uint32_t>(r);
        }

        static Color Lerp(const Color& c1, const Color& c2, float t) {
            t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
            return Color(
                static_cast<uint8_t>(c1.r + (c2.r - c1.r) * t),
                static_cast<uint8_t>(c1.g + (c2.g - c1.g) * t),
                static_cast<uint8_t>(c1.b + (c2.b - c1.b) * t),
                static_cast<uint8_t>(c1.a + (c2.a - c1.a) * t)
            );
        }

        Color WithAlpha(uint8_t alpha) const {
            return Color(r, g, b, alpha);
        }
    };

    struct Vertex {
        float x, y;
        float u, v;
        uint32_t color;
        float flags;
    };

    struct DrawCommand {
        uint32_t index_offset = 0;
        uint32_t index_count = 0;
        Rect clip_rect;
        void* texture_id = nullptr;
    };

    struct InputState {
        Vec2 mouse_pos;
        bool mouse_down = false;
        bool mouse_down_prev = false;
        bool mouse_clicked = false;
        bool mouse_released = false;
        float mouse_wheel = 0.0f;
    };

    struct Theme {
        bool liquid_glass     = true;
        float liquid_time     = 0.0f;

        Color bg_window       = Color(10, 10, 12, 244);
        Color bg_titlebar     = Color(14, 14, 16, 248);
        Color card_bg         = Color(17, 17, 21, 195);
        Color card_header     = Color(21, 21, 25, 210);
        Color border          = Color(255, 255, 255, 16);
        Color border_subtle   = Color(255, 255, 255, 10);
        Color border_bright   = Color(255, 255, 255, 38);

        Color accent          = Color(245, 245, 250, 255);
        Color accent_hover    = Color(255, 255, 255, 255);
        Color accent_dim      = Color(255, 255, 255, 35);
        Color accent_gradient = Color(185, 185, 195, 255);

        Color widget_bg       = Color(22, 22, 26, 220);
        Color widget_hover    = Color(32, 32, 38, 235);
        Color widget_active   = Color(44, 44, 52, 245);

        Color text            = Color(248, 248, 252, 255);
        Color text_muted      = Color(145, 145, 152, 255);
        Color text_dim        = Color(92, 92, 98, 255);
        Color checkmark       = Color(255, 255, 255, 255);

        Color success         = Color(74, 222, 128, 255);
        Color danger          = Color(248, 113, 113, 255);

        float window_rounding = 14.0f;
        float frame_rounding  = 7.0f;
        float card_rounding   = 10.0f;
        float item_spacing    = 9.0f;

        void SetLiquidGlass(bool enable = true) {
            liquid_glass = true;
        }
    };

}

namespace ktna = VoidGUI;
