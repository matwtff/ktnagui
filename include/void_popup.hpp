#pragma once

#include "void_types.hpp"
#include "void_draw.hpp"
#include <string>
#include <vector>

namespace VoidGUI {

    class Context;

    struct ComboPopupData {
        uint32_t id = 0;
        Rect trigger_rect;
        std::vector<std::string> items;
        int* current_item = nullptr;
        bool is_open = false;
        float anim_height = 0.0f;
    };

    struct ColorPickerPopupData {
        uint32_t id = 0;
        Rect trigger_rect;
        Color* target_color = nullptr;
        bool is_open = false;
        float h = 0.0f;
        float s = 1.0f;
        float v = 1.0f;
        int active_drag = 0;
    };

    class PopupManager {
    public:
        static void OpenCombo(Context* ctx, uint32_t id, Rect trigger_rect, const char* const items[], int items_count, int* current_item);
        static void CloseCombo();
        static bool IsComboOpen(uint32_t id);

        static void OpenColorPicker(Context* ctx, uint32_t id, Rect trigger_rect, Color* color);
        static void CloseColorPicker();
        static bool IsColorPickerOpen(uint32_t id);

        static void RenderActivePopups(Context* ctx);

    private:
        static ComboPopupData s_active_combo;
        static ColorPickerPopupData s_active_picker;
    };

}
