#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../include/void_widgets.hpp"
#include "../include/void_gui.hpp"
#include "../include/void_popup.hpp"
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>
#include <cctype>

namespace VoidGUI {

    static const char* GetKeyName(int vk) {
        switch (vk) {
        case 0x01: return "MOUSE1";
        case 0x02: return "MOUSE2";
        case 0x04: return "MOUSE3";
        case 0x05: return "MOUSE4";
        case 0x06: return "MOUSE5";
        case 0x10: case 0xA0: case 0xA1: return "SHIFT";
        case 0x11: case 0xA2: case 0xA3: return "CTRL";
        case 0x12: case 0xA4: case 0xA5: return "ALT";
        case 0x20: return "SPACE";
        case 0x09: return "TAB";
        case 0x0D: return "ENTER";
        case 0x1B: return "ESC";
        case 0x2D: return "INS";
        case 0x2E: return "DEL";
        default: break;
        }
        if (vk >= 'A' && vk <= 'Z') {
            static char s_buf[2] = { 0, 0 };
            s_buf[0] = static_cast<char>(vk);
            return s_buf;
        }
        if (vk >= '0' && vk <= '9') {
            static char s_buf[2] = { 0, 0 };
            s_buf[0] = static_cast<char>(vk);
            return s_buf;
        }
        return "KEY";
    }

