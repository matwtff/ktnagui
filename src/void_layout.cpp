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

        uint32_t title_id = GetId(title);
        uint32_t resize_id = title_id ^ 0xAA55AA55;
        float titlebar_h = 34.0f;
        Rect title_rect(m_current_window.pos, m_current_window.pos + Vec2(m_current_window.size.x, titlebar_h));

        // Window drag handling
        if (m_active_id == 0 && m_input.mouse_clicked && title_rect.Contains(m_input.mouse_pos)) {
            m_current_window.dragging = true;
            m_current_window.drag_offset = m_input.mouse_pos - m_current_window.pos;
            m_active_id = title_id;
        }
        if (m_current_window.dragging && m_active_id == title_id) {
            if (m_input.mouse_down) {
                m_current_window.pos = m_input.mouse_pos - m_current_window.drag_offset;
            } else {
                m_current_window.dragging = false;
                m_active_id = 0;
            }
        }

        // Window resize handling (bottom-right grip)
        float grip_sz = 22.0f;
        Rect grip_rect(m_current_window.pos + m_current_window.size - Vec2(grip_sz, grip_sz),
                       m_current_window.pos + m_current_window.size);

        if (m_active_id == 0 && m_input.mouse_clicked && grip_rect.Contains(m_input.mouse_pos)) {
            m_current_window.resizing = true;
            m_current_window.resize_offset = (m_current_window.pos + m_current_window.size) - m_input.mouse_pos;
            m_active_id = resize_id;
        }
        if (m_current_window.resizing && m_active_id == resize_id) {
            if (m_input.mouse_down) {
                Vec2 new_sz = m_input.mouse_pos + m_current_window.resize_offset - m_current_window.pos;
                if (new_sz.x < m_current_window.min_size.x) new_sz.x = m_current_window.min_size.x;
                if (new_sz.y < m_current_window.min_size.y) new_sz.y = m_current_window.min_size.y;
                m_current_window.size = new_sz;
            } else {
                m_current_window.resizing = false;
                m_active_id = 0;
            }
        }

        Vec2 p = m_current_window.pos;
        Vec2 s = m_current_window.size;

        if (m_theme.liquid_glass) {
            m_draw_list.AddLiquidGlassPanel(p, p + s, m_theme.window_rounding, m_theme.liquid_time, m_theme.bg_window, m_theme.border_bright, m_theme.accent);

            // Frosted Titlebar with top glass reflection
            m_draw_list.AddRectFilled(p, p + Vec2(s.x, titlebar_h), m_theme.bg_titlebar, m_theme.window_rounding);
            m_draw_list.AddRectFilled(p + Vec2(0, titlebar_h - 4.0f), p + Vec2(s.x, titlebar_h), m_theme.bg_titlebar, 0.0f);
            m_draw_list.AddLine(p + Vec2(m_theme.window_rounding, 0.5f), p + Vec2(s.x - m_theme.window_rounding, 0.5f), Color(255, 255, 255, 120), 1.0f);
        } else {
            // Multi-layer Soft Drop Shadow with purple ambient edge glow
            m_draw_list.AddShadow(p, p + s, m_theme.window_rounding, 14.0f, Color(0, 0, 0, 180));
            m_draw_list.AddShadow(p, p + s, m_theme.window_rounding, 8.0f, Color(124, 58, 237, 24));

            // Window Background (Deep Obsidian)
            m_draw_list.AddRectFilled(p, p + s, m_theme.bg_window, m_theme.window_rounding);

            // Titlebar Background
            m_draw_list.AddRectFilled(p, p + Vec2(s.x, titlebar_h), m_theme.bg_titlebar, m_theme.window_rounding);
            m_draw_list.AddRectFilled(p + Vec2(0, titlebar_h - 4.0f), p + Vec2(s.x, titlebar_h), m_theme.bg_titlebar, 0.0f);
        }

        // Sleek Gradient Accent Line under Titlebar
        m_draw_list.AddRectFilledGradient(p + Vec2(0, titlebar_h - 2.0f), p + Vec2(s.x, titlebar_h),
            m_theme.accent, m_theme.accent_gradient, m_theme.accent_gradient, m_theme.accent);

        // Title text & status badge
        m_draw_list.AddCircleFilled(p + Vec2(16.0f, titlebar_h * 0.5f), 3.5f, m_theme.success);
        m_draw_list.AddText(p + Vec2(28.0f, 9.0f), m_theme.text, title, 1.0f);

        // Mode / Version badge pill on the far right of the titlebar
        const char* badge_txt = m_theme.liquid_glass ? "LIQUID GLASS" : "v2.4";
        Vec2 badge_min(p.x + s.x - (m_theme.liquid_glass ? 108.0f : 62.0f), p.y + 7.0f);
        Vec2 badge_max(p.x + s.x - 14.0f, p.y + 25.0f);
        Rect badge_rect(badge_min, badge_max);

        bool badge_hovered = badge_rect.Contains(m_input.mouse_pos);
        if (m_input.mouse_clicked && badge_hovered) {
            m_theme.SetLiquidGlass(!m_theme.liquid_glass);
        }

        if (m_theme.liquid_glass) {
            m_draw_list.AddRectFilledGradient(badge_min, badge_max, Color(56, 189, 248, badge_hovered ? 80 : 50), Color(139, 92, 246, 50), Color(139, 92, 246, 50), Color(56, 189, 248, 50));
            m_draw_list.AddRect(badge_min, badge_max, Color(56, 189, 248, badge_hovered ? 220 : 140), 3.0f, 1.0f);
            m_draw_list.AddText(badge_min + Vec2(8.0f, 2.0f), Color(186, 230, 253), badge_txt, 0.82f);
        } else {
            m_draw_list.AddRectFilled(badge_min, badge_max, Color(124, 58, 237, badge_hovered ? 60 : 30), 3.0f);
            m_draw_list.AddRect(badge_min, badge_max, Color(124, 58, 237, badge_hovered ? 180 : 100), 3.0f, 1.0f);
            m_draw_list.AddText(badge_min + Vec2(10.0f, 2.0f), Color(216, 180, 254), badge_txt, 0.9f);
        }

        // Fine outer border & inner highlight
        if (!m_theme.liquid_glass) {
            m_draw_list.AddRect(p, p + s, m_theme.border, m_theme.window_rounding, 1.0f);
            m_draw_list.AddRect(p + Vec2(1, 1), p + s - Vec2(1, 1), Color(255, 255, 255, 8), m_theme.window_rounding - 1.0f, 1.0f);
        }

        // Setup Content Cursor & Responsive Content Width
        m_current_window.cursor = p + Vec2(16.0f, titlebar_h + 14.0f);
        m_current_window.content_width = s.x - 32.0f;
        m_current_window.columns_count = 1;
        m_current_window.current_column = 0;
        m_in_window = true;

        // Push content scissor clip
        m_draw_list.PushClipRect(Rect(p.x, p.y + titlebar_h, p.x + s.x, p.y + s.y));

        return true;
    }

    void Context::End() {
        if (m_in_window) {
            // Render any active floating popups (e.g. Combos) on top of normal window contents
            PopupManager::RenderActivePopups(this);

            // Draw responsive resize grip hatch marks in bottom-right corner
            Vec2 br = m_current_window.pos + m_current_window.size;
            Color grip_col = m_current_window.resizing ? m_theme.accent : m_theme.border_bright;
            m_draw_list.AddLine(br - Vec2(13, 5), br - Vec2(5, 13), grip_col, 1.5f);
            m_draw_list.AddLine(br - Vec2(9, 5), br - Vec2(5, 9), grip_col, 1.5f);
            m_draw_list.AddLine(br - Vec2(5, 5), br - Vec2(5, 5), grip_col, 1.5f);

            m_draw_list.PopClipRect();
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

        // Auto-sizing: query last frame's actual measured height if fixed_height <= 0
        float card_h = fixed_height;
        if (card_h <= 0.0f) {
            float cached_h = GetCardHeight(card_id);
            card_h = (cached_h > 30.0f) ? cached_h : 80.0f; // Snug initial estimate
        }

        m_current_window.in_card = true;
        m_current_window.current_card_id = card_id;
        m_current_window.card_start_pos = m_current_window.cursor;
        m_current_window.card_width = item_w;
        m_current_window.card_height = card_h;

        Vec2 c_min = m_current_window.card_start_pos;
        Vec2 c_max = c_min + Vec2(item_w, card_h);

        if (m_theme.liquid_glass) {
            // Frosted liquid glass card panel with light refraction
            m_draw_list.AddLiquidGlassPanel(c_min, c_max, m_theme.card_rounding, m_theme.liquid_time + c_min.y * 0.008f, m_theme.card_bg, m_theme.border_bright, m_theme.accent);
        } else {
            // Card background & subtle border
            m_draw_list.AddRectFilled(c_min, c_max, m_theme.card_bg, m_theme.card_rounding);
            m_draw_list.AddRect(c_min, c_max, m_theme.border_subtle, m_theme.card_rounding, 1.0f);
        }

        // Card Header
        if (title && *title) {
            float hdr_h = 26.0f;
            m_draw_list.AddRectFilled(c_min, Vec2(c_max.x, c_min.y + hdr_h), m_theme.card_header, m_theme.card_rounding);
            m_draw_list.AddRectFilled(c_min + Vec2(0, hdr_h - 4.0f), Vec2(c_max.x, c_min.y + hdr_h), m_theme.card_header, 0.0f);
            m_draw_list.AddLine(c_min + Vec2(0, hdr_h), Vec2(c_max.x, c_min.y + hdr_h), m_theme.border_subtle, 1.0f);

            // Left vertical accent bar with liquid gradient
            if (m_theme.liquid_glass) {
                m_draw_list.AddGlow(c_min + Vec2(9.0f, 6.0f), c_min + Vec2(14.0f, hdr_h - 6.0f), 5.0f, 2.0f, Color(56, 189, 248, 130));
                m_draw_list.AddRectFilledGradient(c_min + Vec2(10.0f, 7.0f), c_min + Vec2(13.0f, hdr_h - 7.0f),
                    Color(56, 189, 248, 255), Color(139, 92, 246, 255), Color(139, 92, 246, 255), Color(56, 189, 248, 255));
            } else {
                m_draw_list.AddGlow(c_min + Vec2(9.0f, 6.0f), c_min + Vec2(14.0f, hdr_h - 6.0f), 4.0f, 2.0f, Color(124, 58, 237, 90));
                m_draw_list.AddRectFilled(c_min + Vec2(10.0f, 7.0f), c_min + Vec2(13.0f, hdr_h - 7.0f), Color(139, 92, 246, 255), 1.0f);
            }
            m_draw_list.AddText(c_min + Vec2(20.0f, 6.0f), m_theme.text, title, 0.95f);

            m_current_window.cursor = c_min + Vec2(12.0f, hdr_h + 10.0f);
        } else {
            m_current_window.cursor = c_min + Vec2(12.0f, 10.0f);
        }
    }

    void Context::EndCard() {
        if (m_current_window.in_card) {
            // Compute EXACT height consumed by child widgets + 12px bottom margin
            float measured_h = (m_current_window.cursor.y - m_current_window.card_start_pos.y) + 12.0f;
            measured_h = std::max(measured_h, 38.0f);

            // Save actual measured height for the next frame's auto-sizing
            SetCardHeight(m_current_window.current_card_id, measured_h);

            // Advance cursor past the card cleanly
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

        // Drag handling
        if (m_active_id == 0 && m_input.mouse_clicked && header_rect.Contains(m_input.mouse_pos)) {
            m_active_id = id;
            m_drag_item.id = id;
            m_drag_item.offset = m_input.mouse_pos - *pos;
        }
        if (m_active_id == id && m_drag_item.id == id) {
            if (m_input.mouse_down) {
                *pos = m_input.mouse_pos - m_drag_item.offset;
                if (pos->x < 0.0f) pos->x = 0.0f;
                if (pos->y < 0.0f) pos->y = 0.0f;
            } else {
                m_active_id = 0;
                m_drag_item.id = 0;
            }
        }

        // Soft Shadow & Card Body
        if (m_theme.liquid_glass) {
            m_draw_list.AddLiquidGlassPanel(*pos, *pos + size, m_theme.card_rounding, m_theme.liquid_time, m_theme.card_bg, m_theme.border_bright, m_theme.accent);
            m_draw_list.AddRectFilled(*pos, *pos + Vec2(size.x, header_h), m_theme.card_header, m_theme.card_rounding);
            m_draw_list.AddRectFilled(*pos + Vec2(0, header_h - 3.0f), *pos + Vec2(size.x, header_h), m_theme.card_header, 0.0f);
            m_draw_list.AddRectFilledGradient(*pos + Vec2(0, header_h - 1.5f), *pos + Vec2(size.x, header_h),
                m_theme.accent, m_theme.accent_gradient, m_theme.accent_gradient, m_theme.accent);
        } else {
            m_draw_list.AddShadow(*pos, *pos + size, m_theme.card_rounding, 8.0f, Color(0, 0, 0, 130));
            m_draw_list.AddRectFilled(*pos, *pos + size, m_theme.bg_window, m_theme.card_rounding);

            // Header Bar
            m_draw_list.AddRectFilled(*pos, *pos + Vec2(size.x, header_h), m_theme.bg_titlebar, m_theme.card_rounding);
            m_draw_list.AddRectFilled(*pos + Vec2(0, header_h - 3.0f), *pos + Vec2(size.x, header_h), m_theme.bg_titlebar, 0.0f);
            m_draw_list.AddLine(*pos + Vec2(0, header_h), *pos + Vec2(size.x, header_h), m_theme.accent, 1.5f);
            m_draw_list.AddRect(*pos, *pos + size, m_theme.border, m_theme.card_rounding, 1.0f);
        }

        // Title text & drag handle icon
        m_draw_list.AddText(*pos + Vec2(10.0f, 9.0f), m_theme.text, title, 1.0f);
        m_draw_list.AddLine(*pos + Vec2(size.x - 22.0f, 11.0f), *pos + Vec2(size.x - 10.0f, 11.0f), m_theme.text_dim, 1.5f);
        m_draw_list.AddLine(*pos + Vec2(size.x - 22.0f, 16.0f), *pos + Vec2(size.x - 10.0f, 16.0f), m_theme.text_dim, 1.5f);

        m_draw_list.AddRect(*pos, *pos + size, m_theme.border, m_theme.card_rounding, 1.0f);

        // Setup cursor for child widgets
        m_current_window.cursor = *pos + Vec2(12.0f, header_h + 10.0f);
        m_current_window.content_width = size.x - 24.0f;
        m_current_window.columns_count = 1;
        m_current_window.current_column = 0;

        return true;
    }

    void Context::EndDraggableCard() {
        // Draggable card completed
    }

} // namespace VoidGUI
