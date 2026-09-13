#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef GetCharWidth
#undef GetCharWidth
#endif

#include <d3d11.h>
#include "../include/void_font.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace VoidGUI {

    // 5x8 ASCII bitmap fallback
    static const uint8_t s_font_glyphs[95][5] = {
        {0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
        {0x00, 0x00, 0x5F, 0x00, 0x00}, // '!'
        {0x00, 0x07, 0x00, 0x07, 0x00}, // '"'
        {0x14, 0x7F, 0x14, 0x7F, 0x14}, // '#'
        {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // '$'
        {0x23, 0x13, 0x08, 0x64, 0x62}, // '%'
        {0x36, 0x49, 0x55, 0x22, 0x50}, // '&'
        {0x00, 0x05, 0x03, 0x00, 0x00}, // '''
        {0x00, 0x1C, 0x22, 0x41, 0x00}, // '('
        {0x00, 0x41, 0x22, 0x1C, 0x00}, // ')'
        {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // '*'
        {0x08, 0x08, 0x3E, 0x08, 0x08}, // '+'
        {0x00, 0x50, 0x30, 0x00, 0x00}, // ','
        {0x08, 0x08, 0x08, 0x08, 0x08}, // '-'
        {0x00, 0x60, 0x60, 0x00, 0x00}, // '.'
        {0x20, 0x10, 0x08, 0x04, 0x02}, // '/'
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0'
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
        {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // '3'
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
        {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // '6'
        {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
        {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9'
        {0x00, 0x36, 0x36, 0x00, 0x00}, // ':'
        {0x00, 0x56, 0x36, 0x00, 0x00}, // ';'
        {0x00, 0x08, 0x14, 0x22, 0x41}, // '<'
        {0x14, 0x14, 0x14, 0x14, 0x14}, // '='
        {0x41, 0x22, 0x14, 0x08, 0x00}, // '>'
        {0x02, 0x01, 0x51, 0x09, 0x06}, // '?'
        {0x32, 0x49, 0x79, 0x41, 0x3E}, // '@'
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 'A'
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // 'B'
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // 'C'
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 'D'
        {0x7F, 0x49, 0x49, 0x49, 0x41}, // 'E'
        {0x7F, 0x09, 0x09, 0x01, 0x01}, // 'F'
        {0x3E, 0x41, 0x41, 0x51, 0x32}, // 'G'
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 'H'
        {0x00, 0x41, 0x7F, 0x41, 0x00}, // 'I'
        {0x20, 0x40, 0x41, 0x3F, 0x01}, // 'J'
        {0x7F, 0x08, 0x14, 0x22, 0x41}, // 'K'
        {0x7F, 0x40, 0x40, 0x40, 0x40}, // 'L'
        {0x7F, 0x02, 0x04, 0x02, 0x7F}, // 'M'
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 'N'
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 'O'
        {0x7F, 0x09, 0x09, 0x09, 0x06}, // 'P'
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 'Q'
        {0x7F, 0x09, 0x19, 0x29, 0x46}, // 'R'
        {0x46, 0x49, 0x49, 0x49, 0x31}, // 'S'
        {0x01, 0x01, 0x7F, 0x01, 0x01}, // 'T'
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 'U'
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 'V'
        {0x7F, 0x20, 0x18, 0x20, 0x7F}, // 'W'
        {0x63, 0x14, 0x08, 0x14, 0x63}, // 'X'
        {0x03, 0x04, 0x78, 0x04, 0x03}, // 'Y'
        {0x61, 0x51, 0x49, 0x45, 0x43}, // 'Z'
        {0x00, 0x7F, 0x41, 0x41, 0x00}, // '['
        {0x02, 0x04, 0x08, 0x10, 0x20}, // '\'
        {0x00, 0x41, 0x41, 0x7F, 0x00}, // ']'
        {0x04, 0x02, 0x01, 0x02, 0x04}, // '^'
        {0x40, 0x40, 0x40, 0x40, 0x40}, // '_'
        {0x00, 0x01, 0x02, 0x04, 0x00}, // '`'
        {0x20, 0x54, 0x54, 0x54, 0x78}, // 'a'
        {0x7F, 0x48, 0x44, 0x44, 0x38}, // 'b'
        {0x38, 0x44, 0x44, 0x44, 0x20}, // 'c'
        {0x38, 0x44, 0x44, 0x48, 0x7F}, // 'd'
        {0x38, 0x54, 0x54, 0x54, 0x18}, // 'e'
        {0x08, 0x7E, 0x09, 0x01, 0x02}, // 'f'
        {0x08, 0x14, 0x54, 0x54, 0x3C}, // 'g'
        {0x7F, 0x08, 0x04, 0x04, 0x78}, // 'h'
        {0x00, 0x44, 0x7D, 0x40, 0x00}, // 'i'
        {0x20, 0x40, 0x44, 0x3D, 0x00}, // 'j'
        {0x7F, 0x10, 0x28, 0x44, 0x00}, // 'k'
        {0x00, 0x41, 0x7F, 0x40, 0x00}, // 'l'
        {0x7C, 0x04, 0x18, 0x04, 0x78}, // 'm'
        {0x7C, 0x08, 0x04, 0x04, 0x78}, // 'n'
        {0x38, 0x44, 0x44, 0x44, 0x38}, // 'o'
        {0x7C, 0x14, 0x14, 0x14, 0x08}, // 'p'
        {0x08, 0x14, 0x14, 0x18, 0x7C}, // 'q'
        {0x7C, 0x08, 0x04, 0x04, 0x08}, // 'r'
        {0x48, 0x54, 0x54, 0x54, 0x20}, // 's'
        {0x04, 0x3F, 0x44, 0x40, 0x20}, // 't'
        {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 'u'
        {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 'v'
        {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 'w'
        {0x44, 0x28, 0x10, 0x28, 0x44}, // 'x'
        {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 'y'
        {0x44, 0x64, 0x54, 0x4C, 0x44}, // 'z'
        {0x00, 0x08, 0x36, 0x41, 0x00}, // '{'
        {0x00, 0x00, 0x7F, 0x00, 0x00}, // '|'
        {0x00, 0x41, 0x36, 0x08, 0x00}, // '}'
        {0x08, 0x08, 0x2A, 0x1C, 0x08}  // '~'
    };

    // Anti-aliased Segoe UI atlas
    static GlyphMetric s_atlas_glyphs[128];
    static ID3D11ShaderResourceView* s_font_atlas_srv = nullptr;
    static bool s_has_atlas = false;
    static float s_atlas_base_line_h = 16.0f;

    bool Font::HasAtlas() {
        return s_has_atlas && s_font_atlas_srv != nullptr;
    }

    void* Font::GetAtlasSRV() {
        return s_font_atlas_srv;
    }

    const GlyphMetric* Font::GetGlyph(char c) {
        if (c < 32 || c > 126) c = '?';
        return &s_atlas_glyphs[static_cast<uint8_t>(c)];
    }

    bool Font::InitializeAtlas(ID3D11Device* device) {
        if (!device) return false;
        if (s_font_atlas_srv) return true;

        const int atlas_w = 512;
        const int atlas_h = 512;

        HDC hdc = CreateCompatibleDC(NULL);
        if (!hdc) return false;

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = atlas_w;
        bmi.bmiHeader.biHeight = -atlas_h; // Top-down DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        uint32_t* pixels = nullptr;
        HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);
        if (!hbm || !pixels) {
            DeleteDC(hdc);
            return false;
        }
        HGDIOBJ old_bmp = SelectObject(hdc, hbm);

        // High quality Segoe UI with crisp ClearType / anti-aliased rendering
        HFONT hFont = CreateFontA(
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI"
        );
        if (!hFont) {
            hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        }
        HGDIOBJ old_font = SelectObject(hdc, hFont);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        memset(pixels, 0, atlas_w * atlas_h * sizeof(uint32_t));

        int cur_x = 4;
        int cur_y = 4;
        int max_row_h = 24;

        TEXTMETRICA tm;
        GetTextMetricsA(hdc, &tm);
        s_atlas_base_line_h = static_cast<float>(tm.tmHeight + tm.tmExternalLeading);
        if (s_atlas_base_line_h < 15.0f) s_atlas_base_line_h = 16.0f;

        const int pad_x = 2;
        const int pad_y = 2;

        for (int c = 32; c < 127; ++c) {
            char ch = static_cast<char>(c);
            SIZE sz;
            GetTextExtentPoint32A(hdc, &ch, 1, &sz);
            int glyph_cell_w = sz.cx + pad_x * 2;
            int glyph_cell_h = sz.cy + pad_y * 2;
            if (glyph_cell_h > max_row_h) max_row_h = glyph_cell_h;

            if (cur_x + glyph_cell_w + 4 >= atlas_w) {
                cur_x = 4;
                cur_y += max_row_h + 4;
            }

            if (c > 32) {
                TextOutA(hdc, cur_x + pad_x, cur_y + pad_y, &ch, 1);
            }

            GlyphMetric& m = s_atlas_glyphs[c];
            m.u0 = static_cast<float>(cur_x) / static_cast<float>(atlas_w);
            m.v0 = static_cast<float>(cur_y) / static_cast<float>(atlas_h);
            m.u1 = static_cast<float>(cur_x + glyph_cell_w) / static_cast<float>(atlas_w);
            m.v1 = static_cast<float>(cur_y + glyph_cell_h) / static_cast<float>(atlas_h);
            m.width = static_cast<float>(glyph_cell_w);
            m.height = static_cast<float>(glyph_cell_h);
            m.x_offset = -static_cast<float>(pad_x);
            m.y_offset = -static_cast<float>(pad_y);
            m.x_advance = static_cast<float>(sz.cx);

            cur_x += glyph_cell_w + 4;
        }

        SelectObject(hdc, old_font);
        DeleteObject(hFont);

        // Convert GDI RGB anti-aliased font glyphs to pure white RGB + alpha channel
        for (int i = 0; i < atlas_w * atlas_h; ++i) {
            uint32_t val = pixels[i];
            uint8_t r = (val >> 16) & 0xFF;
            uint8_t g = (val >> 8) & 0xFF;
            uint8_t b = val & 0xFF;
            uint8_t alpha = std::max({ r, g, b });
            pixels[i] = (static_cast<uint32_t>(alpha) << 24) | 0x00FFFFFF;
        }

        // Upload to Direct3D 11 Texture2D
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = atlas_w;
        desc.Height = atlas_h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sub = {};
        sub.pSysMem = pixels;
        sub.SysMemPitch = atlas_w * sizeof(uint32_t);

        ID3D11Texture2D* pTex = nullptr;
        HRESULT hr = device->CreateTexture2D(&desc, &sub, &pTex);

        // Safe to release GDI bitmap and DC now that pixels have been copied to GPU
        SelectObject(hdc, old_bmp);
        DeleteObject(hbm);
        DeleteDC(hdc);

        if (FAILED(hr) || !pTex) {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(pTex, &srvDesc, &s_font_atlas_srv);
        pTex->Release();

        s_has_atlas = SUCCEEDED(hr);
        return s_has_atlas;
    }

    void Font::ShutdownAtlas() {
        if (s_font_atlas_srv) {
            s_font_atlas_srv->Release();
            s_font_atlas_srv = nullptr;
        }
        s_has_atlas = false;
    }

    float Font::GetCharWidth(char c, float scale) {
        if (s_has_atlas) {
            if (c < 32 || c > 126) c = '?';
            return s_atlas_glyphs[static_cast<uint8_t>(c)].x_advance * scale;
        }
        if (c < 32 || c > 126) c = '?';
        switch (c) {
        case ' ': return 4.0f * scale;
        case '!': case '|': case '\'': case '`': return 3.0f * scale;
        case 'i': case 'l': case ':': case ';': case '.': case ',': return 4.0f * scale;
        case 'I': case '[': case ']': case '(': case ')': return 5.0f * scale;
        case 'm': case 'w': case 'M': case 'W': return 7.5f * scale;
        default: return 6.0f * scale;
        }
    }

    float Font::GetCharAdvance(char c, float scale) {
        return GetCharWidth(c, scale);
    }

    float Font::GetLineHeight(float scale) {
        if (s_has_atlas) {
            return s_atlas_base_line_h * scale;
        }
        return 12.0f * scale;
    }

    const uint8_t* Font::GetGlyphBitmap(char c, uint8_t& out_w, uint8_t& out_h) {
        if (c < 32 || c > 126) c = '?';
        out_w = 5;
        out_h = 8;
        return s_font_glyphs[c - 32];
    }

    Vec2 Font::CalcTextSize(const char* text, float scale) {
        if (!text) return Vec2(0, 0);

        float max_w = 0.0f;
        float cur_w = 0.0f;
        float h = GetLineHeight(scale);

        while (*text) {
            char ch = *text++;
            if (ch == '\n') {
                max_w = std::max(max_w, cur_w);
                cur_w = 0.0f;
                h += GetLineHeight(scale);
                continue;
            }
            cur_w += GetCharWidth(ch, scale);
        }
        max_w = std::max(max_w, cur_w);
        return Vec2(max_w, h);
    }

} // namespace VoidGUI
