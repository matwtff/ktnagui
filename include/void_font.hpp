#pragma once

#include "void_types.hpp"
struct ID3D11Device;

#ifdef GetCharWidth
#undef GetCharWidth
#endif

namespace VoidGUI {

    struct GlyphMetric {
        float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float x_offset = 0.0f;
        float y_offset = 0.0f;
        float x_advance = 0.0f;
    };

    class Font {
    public:
        static bool InitializeAtlas(ID3D11Device* device);
        static void ShutdownAtlas();
        static bool HasAtlas();
        static void* GetAtlasSRV();
        static const GlyphMetric* GetGlyph(char c);

        static Vec2 CalcTextSize(const char* text, float scale = 1.0f);
        static float GetCharWidth(char c, float scale = 1.0f);
        static float GetCharAdvance(char c, float scale = 1.0f);
        static float GetLineHeight(float scale = 1.0f);
        static const uint8_t* GetGlyphBitmap(char c, uint8_t& out_w, uint8_t& out_h);
    };

}
