#include "../include/void_gui.hpp"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace VoidGUI {

    static Context* g_context = nullptr;

    Context* GetCurrentContext() { return g_context; }
    void SetCurrentContext(Context* ctx) { g_context = ctx; }

    Context::Context() {
        SetCurrentContext(this);
    }

    void Context::NewFrame(float delta_time) {
        m_delta_time = (delta_time > 0.0001f && delta_time < 0.2f) ? delta_time : 0.016f;
        m_theme.liquid_time += m_delta_time;
        m_draw_list.Clear();

        // Edge detection for reliable, crisp hardware clicks
        m_input.mouse_clicked = (m_input.mouse_down && !m_input.mouse_down_prev);
        m_input.mouse_released = (!m_input.mouse_down && m_input.mouse_down_prev);
        m_input.mouse_down_prev = m_input.mouse_down;

        if (!m_input.mouse_down) {
            if (m_current_window.dragging) {
                m_current_window.dragging = false;
            }
            if (m_current_window.resizing) {
                m_current_window.resizing = false;
            }
            if (m_drag_item.id != 0) {
                m_drag_item.id = 0;
            }
            // If active_id is not a modal keybind waiting for user input, clear it
            if (m_active_id != 0 && m_active_id != m_keybind_id) {
                m_active_id = 0;
            }
        }

        m_input.mouse_wheel = 0.0f;
    }

    void Context::Render() {
        // Draw commands are prepared in m_draw_list and executed by backend
    }

    void Context::SetMousePos(float x, float y) {
        m_input.mouse_pos = Vec2(x, y);
    }

    void Context::SetMouseDown(bool down) {
        m_input.mouse_down = down;
    }

    void Context::SetMouseWheel(float delta) {
        m_input.mouse_wheel = delta;
    }

    // FNV-1a 32-bit Hash (Fast, zero-collision immediate-mode ID generator)
    uint32_t Context::GetId(const char* str) {
        if (!str) return 0;
        uint32_t hash = 0x811C9DC5;
        while (*str) {
            hash ^= static_cast<uint8_t>(*str++);
            hash *= 0x01000193;
        }
        return hash;
    }

    float Context::GetAnim(uint32_t id, bool active, float speed) {
        float target = active ? 1.0f : 0.0f;
        for (auto& pair : m_anims) {
            if (pair.first == id) {
                float dt = m_delta_time;
                pair.second.value += (target - pair.second.value) * (1.0f - std::exp(-speed * dt));
                if (std::abs(pair.second.value - target) < 0.001f) {
                    pair.second.value = target;
                }
                return pair.second.value;
            }
        }
        m_anims.push_back({ id, { target } });
        return target;
    }

    float Context::GetFloatState(uint32_t id, float target, float speed) {
        for (auto& pair : m_anims) {
            if (pair.first == id) {
                float dt = m_delta_time;
                pair.second.value += (target - pair.second.value) * (1.0f - std::exp(-speed * dt));
                if (std::abs(pair.second.value - target) < 0.05f) {
                    pair.second.value = target;
                }
                return pair.second.value;
            }
        }
        m_anims.push_back({ id, { target } });
        return target;
    }

    void Context::Text(const char* fmt, ...) {
        char buf[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        Widgets::Text(this, "%s", buf);
    }

    void Context::TextColored(Color col, const char* fmt, ...) {
        char buf[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        Widgets::TextColored(this, col, "%s", buf);
    }

    bool Context::Button(const char* label, Vec2 size) {
        return Widgets::Button(this, label, size);
    }

    bool Context::Checkbox(const char* label, bool* value) {
        return Widgets::Checkbox(this, label, value);
    }

    bool Context::Toggle(const char* label, bool* value) {
        return Widgets::Toggle(this, label, value);
    }

    bool Context::SliderFloat(const char* label, float* value, float min, float max, const char* format) {
        return Widgets::SliderFloat(this, label, value, min, max, format);
    }

    bool Context::SliderInt(const char* label, int* value, int min, int max) {
        return Widgets::SliderInt(this, label, value, min, max);
    }

    bool Context::Combo(const char* label, int* current_item, const char* const items[], int items_count) {
        return Widgets::Combo(this, label, current_item, items, items_count);
    }

    bool Context::Keybind(const char* label, int* key) {
        return Widgets::Keybind(this, label, key);
    }

    bool Context::ColorEdit(const char* label, Color* color) {
        return Widgets::ColorEdit(this, label, color);
    }

    bool Context::TabBar(const char** tabs, int count, int* selected_tab) {
        return Widgets::TabBar(this, tabs, count, selected_tab);
    }

    void Context::ProgressBar(float fraction, const char* overlay) {
        Widgets::ProgressBar(this, fraction, overlay);
    }

    void Context::Separator() {
        Widgets::Separator(this);
    }

    void Context::Spacing(float height) {
        Widgets::Spacing(this, height);
    }

} // namespace VoidGUI
