#include "../include/void_popup.hpp"
#include "../include/void_gui.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace VoidGUI {

    ComboPopupData PopupManager::s_active_combo;
    ColorPickerPopupData PopupManager::s_active_picker;

    static void RgbToHsv(Color col, float& out_h, float& out_s, float& out_v) {
        float r = col.r / 255.0f;
        float g = col.g / 255.0f;
        float b = col.b / 255.0f;
        float max_v = (std::max)({r, g, b});
        float min_v = (std::min)({r, g, b});
        float d = max_v - min_v;

        out_v = max_v;
        out_s = (max_v <= 0.0001f) ? 0.0f : (d / max_v);

        if (d > 0.0001f) {
            if (max_v == r) out_h = std::fmod((g - b) / d, 6.0f);
            else if (max_v == g) out_h = ((b - r) / d) + 2.0f;
            else out_h = ((r - g) / d) + 4.0f;
            out_h /= 6.0f;
            if (out_h < 0.0f) out_h += 1.0f;
        }
    }

    static Color HsvToRgb(float h, float s, float v, uint8_t a = 255) {
        h = std::fmod(h, 1.0f);
        if (h < 0.0f) h += 1.0f;
        s = std::clamp(s, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        float c = v * s;
        float x = c * (1.0f - std::abs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
        float m = v - c;

        float r = 0, g = 0, b = 0;
        int sector = static_cast<int>(h * 6.0f);
        switch (sector) {
            case 0: r = c; g = x; b = 0; break;
            case 1: r = x; g = c; b = 0; break;
            case 2: r = 0; g = c; b = x; break;
            case 3: r = 0; g = x; b = c; break;
            case 4: r = x; g = 0; b = c; break;
            default:r = c; g = 0; b = x; break;
        }

        return Color(
            static_cast<uint8_t>(std::clamp((r + m) * 255.0f + 0.5f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp((g + m) * 255.0f + 0.5f, 0.0f, 255.0f)),
            static_cast<uint8_t>(std::clamp((b + m) * 255.0f + 0.5f, 0.0f, 255.0f)),
            a
        );
    }

    void PopupManager::OpenCombo(Context* ctx, uint32_t id, Rect trigger_rect, const char* const items[], int items_count, int* current_item) {
        if (s_active_combo.is_open && s_active_combo.id == id) {
            s_active_combo.is_open = false;
            return;
        }
        s_active_picker.is_open = false;

        s_active_combo.id = id;
        s_active_combo.trigger_rect = trigger_rect;
        s_active_combo.items.clear();
        for (int i = 0; i < items_count; ++i) {
            s_active_combo.items.push_back(items[i]);
        }
        s_active_combo.current_item = current_item;
        s_active_combo.is_open = true;
        s_active_combo.anim_height = 0.0f;
    }

    void PopupManager::CloseCombo() {
        s_active_combo.is_open = false;
    }

    bool PopupManager::IsComboOpen(uint32_t id) {
        return s_active_combo.is_open && s_active_combo.id == id;
    }

    void PopupManager::OpenColorPicker(Context* ctx, uint32_t id, Rect trigger_rect, Color* color) {
        if (s_active_picker.is_open && s_active_picker.id == id) {
            s_active_picker.is_open = false;
            return;
        }
        s_active_combo.is_open = false;

        s_active_picker.id = id;
        s_active_picker.trigger_rect = trigger_rect;
        s_active_picker.target_color = color;
        s_active_picker.is_open = true;
        s_active_picker.active_drag = 0;

        if (color) {
            float h = 0.0f, s = 1.0f, v = 1.0f;
            RgbToHsv(*color, h, s, v);
            if (s > 0.01f) {
                s_active_picker.h = h;
            }
            s_active_picker.s = s;
            s_active_picker.v = v;
        }
    }

    void PopupManager::CloseColorPicker() {
        s_active_picker.is_open = false;
        s_active_picker.active_drag = 0;
    }

    bool PopupManager::IsColorPickerOpen(uint32_t id) {
        return s_active_picker.is_open && (s_active_picker.id == id);
    }

    void PopupManager::RenderActivePopups(Context* ctx) {
        if (!ctx) return;
        DrawList* dl = ctx->GetDrawList();
        const InputState& input = ctx->GetInput();
        const Theme& theme = ctx->GetTheme();

        if (s_active_combo.is_open && s_active_combo.current_item) {
            float item_h = 24.0f;
            float total_h = static_cast<float>(s_active_combo.items.size()) * item_h + 8.0f;
            Vec2 pop_min(s_active_combo.trigger_rect.min.x, s_active_combo.trigger_rect.max.y + 3.0f);
            Vec2 pop_max(s_active_combo.trigger_rect.max.x, pop_min.y + total_h);
            Rect pop_rect(pop_min, pop_max);

            dl->PushClipRect(Rect(-99999, -99999, 99999, 99999), false);

            dl->AddShadow(pop_min, pop_max, 6.0f, 10.0f, Color(0, 0, 0, 180));
            dl->AddRectFilled(pop_min, pop_max, Color(16, 16, 20, 255), 6.0f);
            dl->AddRect(pop_min, pop_max, Color(255, 255, 255, 30), 6.0f, 1.0f);

            bool item_clicked = false;
            for (size_t i = 0; i < s_active_combo.items.size(); ++i) {
                Rect item_r(
                    pop_min.x + 4.0f,
                    pop_min.y + 4.0f + i * item_h,
                    pop_max.x - 4.0f,
                    pop_min.y + 4.0f + (i + 1) * item_h
                );

                bool is_selected = (*s_active_combo.current_item == static_cast<int>(i));
                bool is_hovered = item_r.Contains(input.mouse_pos);

                if (is_selected) {
                    dl->AddRectFilled(item_r.min, item_r.max, Color(255, 255, 255, 30), 4.0f);
                    dl->AddRectFilled(Vec2(item_r.min.x, item_r.min.y + 4.0f), Vec2(item_r.min.x + 3.0f, item_r.max.y - 4.0f), Color(255, 255, 255, 255), 1.0f);
                } else if (is_hovered) {
                    dl->AddRectFilled(item_r.min, item_r.max, Color(30, 30, 36, 200), 4.0f);
                }

                Color text_col = is_selected ? Color(255, 255, 255) : (is_hovered ? Color(220, 220, 225) : Color(145, 145, 150));
                dl->AddText(Vec2(item_r.min.x + 10.0f, item_r.min.y + 6.0f), text_col, s_active_combo.items[i].c_str(), 1.0f);

                if (input.mouse_clicked && is_hovered) {
                    *s_active_combo.current_item = static_cast<int>(i);
                    item_clicked = true;
                }
            }

            dl->PopClipRect();

            if (item_clicked) {
                s_active_combo.is_open = false;
            } else if (input.mouse_clicked) {
                if (!pop_rect.Contains(input.mouse_pos) && !s_active_combo.trigger_rect.Contains(input.mouse_pos)) {
                    s_active_combo.is_open = false;
                }
            }
        }

        if (s_active_picker.is_open && s_active_picker.target_color) {
            float pop_w = 186.0f;
            float pop_h = 184.0f;

            float pop_x = s_active_picker.trigger_rect.max.x - pop_w;
            float pop_y = s_active_picker.trigger_rect.max.y + 4.0f;
            pop_x = std::max(pop_x, 12.0f);
            if (pop_y + pop_h > 710.0f) {
                pop_y = s_active_picker.trigger_rect.min.y - pop_h - 4.0f;
            }
            pop_y = std::max(pop_y, 12.0f);

            Vec2 pop_min(pop_x, pop_y);
            Vec2 pop_max(pop_x + pop_w, pop_y + pop_h);
            Rect pop_rect(pop_min, pop_max);

            dl->PushClipRect(Rect(-99999, -99999, 99999, 99999), false);

            dl->AddShadow(pop_min, pop_max, 8.0f, 16.0f, Color(0, 0, 0, 200));
            dl->AddRectFilled(pop_min, pop_max, Color(14, 14, 18, 252), 8.0f);
            dl->AddRect(pop_min, pop_max, Color(255, 255, 255, 32), 8.0f, 1.0f);
            dl->AddLine(Vec2(pop_min.x + 8.0f, pop_min.y + 0.5f), Vec2(pop_max.x - 8.0f, pop_min.y + 0.5f), Color(255, 255, 255, 75), 1.0f);

            float y_cursor = pop_min.y + 9.0f;
            Vec2 chip_min(pop_min.x + 10.0f, y_cursor);
            Vec2 chip_max(chip_min.x + 16.0f, y_cursor + 16.0f);
            dl->AddRectFilled(chip_min, chip_max, *s_active_picker.target_color, 3.0f);
            dl->AddRect(chip_min, chip_max, Color(255, 255, 255, 60), 3.0f, 1.0f);

            char hex_buf[32];
            snprintf(hex_buf, sizeof(hex_buf), "#%02X%02X%02X",
                     s_active_picker.target_color->r, s_active_picker.target_color->g, s_active_picker.target_color->b);
            dl->AddText(Vec2(chip_max.x + 8.0f, y_cursor + 1.0f), Color(245, 245, 250), hex_buf, 0.85f);

            char a_buf[16];
            int a_pct = static_cast<int>((s_active_picker.target_color->a / 255.0f) * 100.0f + 0.5f);
            snprintf(a_buf, sizeof(a_buf), "%d%%", a_pct);
            Vec2 a_sz = Font::CalcTextSize(a_buf, 0.82f);
            dl->AddText(Vec2(pop_max.x - 10.0f - a_sz.x, y_cursor + 1.0f), Color(145, 145, 152), a_buf, 0.82f);

            y_cursor += 24.0f;
            Vec2 sv_min(pop_min.x + 10.0f, y_cursor);
            Vec2 sv_max(pop_max.x - 10.0f, y_cursor + 78.0f);
            float sv_w = sv_max.x - sv_min.x;
            float sv_h = sv_max.y - sv_min.y;
            Rect sv_rect(sv_min, sv_max);

            if (ctx->GetActiveId() == 0 && input.mouse_clicked && sv_rect.Contains(input.mouse_pos)) {
                s_active_picker.active_drag = 1;
                ctx->SetActiveId(s_active_picker.id + 0x10);
            }
            if (s_active_picker.active_drag == 1) {
                if (input.mouse_down) {
                    float s = std::clamp((input.mouse_pos.x - sv_min.x) / sv_w, 0.0f, 1.0f);
                    float v = std::clamp(1.0f - (input.mouse_pos.y - sv_min.y) / sv_h, 0.0f, 1.0f);
                    s_active_picker.s = s;
                    s_active_picker.v = v;
                    *s_active_picker.target_color = HsvToRgb(s_active_picker.h, s, v, s_active_picker.target_color->a);
                } else {
                    s_active_picker.active_drag = 0;
                    if (ctx->GetActiveId() == s_active_picker.id + 0x10) ctx->SetActiveId(0);
                }
            }

            Color pure_hue = HsvToRgb(s_active_picker.h, 1.0f, 1.0f);
            dl->AddRectFilledGradient(sv_min, sv_max, Color(255, 255, 255), pure_hue, Color(0, 0, 0), Color(0, 0, 0));
            dl->AddRect(sv_min, sv_max, Color(255, 255, 255, 30), 3.0f, 1.0f);

            float marker_x = sv_min.x + s_active_picker.s * sv_w;
            float marker_y = sv_min.y + (1.0f - s_active_picker.v) * sv_h;
            Vec2 marker_c(marker_x, marker_y);
            dl->AddCircle(marker_c, 4.5f, Color(0, 0, 0, 180), 16, 2.0f);
            dl->AddCircle(marker_c, 4.0f, Color(255, 255, 255, 255), 16, 1.2f);

            y_cursor += 84.0f;
            Vec2 hue_min(pop_min.x + 10.0f, y_cursor);
            Vec2 hue_max(pop_max.x - 10.0f, y_cursor + 9.0f);
            float hue_w = hue_max.x - hue_min.x;
            Rect hue_rect(hue_min, hue_max);

            if (ctx->GetActiveId() == 0 && input.mouse_clicked && hue_rect.Contains(input.mouse_pos)) {
                s_active_picker.active_drag = 2;
                ctx->SetActiveId(s_active_picker.id + 0x20);
            }
            if (s_active_picker.active_drag == 2) {
                if (input.mouse_down) {
                    float h = std::clamp((input.mouse_pos.x - hue_min.x) / hue_w, 0.0f, 0.999f);
                    s_active_picker.h = h;
                    *s_active_picker.target_color = HsvToRgb(h, s_active_picker.s, s_active_picker.v, s_active_picker.target_color->a);
                } else {
                    s_active_picker.active_drag = 0;
                    if (ctx->GetActiveId() == s_active_picker.id + 0x20) ctx->SetActiveId(0);
                }
            }

            static const Color k_hue_stops[7] = {
                Color(255, 0, 0), Color(255, 255, 0), Color(0, 255, 0),
                Color(0, 255, 255), Color(0, 0, 255), Color(255, 0, 255), Color(255, 0, 0)
            };
            float seg_w = hue_w / 6.0f;
            for (int seg = 0; seg < 6; ++seg) {
                Vec2 p1(hue_min.x + seg * seg_w, hue_min.y);
                Vec2 p2(hue_min.x + (seg + 1) * seg_w, hue_max.y);
                dl->AddRectFilledGradient(p1, p2, k_hue_stops[seg], k_hue_stops[seg + 1], k_hue_stops[seg + 1], k_hue_stops[seg]);
            }
            dl->AddRect(hue_min, hue_max, Color(255, 255, 255, 30), 2.0f, 1.0f);

            float hx = hue_min.x + s_active_picker.h * hue_w;
            dl->AddRectFilled(Vec2(hx - 1.5f, hue_min.y - 1.5f), Vec2(hx + 1.5f, hue_max.y + 1.5f), Color(255, 255, 255), 1.0f);
            dl->AddRect(Vec2(hx - 2.0f, hue_min.y - 2.0f), Vec2(hx + 2.0f, hue_max.y + 2.0f), Color(0, 0, 0, 160), 1.0f, 1.0f);

            y_cursor += 15.0f;
            Vec2 al_min(pop_min.x + 10.0f, y_cursor);
            Vec2 al_max(pop_max.x - 10.0f, y_cursor + 9.0f);
            float al_w = al_max.x - al_min.x;
            Rect al_rect(al_min, al_max);

            if (ctx->GetActiveId() == 0 && input.mouse_clicked && al_rect.Contains(input.mouse_pos)) {
                s_active_picker.active_drag = 3;
                ctx->SetActiveId(s_active_picker.id + 0x30);
            }
            if (s_active_picker.active_drag == 3) {
                if (input.mouse_down) {
                    float a_frac = std::clamp((input.mouse_pos.x - al_min.x) / al_w, 0.0f, 1.0f);
                    s_active_picker.target_color->a = static_cast<uint8_t>(a_frac * 255.0f + 0.5f);
                } else {
                    s_active_picker.active_drag = 0;
                    if (ctx->GetActiveId() == s_active_picker.id + 0x30) ctx->SetActiveId(0);
                }
            }

            Color base_rgb(s_active_picker.target_color->r, s_active_picker.target_color->g, s_active_picker.target_color->b, 0);
            Color full_rgb(s_active_picker.target_color->r, s_active_picker.target_color->g, s_active_picker.target_color->b, 255);
            dl->AddRectFilled(al_min, al_max, Color(22, 22, 26), 2.0f);
            dl->AddRectFilledGradient(al_min, al_max, base_rgb, full_rgb, full_rgb, base_rgb);
            dl->AddRect(al_min, al_max, Color(255, 255, 255, 30), 2.0f, 1.0f);

            float ax = al_min.x + (s_active_picker.target_color->a / 255.0f) * al_w;
            dl->AddRectFilled(Vec2(ax - 1.5f, al_min.y - 1.5f), Vec2(ax + 1.5f, al_max.y + 1.5f), Color(255, 255, 255), 1.0f);
            dl->AddRect(Vec2(ax - 2.0f, al_min.y - 2.0f), Vec2(ax + 2.0f, al_max.y + 2.0f), Color(0, 0, 0, 160), 1.0f, 1.0f);

            y_cursor += 15.0f;
            static const Color k_quick_presets[8] = {
                Color(255, 255, 255, 255),
                Color(56, 189, 248, 255),
                Color(168, 85, 247, 255),
                Color(244, 114, 182, 255),
                Color(74, 222, 128, 255),
                Color(250, 204, 21, 255),
                Color(248, 113, 113, 255),
                Color(148, 163, 184, 255)
            };
            float chip_sz = 14.0f;
            float chip_spacing = (hue_w - 8 * chip_sz) / 7.0f;
            for (int p = 0; p < 8; ++p) {
                Vec2 cp_min(al_min.x + p * (chip_sz + chip_spacing), y_cursor);
                Vec2 cp_max(cp_min.x + chip_sz, cp_min.y + chip_sz);
                Rect cp_rect(cp_min, cp_max);
                bool cp_hov = cp_rect.Contains(input.mouse_pos);
                dl->AddRectFilled(cp_min, cp_max, k_quick_presets[p], 3.0f);
                dl->AddRect(cp_min, cp_max, cp_hov ? Color(255, 255, 255, 240) : Color(255, 255, 255, 40), 3.0f, 1.0f);
                if (cp_hov && input.mouse_clicked) {
                    uint8_t old_a = s_active_picker.target_color->a;
                    *s_active_picker.target_color = k_quick_presets[p].WithAlpha(old_a);
                    RgbToHsv(*s_active_picker.target_color, s_active_picker.h, s_active_picker.s, s_active_picker.v);
                }
            }

            dl->PopClipRect();

            if (input.mouse_clicked && s_active_picker.active_drag == 0) {
                if (!pop_rect.Contains(input.mouse_pos) && !s_active_picker.trigger_rect.Contains(input.mouse_pos)) {
                    s_active_picker.is_open = false;
                }
            }
        }
    }

}
