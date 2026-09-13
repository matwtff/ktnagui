#include "../include/void_layout.hpp"
#include "../include/void_gui.hpp"
#include "../include/void_popup.hpp"
#include <algorithm>

namespace VoidGUI {

bool Context::Begin(const char* title, Vec2 default_pos, Vec2 default_size) {
        if (m_current_window.title.empty()) {
            m_current_window.title = title;
            m_current_window.pos = default_pos;
            m_current_window.size = default_size;
        }

        Vec2 p = m_current_window.pos;
        Vec2 s = m_current_window.size;

        uint32_t title_id = GetId(title);
        uint32_t resize_id = title_id ^ 0xAA55AA55;
        float titlebar_h = 34.0f;
        Rect title_rect(p, p + Vec2(s.x, titlebar_h));

        if (m_active_id == 0 && m_input.mouse_clicked && title_rect.Contains(m_input.mouse_pos)) {
            m_current_window.dragging = true;
            m_current_window.drag_offset = m_input.mouse_pos - m_current_window.pos;
            m_active_id = title_id;
        }
        if (m_current_window.dragging) {
            if (m_input.mouse_down) {
                m_current_window.pos = m_input.mouse_pos - m_current_window.drag_offset;
                p = m_current_window.pos;
            } else {
                m_current_window.dragging = false;
                if (m_active_id == title_id) m_active_id = 0;
            }
        }

        float grip_sz = 22.0f;
        Rect grip_rect(p + s - Vec2(grip_sz, grip_sz), p + s);

        if (m_active_id == 0 && m_input.mouse_clicked && grip_rect.Contains(m_input.mouse_pos)) {
            m_current_window.resizing = true;
            m_current_window.resize_offset = (p + s) - m_input.mouse_pos;
            m_active_id = resize_id;
        }
        if (m_current_window.resizing) {
            if (m_input.mouse_down) {
                Vec2 new_sz = m_input.mouse_pos + m_current_window.resize_offset - p;
                if (new_sz.x < m_current_window.min_size.x) new_sz.x = m_current_window.min_size.x;
                if (new_sz.y < m_current_window.min_size.y) new_sz.y = m_current_window.min_size.y;
                m_current_window.size = new_sz;
                s = m_current_window.size;
            } else {
                m_current_window.resizing = false;
                if (m_active_id == resize_id) m_active_id = 0;
            }
        }

        m_draw_list.AddLiquidGlassPanel(p, p + s, m_theme.window_rounding, m_theme.liquid_time, m_theme.bg_window, m_theme.border_bright, m_theme.accent);

        m_draw_list.AddRectFilled(p, p + Vec2(s.x, titlebar_h), m_theme.bg_titlebar, m_theme.window_rounding);
        m_draw_list.AddRectFilled(p + Vec2(0, titlebar_h - 6.0f), p + Vec2(s.x, titlebar_h), m_theme.bg_titlebar, 0.0f);
        m_draw_list.AddLine(p + Vec2(m_theme.window_rounding, 0.5f), p + Vec2(s.x - m_theme.window_rounding, 0.5f), Color(255, 255, 255, 60), 1.0f);

        m_draw_list.AddLine(p + Vec2(14.0f, titlebar_h), p + Vec2(s.x - 14.0f, titlebar_h), Color(255, 255, 255, 12), 1.0f);

        m_draw_list.AddCircleFilled(p + Vec2(16.0f, titlebar_h * 0.5f), 3.0f, m_theme.success);
        m_draw_list.AddText(p + Vec2(27.0f, 9.0f), m_theme.text, title, 1.0f);

        m_current_window.cursor = p + Vec2(16.0f, titlebar_h + 14.0f);
        m_current_window.content_width = s.x - 32.0f;
        m_current_window.columns_count = 1;
        m_current_window.current_column = 0;
        m_in_window = true;

        m_draw_list.PushClipRect(Rect(p.x, p.y + titlebar_h, p.x + s.x, p.y + s.y));

        return true;
    }

    void Context::End() {
        if (m_in_window) {
            Vec2 br = m_current_window.pos + m_current_window.size;
            Color grip_col = m_current_window.resizing ? m_theme.accent : m_theme.border_bright;
            m_draw_list.AddLine(br - Vec2(13, 5), br - Vec2(5, 13), grip_col, 1.5f);
            m_draw_list.AddLine(br - Vec2(9, 5), br - Vec2(5, 9), grip_col, 1.5f);
            m_draw_list.AddLine(br - Vec2(5, 5), br - Vec2(5, 5), grip_col, 1.5f);

            m_draw_list.PopClipRect();

            PopupManager::RenderActivePopups(this);

            m_in_window = false;
        }
    }

void Context::Columns(int count) {
        if (count < 1) count = 1;
        m_current_window.columns_count = count;
        m_current_window.current_column = 0;
        m_current_window.column_start_cursor = m_current_window.cursor;
        m_current_window.column_width = (m_current_window.content_width - (count - 1) * m_theme.item_spacing) / count;
        m_current_window.column_max_y = m_current_window.cursor.y;
    }

    void Context::NextColumn() {
        if (m_current_window.columns_count <= 1) return;
        m_current_window.column_max_y = std::max(m_current_window.column_max_y, m_current_window.cursor.y);
        m_current_window.current_column = (m_current_window.current_column + 1) % m_current_window.columns_count;
        m_current_window.cursor = m_current_window.column_start_cursor +
            Vec2(m_current_window.current_column * (m_current_window.column_width + m_theme.item_spacing), 0);
    }

