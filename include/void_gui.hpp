#pragma once

#include "void_types.hpp"
#include "void_font.hpp"
#include "void_draw.hpp"
#include "void_layout.hpp"
#include "void_widgets.hpp"
#include "void_popup.hpp"
#include <unordered_map>

namespace VoidGUI {

    class Context {
    public:
        Context();
        ~Context() = default;

        void NewFrame(float delta_time = 0.016f);
        void Render();

        // Hardware Input Stream
        void SetMousePos(float x, float y);
        void SetMouseDown(bool down);
        void SetMouseWheel(float delta);

        // Window & Layout Flow
        bool Begin(const char* title, Vec2 default_pos, Vec2 default_size);
        void End();

        // Multi-Column Responsive System
        void Columns(int count);
        void NextColumn();
        void EndColumns();

        // Group Containers & Adaptive Auto-Sized Cards (Eliminates empty boxes)
        void BeginCard(const char* title, float fixed_height = 0.0f);
        void EndCard();

        // Standalone Draggable Modular Window
        bool BeginDraggableCard(const char* title, Vec2* pos, Vec2 size, bool* open = nullptr);
        void EndDraggableCard();

        // Interactive Widgets
        void Text(const char* fmt, ...);
        void TextColored(Color col, const char* fmt, ...);
        bool Button(const char* label, Vec2 size = Vec2(0, 0));
        bool Checkbox(const char* label, bool* value);
        bool Toggle(const char* label, bool* value);
        bool SliderFloat(const char* label, float* value, float min, float max, const char* format = "%.2f");
        bool SliderInt(const char* label, int* value, int min, int max);
        bool Combo(const char* label, int* current_item, const char* const items[], int items_count);
        bool Keybind(const char* label, int* key);
        bool ColorEdit(const char* label, Color* color);
        bool TabBar(const char** tabs, int count, int* selected_tab);
        void ProgressBar(float fraction, const char* overlay = nullptr);
        void Separator();
        void Spacing(float height = 6.0f);

        // Accessors & Subsystems
        DrawList* GetDrawList() { return &m_draw_list; }
        Theme& GetTheme() { return m_theme; }
        const InputState& GetInput() const { return m_input; }
        WindowState& GetCurrentWindow() { return m_current_window; }
        float GetDeltaTime() const { return m_delta_time; }
        uint32_t GetActiveId() const { return m_active_id; }
        void SetActiveId(uint32_t id) { m_active_id = id; }
        uint32_t GetKeybindId() const { return m_keybind_id; }
        void SetKeybindId(uint32_t id) { m_keybind_id = id; }
        uint32_t GetHotId() const { return m_hot_id; }
        void SetHotId(uint32_t id) { m_hot_id = id; }

        uint32_t GetId(const char* str);
        float GetAnim(uint32_t id, bool active, float speed = 14.0f);
        float GetFloatState(uint32_t id, float target, float speed = 14.0f);

        float GetCardHeight(uint32_t id) const {
            auto it = m_card_heights.find(id);
            return (it != m_card_heights.end()) ? it->second : 0.0f;
        }
        void SetCardHeight(uint32_t id, float h) {
            m_card_heights[id] = h;
        }

    private:
        DrawList m_draw_list;
        InputState m_input;
        Theme m_theme;
        WindowState m_current_window;
        bool m_in_window = false;

        uint32_t m_active_id = 0;
        uint32_t m_keybind_id = 0;
        uint32_t m_hot_id = 0;
        float m_delta_time = 0.016f;

        DragItemState m_drag_item;
        std::unordered_map<uint32_t, float> m_card_heights;

        struct AnimState {
            float value = 0.0f;
        };
        std::vector<std::pair<uint32_t, AnimState>> m_anims;
    };

    // Global Context Lifecycle
    Context* GetCurrentContext();
    void SetCurrentContext(Context* ctx);

} // namespace VoidGUI

namespace KtnaGUI = VoidGUI;
namespace ktna = VoidGUI;
