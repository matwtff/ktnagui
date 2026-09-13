#pragma once

#include "void_types.hpp"
#include "void_draw.hpp"

namespace VoidGUI {

    struct WindowState {
        std::string title;
        Vec2 pos;
        Vec2 size;
        Vec2 min_size = Vec2(340.0f, 240.0f);
        Vec2 cursor;
        bool dragging = false;
        Vec2 drag_offset;
        bool resizing = false;
        Vec2 resize_offset;
        float content_width = 0.0f;

        // Multi-Column State
        int columns_count = 1;
        int current_column = 0;
        Vec2 column_start_cursor;
        float column_width = 0.0f;
        float column_max_y = 0.0f;

        // Card Container State
        bool in_card = false;
        uint32_t current_card_id = 0;
        Vec2 card_start_pos;
        float card_width = 0.0f;
        float card_height = 0.0f;
    };

    struct DragItemState {
        uint32_t id = 0;
        Vec2 offset;
    };

} // namespace VoidGUI
