#pragma once

#include "void_types.hpp"
#include "void_font.hpp"

namespace VoidGUI {

    class DrawList {
    public:
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<DrawCommand> commands;
        std::vector<Rect> clip_stack;

        void Clear();
        void PushClipRect(const Rect& rect, bool intersect_with_current = true);
        void PopClipRect();

        void AddLine(Vec2 p1, Vec2 p2, Color col, float thickness = 1.0f);
        void AddPolyline(const Vec2* points, int count, Color col, float thickness = 1.0f);
        void AddRect(Vec2 min, Vec2 max, Color col, float rounding = 0.0f, float thickness = 1.0f);
        void AddRectFilled(Vec2 min, Vec2 max, Color col, float rounding = 0.0f);
        void AddRectFilledGradient(Vec2 min, Vec2 max, Color c_tl, Color c_tr, Color c_br, Color c_bl);
        void AddShadow(Vec2 min, Vec2 max, float rounding = 6.0f, float spread = 6.0f, Color col = Color(0, 0, 0, 100));
        void AddGlow(Vec2 min, Vec2 max, float rounding = 6.0f, float spread = 4.0f, Color col = Color(124, 58, 237, 70));
        void AddCircle(Vec2 center, float radius, Color col, int segments = 28, float thickness = 1.0f);
        void AddCircleFilled(Vec2 center, float radius, Color col, int segments = 28);
        void AddTriangleFilled(Vec2 p1, Vec2 p2, Vec2 p3, Color col);
        void AddText(Vec2 pos, Color col, const char* text, float scale = 1.0f);
        void AddImage(void* texture_id, Vec2 min, Vec2 max, Vec2 uv_min = Vec2(0, 0), Vec2 uv_max = Vec2(1, 1), Color col = Color(255, 255, 255, 255), float rounding = 0.0f);
        void AddImageRounded(void* texture_id, Vec2 min, Vec2 max, float rounding, Color col = Color(255, 255, 255, 255));
        void AddLiquidGlassPanel(Vec2 min, Vec2 max, float rounding, float time, Color base_glass, Color rim_highlight, Color liquid_accent);

    private:
        void EnsureCommand(void* texture_id = nullptr);
    };

}
