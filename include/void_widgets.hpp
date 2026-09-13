#pragma once

#include "void_types.hpp"
#include "void_draw.hpp"

namespace VoidGUI {

    class Context;

    class Widgets {
    public:
        static void Text(Context* ctx, const char* fmt, ...);
        static void TextColored(Context* ctx, Color col, const char* fmt, ...);
        static bool Button(Context* ctx, const char* label, Vec2 size = Vec2(0, 0));
        static bool Checkbox(Context* ctx, const char* label, bool* value);
        static bool Toggle(Context* ctx, const char* label, bool* value);
        static bool SliderFloat(Context* ctx, const char* label, float* value, float min, float max, const char* format = "%.2f");
        static bool SliderInt(Context* ctx, const char* label, int* value, int min, int max);
        static bool Combo(Context* ctx, const char* label, int* current_item, const char* const items[], int items_count);
        static bool Keybind(Context* ctx, const char* label, int* key);
        static bool ColorEdit(Context* ctx, const char* label, Color* color);
        static bool TabBar(Context* ctx, const char** tabs, int count, int* selected_tab);
        static void ProgressBar(Context* ctx, float fraction, const char* overlay = nullptr);
        static void Separator(Context* ctx);
        static void Spacing(Context* ctx, float height = 6.0f);
    };

}