    void Context::EndColumns() {
        if (m_current_window.columns_count > 1) {
            m_current_window.column_max_y = std::max(m_current_window.column_max_y, m_current_window.cursor.y);
            m_current_window.cursor.x = m_current_window.column_start_cursor.x;
            m_current_window.cursor.y = m_current_window.column_max_y + m_theme.item_spacing;
            m_current_window.columns_count = 1;
            m_current_window.current_column = 0;
        }
    }

    void Context::BeginCard(const char* title, float fixed_height) {
        uint32_t card_id = GetId(title);
        float item_w = (m_current_window.columns_count > 1) ? m_current_window.column_width : m_current_window.content_width;

        float card_h = fixed_height;
        if (card_h <= 0.0f) {
            float cached_h = GetCardHeight(card_id);
            card_h = (cached_h > 30.0f) ? cached_h : 80.0f;
        }

        m_current_window.in_card = true;
        m_current_window.current_card_id = card_id;
        m_current_window.card_start_pos = m_current_window.cursor;
        m_current_window.card_width = item_w;
        m_current_window.card_height = card_h;

        Vec2 c_min = m_current_window.card_start_pos;
        Vec2 c_max = c_min + Vec2(item_w, card_h);

        if (m_theme.liquid_glass) {
            m_draw_list.AddLiquidGlassPanel(c_min, c_max, m_theme.card_rounding, m_theme.liquid_time + c_min.y * 0.008f, m_theme.card_bg, m_theme.border_bright, m_theme.accent);
        } else {
            m_draw_list.AddShadow(c_min, c_max, m_theme.card_rounding, 8.0f, Color(0, 0, 0, 90));
            m_draw_list.AddRectFilled(c_min, c_max, m_theme.card_bg, m_theme.card_rounding);
            m_draw_list.AddRect(c_min, c_max, m_theme.border_subtle, m_theme.card_rounding, 1.0f);
        }

        if (title && *title) {
            m_draw_list.AddText(c_min + Vec2(14.0f, 10.0f), m_theme.text, title, 1.0f);
            m_draw_list.AddLine(c_min + Vec2(14.0f, 30.0f), Vec2(c_max.x - 14.0f, 30.0f), Color(255, 255, 255, 8), 1.0f);

            m_current_window.cursor = c_min + Vec2(14.0f, 40.0f);
        } else {
            m_current_window.cursor = c_min + Vec2(14.0f, 14.0f);
        }
    }

    void Context::EndCard() {
        if (m_current_window.in_card) {
            float measured_h = (m_current_window.cursor.y - m_current_window.card_start_pos.y) + 8.0f;
            measured_h = std::max(measured_h, 38.0f);

            SetCardHeight(m_current_window.current_card_id, measured_h);

            m_current_window.cursor = Vec2(
                m_current_window.card_start_pos.x,
                m_current_window.card_start_pos.y + m_current_window.card_height + m_theme.item_spacing
            );
            m_current_window.in_card = false;
        }
    }

bool Context::BeginDraggableCard(const char* title, Vec2* pos, Vec2 size, bool* open) {
        if (open && !(*open)) return false;

        uint32_t id = GetId(title);
        float header_h = 32.0f;
        Rect header_rect(*pos, *pos + Vec2(size.x, header_h));

        if (m_active_id == 0 && m_input.mouse_clicked && header_rect.Contains(m_input.mouse_pos)) {
            m_active_id = id;
            m_drag_item.id = id;
            m_drag_item.offset = m_input.mouse_pos - *pos;
        }
        if (m_drag_item.id == id) {
            if (m_input.mouse_down) {
                *pos = m_input.mouse_pos - m_drag_item.offset;
                if (pos->x < 0.0f) pos->x = 0.0f;
                if (pos->y < 0.0f) pos->y = 0.0f;
            } else {
                if (m_active_id == id) m_active_id = 0;
                m_drag_item.id = 0;
            }
        }

        if (m_theme.liquid_glass) {
            m_draw_list.AddLiquidGlassPanel(*pos, *pos + size, m_theme.card_rounding, m_theme.liquid_time, m_theme.card_bg, m_theme.border_bright, m_theme.accent);
            m_draw_list.AddLine(*pos + Vec2(12.0f, header_h), *pos + Vec2(size.x - 12.0f, header_h), Color(255, 255, 255, 14), 1.0f);
        } else {
            m_draw_list.AddShadow(*pos, *pos + size, m_theme.card_rounding, 14.0f, Color(0, 0, 0, 130));
            m_draw_list.AddRectFilled(*pos, *pos + size, m_theme.bg_window, m_theme.card_rounding);
            m_draw_list.AddRect(*pos, *pos + size, m_theme.border_subtle, m_theme.card_rounding, 1.0f);
            m_draw_list.AddLine(*pos + Vec2(12.0f, header_h), *pos + Vec2(size.x - 12.0f, header_h), Color(255, 255, 255, 12), 1.0f);
        }

        m_draw_list.AddText(*pos + Vec2(14.0f, 8.0f), m_theme.text, title, 1.0f);
        m_draw_list.AddLine(*pos + Vec2(size.x - 22.0f, 13.0f), *pos + Vec2(size.x - 13.0f, 13.0f), Color(255, 255, 255, 35), 1.5f);
        m_draw_list.AddLine(*pos + Vec2(size.x - 22.0f, 17.0f), *pos + Vec2(size.x - 13.0f, 17.0f), Color(255, 255, 255, 35), 1.5f);

        m_current_window.cursor = *pos + Vec2(14.0f, header_h + 10.0f);
        m_current_window.content_width = size.x - 28.0f;
        m_current_window.columns_count = 1;
        m_current_window.current_column = 0;

        return true;
    }

    void Context::EndDraggableCard() {
    }

}