    void Widgets::Text(Context* ctx, const char* fmt, ...) {
        if (!ctx || !fmt) return;
        char buf[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        dl->AddText(win.cursor, ctx->GetTheme().text, buf, 1.0f);
        Vec2 sz = Font::CalcTextSize(buf, 1.0f);
        win.cursor.y += sz.y + 4.0f;
    }

    void Widgets::TextColored(Context* ctx, Color col, const char* fmt, ...) {
        if (!ctx || !fmt) return;
        char buf[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        dl->AddText(win.cursor, col, buf, 1.0f);
        Vec2 sz = Font::CalcTextSize(buf, 1.0f);
        win.cursor.y += sz.y + 4.0f;
    }

    bool Widgets::Button(Context* ctx, const char* label, Vec2 size) {
        if (!ctx || !label) return false;
        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        const InputState& input = ctx->GetInput();
        const Theme& theme = ctx->GetTheme();

        float item_w = size.x > 0.0f ? size.x :
            (win.in_card ? (win.card_width - 28.0f) :
            (win.columns_count > 1 ? win.column_width : win.content_width));
        float item_h = size.y > 0.0f ? size.y : 28.0f;

        Vec2 pos = win.cursor;
        Rect rect(pos, pos + Vec2(item_w, item_h));
        uint32_t id = ctx->GetId(label);

        bool hovered = rect.Contains(input.mouse_pos);
        bool active = (hovered && input.mouse_down);
        bool clicked = (hovered && input.mouse_clicked);

        float anim = ctx->GetAnim(id, hovered, 16.0f);

        Color bg = active ? Color(36, 36, 44, 245) :
            Color::Lerp(Color(22, 22, 26, 230), Color(30, 30, 36, 245), anim);
        Color border = Color::Lerp(Color(45, 45, 52, 180), theme.accent, anim);

        dl->AddRectFilled(rect.min, rect.max, bg, theme.frame_rounding);
        dl->AddRect(rect.min, rect.max, border, theme.frame_rounding, 1.0f);

        if (anim > 0.2f) {
            dl->AddGlow(rect.min, rect.max, 5.0f, 1.5f, theme.accent.WithAlpha(static_cast<uint8_t>(anim * 28)));
        }

        Vec2 text_sz = Font::CalcTextSize(label, 0.95f);
        Vec2 text_pos(
            rect.min.x + (item_w - text_sz.x) * 0.5f,
            rect.min.y + (item_h - text_sz.y) * 0.5f
        );
        Color text_col = Color::Lerp(theme.text_muted, theme.text, anim);
        dl->AddText(text_pos, text_col, label, 0.95f);

        win.cursor.y += item_h + theme.item_spacing;
        return clicked;
    }

    bool Widgets::Checkbox(Context* ctx, const char* label, bool* value) {
        if (!ctx || !label || !value) return false;
        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        const InputState& input = ctx->GetInput();
        const Theme& theme = ctx->GetTheme();

        float item_w = win.in_card ? (win.card_width - 28.0f) :
            (win.columns_count > 1 ? win.column_width : win.content_width);
        float item_h = 22.0f;

        Vec2 pos = win.cursor;
        Rect row_rect(pos, pos + Vec2(item_w, item_h));
        uint32_t id = ctx->GetId(label);

        bool hovered = row_rect.Contains(input.mouse_pos);
        bool clicked = false;

        if (hovered && input.mouse_clicked) {
            *value = !*value;
            clicked = true;
        }

        float anim = ctx->GetAnim(id, *value, 18.0f);
        float h_anim = ctx->GetAnim(id + 0x555, hovered, 16.0f);

        Vec2 b_min = Vec2(pos.x, pos.y + 3.0f);
        Vec2 b_max = b_min + Vec2(16.0f, 16.0f);

        Color b_bg = Color::Lerp(Color(22, 22, 26, 255), theme.accent, anim);
        Color b_border = Color::Lerp(Color(48, 48, 56, 255), theme.accent_hover, std::max(anim, h_anim));

        dl->AddRectFilled(b_min, b_max, b_bg, 4.0f);
        dl->AddRect(b_min, b_max, b_border, 4.0f, 1.0f);

        if (anim > 0.05f) {
            Color check_col = Color(16, 16, 20, static_cast<uint8_t>(anim * 255));
            dl->AddLine(b_min + Vec2(3.5f, 8.5f), b_min + Vec2(6.5f, 12.0f), check_col, 1.5f);
            dl->AddLine(b_min + Vec2(6.5f, 12.0f), b_min + Vec2(12.5f, 4.5f), check_col, 1.5f);
        }

        Color text_col = Color::Lerp(theme.text_muted, theme.text, std::max(anim, h_anim));
        dl->AddText(pos + Vec2(24.0f, 4.0f), text_col, label, 0.95f);

        win.cursor.y += item_h + theme.item_spacing;
        return clicked;
    }

    bool Widgets::Toggle(Context* ctx, const char* label, bool* value) {
        if (!ctx || !label || !value) return false;
        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        const InputState& input = ctx->GetInput();
        const Theme& theme = ctx->GetTheme();

        float item_w = win.in_card ? (win.card_width - 28.0f) :
            (win.columns_count > 1 ? win.column_width : win.content_width);
        float item_h = 24.0f;

        Vec2 pos = win.cursor;
        Rect row_rect(pos, pos + Vec2(item_w, item_h));
        uint32_t id = ctx->GetId(label);

        bool hovered = row_rect.Contains(input.mouse_pos);
        bool clicked = false;

        if (hovered && input.mouse_clicked) {
            *value = !*value;
            clicked = true;
        }

        float anim = ctx->GetAnim(id, *value, 18.0f);
        float h_anim = ctx->GetAnim(id + 0x333, hovered, 14.0f);

        float switch_w = 34.0f;
        float switch_h = 18.0f;
        Vec2 s_min(pos.x + item_w - switch_w, pos.y + 3.0f);
        Vec2 s_max = s_min + Vec2(switch_w, switch_h);

        Color track_off = Color(22, 22, 26, 240);
        Color track_bg = Color::Lerp(track_off, theme.accent, anim);
        Color track_border = Color::Lerp(Color(48, 48, 56, 200), theme.accent_hover, std::max(anim, h_anim));

        if (anim > 0.1f) {
            dl->AddGlow(s_min, s_max, 5.0f, 1.5f, theme.accent.WithAlpha(static_cast<uint8_t>(anim * 35)));
        }

        dl->AddRectFilled(s_min, s_max, track_bg, 9.0f);
        dl->AddRect(s_min, s_max, track_border, 9.0f, 1.0f);

        float knob_r = 6.5f;
        float knob_x = s_min.x + 9.0f + anim * (switch_w - 18.0f);
        float knob_y = s_min.y + 9.0f;

        Color knob_col = Color::Lerp(Color(255, 255, 255, 255), Color(16, 16, 20, 255), anim);
        dl->AddCircleFilled(Vec2(knob_x, knob_y + 1.0f), knob_r, Color(0, 0, 0, 75), 16);
        dl->AddCircleFilled(Vec2(knob_x, knob_y), knob_r, knob_col, 16);

        Color text_col = Color::Lerp(theme.text_muted, theme.text, std::max(anim, h_anim));
        dl->AddText(pos + Vec2(0.0f, 4.0f), text_col, label, 0.95f);

        win.cursor.y += item_h + theme.item_spacing;
        return clicked;
    }

    bool Widgets::SliderFloat(Context* ctx, const char* label, float* value, float min, float max, const char* format) {
        if (!ctx || !label || !value) return false;
        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        const InputState& input = ctx->GetInput();
        const Theme& theme = ctx->GetTheme();

        float item_w = win.in_card ? (win.card_width - 28.0f) :
            (win.columns_count > 1 ? win.column_width : win.content_width);
        float item_h = 32.0f;

        Vec2 pos = win.cursor;
        uint32_t id = ctx->GetId(label);

        char val_str[64];
        snprintf(val_str, sizeof(val_str), format ? format : "%.2f", *value);

        float track_y = pos.y + 19.0f;
        float track_h = 4.0f;
        Rect track_rect(pos.x, track_y, pos.x + item_w, track_y + track_h);
        Rect grab_hit_rect(pos.x - 4.0f, track_y - 6.0f, pos.x + item_w + 4.0f, track_y + track_h + 6.0f);

        bool hovered = grab_hit_rect.Contains(input.mouse_pos);
        bool changed = false;

        dl->AddText(pos + Vec2(0, 1.0f), theme.text, label, 0.95f);

        Vec2 val_sz = Font::CalcTextSize(val_str, 0.9f);
        dl->AddText(Vec2(pos.x + item_w - val_sz.x, pos.y + 1.0f), (hovered || ctx->GetActiveId() == id) ? theme.accent_hover : theme.text_muted, val_str, 0.9f);

        if (hovered && input.mouse_clicked) {
            ctx->SetActiveId(id);
        }

        if (ctx->GetActiveId() == id) {
            if (input.mouse_down) {
                float frac = (input.mouse_pos.x - track_rect.min.x) / item_w;
                frac = std::clamp(frac, 0.0f, 1.0f);
                float new_val = min + frac * (max - min);
                if (new_val != *value) {
                    *value = new_val;
                    changed = true;
                }
            } else {
                ctx->SetActiveId(0);
            }
        }

        float cur_frac = std::clamp((*value - min) / (max - min), 0.0f, 1.0f);
        float fill_w = cur_frac * item_w;

        dl->AddRectFilled(track_rect.min, track_rect.max, Color(22, 22, 26, 240), 2.0f);
        dl->AddRect(track_rect.min, track_rect.max, Color(44, 44, 52, 180), 2.0f, 1.0f);

        if (fill_w > 2.0f) {
            dl->AddRectFilledGradient(
                track_rect.min,
                Vec2(track_rect.min.x + fill_w, track_rect.max.y),
                theme.accent, theme.accent_gradient, theme.accent_gradient, theme.accent
            );
        }

        float knob_cx = track_rect.min.x + fill_w;
        float knob_cy = track_y + track_h * 0.5f;
        dl->AddCircleFilled(Vec2(knob_cx, knob_cy + 1.0f), 5.5f, Color(0, 0, 0, 80), 16);
        dl->AddCircleFilled(Vec2(knob_cx, knob_cy), 5.0f, Color(255, 255, 255, 255), 16);
        if (hovered || ctx->GetActiveId() == id) {
            dl->AddCircle(Vec2(knob_cx, knob_cy), 5.0f, theme.accent_hover, 16, 1.0f);
        }

        win.cursor.y += item_h + theme.item_spacing;
        return changed;
    }

    bool Widgets::SliderInt(Context* ctx, const char* label, int* value, int min, int max) {
        if (!value) return false;
        float f_val = static_cast<float>(*value);
        bool res = SliderFloat(ctx, label, &f_val, static_cast<float>(min), static_cast<float>(max), "%.0f");
        if (res) {
            *value = static_cast<int>(std::round(f_val));
        }
        return res;
    }

    bool Widgets::Combo(Context* ctx, const char* label, int* current_item, const char* const items[], int items_count) {
        if (!ctx || !label || !current_item || !items || items_count <= 0) return false;
        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        const InputState& input = ctx->GetInput();
        const Theme& theme = ctx->GetTheme();

        float item_w = win.in_card ? (win.card_width - 28.0f) :
            (win.columns_count > 1 ? win.column_width : win.content_width);

        Vec2 pos = win.cursor;
        uint32_t id = ctx->GetId(label);

        dl->AddText(pos + Vec2(0, 1.0f), theme.text, label, 0.95f);

        float box_y = pos.y + 20.0f;
        float box_h = 26.0f;
        Rect box_rect(pos.x, box_y, pos.x + item_w, box_y + box_h);

        bool hovered = box_rect.Contains(input.mouse_pos);
        bool is_open = PopupManager::IsComboOpen(id);

        if (hovered && input.mouse_clicked) {
            PopupManager::OpenCombo(ctx, id, box_rect, items, items_count, current_item);
        }

        float anim = ctx->GetAnim(id, hovered || is_open, 16.0f);

        Color bg = Color::Lerp(Color(20, 20, 24, 240), Color(28, 28, 34, 250), anim);
        Color border = Color::Lerp(Color(42, 42, 50, 200), theme.accent, anim);

        dl->AddRectFilled(box_rect.min, box_rect.max, bg, theme.frame_rounding);
        dl->AddRect(box_rect.min, box_rect.max, border, theme.frame_rounding, 1.0f);

        const char* preview_text = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "None";
        dl->AddText(box_rect.min + Vec2(10.0f, 5.0f), theme.text, preview_text, 0.95f);

        Vec2 arrow_c(box_rect.max.x - 14.0f, box_y + box_h * 0.5f);
        if (is_open) {
            dl->AddTriangleFilled(arrow_c + Vec2(-3.5f, 2), arrow_c + Vec2(3.5f, 2), arrow_c + Vec2(0, -2.5f), theme.accent);
        } else {
            dl->AddTriangleFilled(arrow_c + Vec2(-3.5f, -2), arrow_c + Vec2(3.5f, -2), arrow_c + Vec2(0, 2.5f), Color::Lerp(theme.text_dim, theme.text, anim));
        }

        win.cursor.y += 50.0f + theme.item_spacing;
        return false;
    }

    bool Widgets::Keybind(Context* ctx, const char* label, int* key) {
        if (!ctx || !label || !key) return false;
        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        const InputState& input = ctx->GetInput();
        const Theme& theme = ctx->GetTheme();

        float item_w = win.in_card ? (win.card_width - 28.0f) :
            (win.columns_count > 1 ? win.column_width : win.content_width);
        float item_h = 24.0f;

        Vec2 pos = win.cursor;
        Rect row_rect(pos, pos + Vec2(item_w, item_h));
        uint32_t id = ctx->GetId(label);

        bool is_listening = (ctx->GetActiveId() == id);
        bool changed = false;

        dl->AddText(pos + Vec2(0.0f, 4.0f), theme.text, label, 0.95f);

        const char* key_text = is_listening ? "..." : GetKeyName(*key);
        char badge_buf[64];
        if (is_listening) {
            snprintf(badge_buf, sizeof(badge_buf), "[ ... ]");
        } else {
            snprintf(badge_buf, sizeof(badge_buf), "[ %s ]", key_text);
        }

        Vec2 b_sz = Font::CalcTextSize(badge_buf, 0.9f);
        float badge_w = std::max(b_sz.x + 14.0f, 55.0f);
        Rect badge_rect(pos.x + item_w - badge_w, pos.y + 2.0f, pos.x + item_w, pos.y + item_h - 2.0f);

        bool hovered = badge_rect.Contains(input.mouse_pos);
        if (hovered && input.mouse_clicked && !is_listening) {
            ctx->SetActiveId(id);
            ctx->SetKeybindId(id);
        }

        if (is_listening) {
            for (int k = 1; k < 255; ++k) {
                if (k == 0x01 && input.mouse_down) continue;
                if (k == 0x1B) {
                    if ((GetAsyncKeyState(k) & 0x8000) != 0) {
                        *key = 0;
                        ctx->SetActiveId(0);
                        ctx->SetKeybindId(0);
                        changed = true;
                        break;
                    }
                } else if ((GetAsyncKeyState(k) & 0x8000) != 0) {
                    *key = k;
                    ctx->SetActiveId(0);
                    ctx->SetKeybindId(0);
                    changed = true;
                    break;
                }
            }
        }

        Color bg = is_listening ? Color(255, 255, 255, 45) : (hovered ? Color(30, 30, 36, 240) : Color(20, 20, 24, 230));
        Color border = is_listening ? theme.accent : (hovered ? theme.accent_dim : Color(45, 45, 52, 180));
        Color text_c = is_listening ? Color(16, 16, 20, 255) : (hovered ? theme.text : theme.text_muted);

        dl->AddRectFilled(badge_rect.min, badge_rect.max, bg, 5.0f);
        dl->AddRect(badge_rect.min, badge_rect.max, border, 5.0f, 1.0f);

        Vec2 txt_pt(
            badge_rect.min.x + (badge_w - b_sz.x) * 0.5f,
            badge_rect.min.y + (badge_rect.Height() - b_sz.y) * 0.5f
        );
        dl->AddText(txt_pt, text_c, badge_buf, 0.9f);

        win.cursor.y += item_h + theme.item_spacing;
        return changed;
    }

    bool Widgets::ColorEdit(Context* ctx, const char* label, Color* color) {
        if (!ctx || !label || !color) return false;
        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        const InputState& input = ctx->GetInput();
        const Theme& theme = ctx->GetTheme();

        float item_w = win.in_card ? (win.card_width - 28.0f) :
            (win.columns_count > 1 ? win.column_width : win.content_width);
        float item_h = 24.0f;

        Vec2 pos = win.cursor;
        uint32_t id = ctx->GetId(label);

        dl->AddText(pos + Vec2(0.0f, 4.0f), theme.text, label, 0.95f);

        float swatch_w = 34.0f;
        float swatch_h = 16.0f;
        Rect swatch_rect(pos.x + item_w - swatch_w, pos.y + 4.0f, pos.x + item_w, pos.y + 4.0f + swatch_h);

        bool hovered = swatch_rect.Contains(input.mouse_pos);
        bool is_open = PopupManager::IsColorPickerOpen(id);

        if (hovered && input.mouse_clicked) {
            PopupManager::OpenColorPicker(ctx, id, swatch_rect, color);
        }

        dl->AddRectFilled(swatch_rect.min, swatch_rect.max, Color(16, 16, 20, 255), 4.0f);
        dl->AddRectFilled(swatch_rect.min, swatch_rect.max, *color, 4.0f);

        Color border_col = is_open ? Color(255, 255, 255, 240) : (hovered ? Color(255, 255, 255, 150) : Color(255, 255, 255, 45));
        dl->AddRect(swatch_rect.min, swatch_rect.max, border_col, 4.0f, 1.0f);

        win.cursor.y += item_h + theme.item_spacing;
        return is_open;
    }

    static void DrawTabIcon(DrawList* dl, Vec2 ic, int tab_idx, const char* name, Color col) {
        std::string s = name ? name : "";
        for (char& c : s) c = static_cast<char>(std::tolower(c));

        if (s.find("aim") != std::string::npos || tab_idx == 0) {
            dl->AddCircle(ic, 4.0f, col, 16, 1.0f);
            dl->AddCircleFilled(ic, 1.0f, col);
            dl->AddLine(ic - Vec2(5.5f, 0), ic - Vec2(3.0f, 0), col, 1.0f);
            dl->AddLine(ic + Vec2(3.0f, 0), ic + Vec2(5.5f, 0), col, 1.0f);
            dl->AddLine(ic - Vec2(0, 5.5f), ic - Vec2(0, 3.0f), col, 1.0f);
            dl->AddLine(ic + Vec2(0, 3.0f), ic + Vec2(0, 5.5f), col, 1.0f);
        } else if (s.find("vis") != std::string::npos || s.find("esp") != std::string::npos || tab_idx == 1) {
            dl->AddLine(ic - Vec2(5.5f, 0), ic - Vec2(2.0f, 2.8f), col, 1.0f);
            dl->AddLine(ic - Vec2(2.0f, 2.8f), ic + Vec2(2.0f, 2.8f), col, 1.0f);
            dl->AddLine(ic + Vec2(2.0f, 2.8f), ic + Vec2(5.5f, 0), col, 1.0f);
            dl->AddLine(ic - Vec2(5.5f, 0), ic - Vec2(2.0f, -2.8f), col, 1.0f);
            dl->AddLine(ic - Vec2(2.0f, -2.8f), ic + Vec2(2.0f, -2.8f), col, 1.0f);
            dl->AddLine(ic + Vec2(2.0f, -2.8f), ic + Vec2(5.5f, 0), col, 1.0f);
            dl->AddCircleFilled(ic, 1.6f, col);
        } else if (s.find("misc") != std::string::npos || s.find("util") != std::string::npos || tab_idx == 2) {
            dl->AddLine(ic + Vec2(-5.0f, -2.5f), ic + Vec2(5.0f, -2.5f), col, 1.0f);
            dl->AddCircleFilled(ic + Vec2(-1.8f, -2.5f), 1.6f, col);
            dl->AddLine(ic + Vec2(-5.0f, 2.5f), ic + Vec2(5.0f, 2.5f), col, 1.0f);
            dl->AddCircleFilled(ic + Vec2(1.8f, 2.5f), 1.6f, col);
        } else {
            dl->AddRect(ic - Vec2(4.5f, 4.5f), ic + Vec2(4.5f, 4.5f), col, 1.5f, 1.0f);
            dl->AddLine(ic + Vec2(-2.2f, -2.0f), ic + Vec2(2.2f, -2.0f), col, 1.0f);
            dl->AddLine(ic + Vec2(-2.2f, 0.5f), ic + Vec2(2.2f, 0.5f), col, 1.0f);
            dl->AddLine(ic + Vec2(-2.2f, 2.8f), ic + Vec2(0.8f, 2.8f), col, 1.0f);
        }
    }

    bool Widgets::TabBar(Context* ctx, const char** tabs, int count, int* selected_tab) {
        if (!ctx || !tabs || count <= 0 || !selected_tab) return false;
        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        const InputState& input = ctx->GetInput();
        const Theme& theme = ctx->GetTheme();

        float bar_w = win.content_width;
        float bar_h = 32.0f;
        Vec2 pos = win.cursor;
        Rect bar_rect(pos, pos + Vec2(bar_w, bar_h));
        const float pill_r = 7.0f;

        if (theme.liquid_glass) {
            dl->AddRectFilled(bar_rect.min, bar_rect.max, Color(17, 18, 25, 200), pill_r);
            dl->AddRect(bar_rect.min, bar_rect.max, Color(255, 255, 255, 14), pill_r, 1.0f);
        } else {
            dl->AddRectFilled(bar_rect.min, bar_rect.max, Color(18, 20, 27, 240), pill_r);
            dl->AddRect(bar_rect.min, bar_rect.max, Color(255, 255, 255, 10), pill_r, 1.0f);
        }

        float tab_w = (bar_w - 4.0f) / count;
        float target_pill_x = pos.x + 2.0f + (*selected_tab) * tab_w;
        uint32_t bar_id = ctx->GetId("##VOID_TAB_BAR");

        float cur_pill_x = ctx->GetFloatState(bar_id, target_pill_x, 22.0f);
        Rect pill_rect(cur_pill_x, pos.y + 2.0f, cur_pill_x + tab_w, pos.y + bar_h - 2.0f);

        dl->AddShadow(pill_rect.min, pill_rect.max, 5.0f, 6.0f, Color(0, 0, 0, 70));
        if (theme.liquid_glass) {
            dl->AddRectFilled(pill_rect.min, pill_rect.max, Color(255, 255, 255, 26), 5.0f);
            dl->AddRect(pill_rect.min, pill_rect.max, Color(255, 255, 255, 75), 5.0f, 1.0f);
            dl->AddLine(pill_rect.min + Vec2(6.0f, 1.0f), Vec2(pill_rect.max.x - 6.0f, pill_rect.min.y + 1.0f), Color(255, 255, 255, 40), 1.0f);
        } else {
            dl->AddRectFilled(pill_rect.min, pill_rect.max, Color(28, 28, 34, 245), 5.0f);
            dl->AddRect(pill_rect.min, pill_rect.max, Color(255, 255, 255, 70), 5.0f, 1.0f);
            dl->AddLine(pill_rect.min + Vec2(6.0f, 1.0f), Vec2(pill_rect.max.x - 6.0f, pill_rect.min.y + 1.0f), Color(255, 255, 255, 30), 1.0f);
        }

        bool changed = false;
        float icon_sz = 12.0f;
        float icon_spacing = 7.0f;

        for (int i = 0; i < count; ++i) {
            Rect t_rect(pos.x + 2.0f + i * tab_w, pos.y + 2.0f, pos.x + 2.0f + (i + 1) * tab_w, pos.y + bar_h - 2.0f);
            bool hovered = t_rect.Contains(input.mouse_pos);

            if (hovered && input.mouse_clicked) {
                if (*selected_tab != i) {
                    *selected_tab = i;
                    changed = true;
                }
            }

            bool active = (*selected_tab == i);
            if (hovered && !active) {
                dl->AddRectFilled(t_rect.min, t_rect.max, Color(255, 255, 255, 8), 5.0f);
            }

            Vec2 text_sz = Font::CalcTextSize(tabs[i], 0.95f);
            float total_w = icon_sz + icon_spacing + text_sz.x;
            float start_x = t_rect.min.x + (tab_w - total_w) * 0.5f;

            Color text_c = active ? Color(255, 255, 255) : (hovered ? Color(220, 220, 225) : Color(135, 135, 140));
            Color icon_c = active ? theme.accent_hover : (hovered ? Color(200, 200, 205) : Color(120, 120, 125));

            Vec2 ic_center(start_x + icon_sz * 0.5f, pos.y + bar_h * 0.5f - 0.5f);
            DrawTabIcon(dl, ic_center, i, tabs[i], icon_c);

            Vec2 text_pos(start_x + icon_sz + icon_spacing, pos.y + (bar_h - text_sz.y) * 0.5f - 0.5f);
            dl->AddText(text_pos, text_c, tabs[i], 0.95f);
        }

        win.cursor.y += bar_h + 12.0f;
        return changed;
    }

    void Widgets::ProgressBar(Context* ctx, float fraction, const char* overlay) {
        if (!ctx) return;
        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();
        const Theme& theme = ctx->GetTheme();

        float item_w = win.in_card ? (win.card_width - 28.0f) :
            (win.columns_count > 1 ? win.column_width : win.content_width);
        float item_h = 18.0f;

        Vec2 pos = win.cursor;
        Rect rect(pos, pos + Vec2(item_w, item_h));

        fraction = std::clamp(fraction, 0.0f, 1.0f);

        dl->AddRectFilled(rect.min, rect.max, Color(18, 18, 22, 240), 4.0f);
        dl->AddRect(rect.min, rect.max, Color(38, 38, 44, 200), 4.0f, 1.0f);

        float fill_w = fraction * item_w;
        if (fill_w > 2.0f) {
            dl->AddRectFilledGradient(
                rect.min,
                Vec2(rect.min.x + fill_w, rect.max.y),
                theme.accent, theme.accent_gradient, theme.accent_gradient, theme.accent
            );
        }

        if (overlay && *overlay) {
            Vec2 txt_sz = Font::CalcTextSize(overlay, 0.82f);
            Vec2 txt_pt(
                rect.min.x + (item_w - txt_sz.x) * 0.5f,
                rect.min.y + (item_h - txt_sz.y) * 0.5f
            );
            dl->AddText(txt_pt, Color(255, 255, 255, 240), overlay, 0.82f);
        }

        win.cursor.y += item_h + theme.item_spacing;
    }

    void Widgets::Separator(Context* ctx) {
        if (!ctx) return;
        WindowState& win = ctx->GetCurrentWindow();
        DrawList* dl = ctx->GetDrawList();

        float item_w = win.in_card ? (win.card_width - 28.0f) :
            (win.columns_count > 1 ? win.column_width : win.content_width);

        Vec2 p1 = win.cursor + Vec2(0, 4.0f);
        Vec2 p2 = p1 + Vec2(item_w, 0);

        dl->AddLine(p1, p2, Color(255, 255, 255, 12), 1.0f);
        win.cursor.y += 10.0f;
    }

    void Widgets::Spacing(Context* ctx, float height) {
        if (!ctx) return;
        ctx->GetCurrentWindow().cursor.y += height;
    }

}
