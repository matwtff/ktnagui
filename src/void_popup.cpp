#include "../include/void_popup.hpp"
#include "../include/void_gui.hpp"

namespace VoidGUI {

    ComboPopupData PopupManager::s_active_combo;

    void PopupManager::OpenCombo(Context* ctx, uint32_t id, Rect trigger_rect, const char* const items[], int items_count, int* current_item) {
        if (s_active_combo.is_open && s_active_combo.id == id) {
            s_active_combo.is_open = false;
            return;
        }

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
        return s_active_combo.is_open && (s_active_combo.id == id);
    }

    void PopupManager::RenderActivePopups(Context* ctx) {
        if (!s_active_combo.is_open || !ctx || !s_active_combo.current_item) return;

        float item_h = 24.0f;
        float total_h = static_cast<float>(s_active_combo.items.size()) * item_h + 8.0f;
        Vec2 pop_min(s_active_combo.trigger_rect.min.x, s_active_combo.trigger_rect.max.y + 3.0f);
        Vec2 pop_max(s_active_combo.trigger_rect.max.x, pop_min.y + total_h);
        Rect pop_rect(pop_min, pop_max);

        DrawList* dl = ctx->GetDrawList();
        const InputState& input = ctx->GetInput();

        // Push unconstrained screen clip rect so popup is never clipped by parent window/card
        dl->PushClipRect(Rect(-99999, -99999, 99999, 99999));

        // Soft drop shadow
        dl->AddShadow(pop_min, pop_max, 6.0f, 10.0f, Color(0, 0, 0, 180));

        // Background & border
        dl->AddRectFilled(pop_min, pop_max, Color(16, 18, 25, 255), 6.0f);
        dl->AddRect(pop_min, pop_max, Color(45, 52, 72, 255), 6.0f, 1.0f);

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
                dl->AddRectFilled(item_r.min, item_r.max, Color(124, 58, 237, 45), 4.0f);
                dl->AddRectFilled(Vec2(item_r.min.x, item_r.min.y + 4.0f), Vec2(item_r.min.x + 3.0f, item_r.max.y - 4.0f), Color(124, 58, 237, 255), 1.0f);
            } else if (is_hovered) {
                dl->AddRectFilled(item_r.min, item_r.max, Color(35, 40, 56, 200), 4.0f);
            }

            Color text_col = is_selected ? Color(255, 255, 255) : (is_hovered ? Color(220, 226, 242) : Color(150, 156, 175));
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
            // Clicked outside both popup and trigger
            if (!pop_rect.Contains(input.mouse_pos) && !s_active_combo.trigger_rect.Contains(input.mouse_pos)) {
                s_active_combo.is_open = false;
            }
        }
    }

} // namespace VoidGUI
