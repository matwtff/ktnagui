#include "../include/void_playerbar.hpp"
#include "../include/void_gui.hpp"
#include <cstdio>
#include <algorithm>

namespace VoidGUI {

    PlayerBarData PlayerBar::s_data;

    PlayerBarData& PlayerBar::GetData() {
        return s_data;
    }

    void PlayerBar::Render(Context* ctx, Vec2* pos, bool* open) {
        if (!ctx || !pos || (open && !*open)) return;

        Vec2 size(300.0f, 68.0f);
        uint32_t id = ctx->GetId("##VOID_PLAYER_BAR");

        Rect bar_hit(*pos, *pos + size);
        const InputState& input = ctx->GetInput();

        static Vec2 s_pb_drag_offset(0.0f, 0.0f);
        static bool s_pb_dragging = false;

        if (ctx->GetActiveId() == 0 && input.mouse_clicked && bar_hit.Contains(input.mouse_pos)) {
            ctx->SetActiveId(id);
            s_pb_dragging = true;
            s_pb_drag_offset = input.mouse_pos - *pos;
        }
        if (s_pb_dragging) {
            if (input.mouse_down) {
                *pos = input.mouse_pos - s_pb_drag_offset;
                if (pos->x < 0.0f) pos->x = 0.0f;
                if (pos->y < 0.0f) pos->y = 0.0f;
            } else {
                if (ctx->GetActiveId() == id) ctx->SetActiveId(0);
                s_pb_dragging = false;
            }
        }

        DrawList* dl = ctx->GetDrawList();
        Theme& theme = ctx->GetTheme();

        Vec2 p_min = *pos;
        Vec2 p_max = p_min + size;

        if (theme.liquid_glass) {
            dl->AddLiquidGlassPanel(p_min, p_max, 12.0f, theme.liquid_time, theme.card_bg, theme.border_bright, s_data.is_enemy ? Color(239, 68, 68, 255) : Color(34, 197, 94, 255));
        } else {
            dl->AddShadow(p_min, p_max, 12.0f, 10.0f, Color(0, 0, 0, 130));
            dl->AddRectFilled(p_min, p_max, theme.card_bg, 12.0f);
            dl->AddRect(p_min, p_max, theme.border_subtle, 12.0f, 1.0f);
        }

        Vec2 av_min = p_min + Vec2(11.0f, 11.0f);
        Vec2 av_max = av_min + Vec2(46.0f, 46.0f);

        dl->AddRectFilled(av_min, av_max, Color(18, 18, 22, 240), 8.0f);
        dl->AddRect(av_min, av_max, Color(42, 42, 50, 180), 8.0f, 1.0f);

        Vec2 av_head = av_min + Vec2(23.0f, 18.0f);
        dl->AddCircleFilled(av_head, 7.0f, Color(110, 110, 120, 255), 16);
        dl->AddCircleFilled(av_min + Vec2(23.0f, 38.0f), 12.0f, Color(110, 110, 120, 255), 16);

        dl->AddCircleFilled(av_max - Vec2(4.0f, 4.0f), 3.5f, s_data.is_enemy ? Color(239, 68, 68, 255) : Color(74, 222, 128, 255));

        float info_x = p_min.x + 68.0f;
        float info_w = size.x - 80.0f;

        dl->AddText(Vec2(info_x, p_min.y + 10.0f), theme.text, s_data.username.c_str(), 1.02f);

        const char* tag_str = s_data.is_enemy ? "ENEMY" : "TEAM";
        Vec2 tag_sz = Font::CalcTextSize(tag_str, 0.80f);
        float tag_pad_x = 7.0f;
        float tag_pad_y = 2.0f;
        Vec2 tag_min(info_x + info_w - tag_sz.x - tag_pad_x * 2.0f, p_min.y + 11.0f);
        Vec2 tag_max(info_x + info_w, tag_min.y + tag_sz.y + tag_pad_y * 2.0f);
        Color tag_bg = s_data.is_enemy ? Color(239, 68, 68, 30) : Color(74, 222, 128, 30);
        Color tag_border = s_data.is_enemy ? Color(239, 68, 68, 110) : Color(74, 222, 128, 110);
        Color tag_col = s_data.is_enemy ? Color(248, 113, 113, 255) : Color(74, 222, 128, 255);
        dl->AddRectFilled(tag_min, tag_max, tag_bg, 4.0f);
        dl->AddRect(tag_min, tag_max, tag_border, 4.0f, 1.0f);
        dl->AddText(tag_min + Vec2(tag_pad_x, tag_pad_y), tag_col, tag_str, 0.80f);

        float hp_y = p_min.y + 30.0f;
        float hp_h = 5.0f;
        Rect hp_rect(info_x, hp_y, info_x + info_w, hp_y + hp_h);

        dl->AddRectFilled(hp_rect.min, hp_rect.max, Color(22, 22, 26, 255), 2.5f);

        float hp_frac = std::clamp(static_cast<float>(s_data.health) / s_data.max_health, 0.0f, 1.0f);
        float hp_w = hp_frac * info_w;

        if (hp_w > 1.0f) {
            Color hp_start = Color::Lerp(Color(239, 68, 68), Color(74, 222, 128), hp_frac);
            dl->AddRectFilled(hp_rect.min, Vec2(hp_rect.min.x + hp_w, hp_rect.max.y), hp_start, 2.5f);
        }

        char dist_buf[32];
        snprintf(dist_buf, sizeof(dist_buf), "%.1fm", s_data.distance);
        dl->AddText(Vec2(info_x, p_min.y + 42.0f), theme.text_muted, s_data.weapon.c_str(), 0.92f);

        Vec2 dist_sz = Font::CalcTextSize(dist_buf, 0.92f);
        dl->AddText(Vec2(info_x + info_w - dist_sz.x, p_min.y + 42.0f), theme.text_dim, dist_buf, 0.92f);
    }

}
