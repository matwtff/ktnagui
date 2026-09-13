#include "../include/void_draw.hpp"
#include <cmath>

namespace VoidGUI {

    void DrawList::Clear() {
        vertices.clear();
        indices.clear();
        commands.clear();
        clip_stack.clear();
    }

    void DrawList::EnsureCommand(void* texture_id) {
        Rect clip = clip_stack.empty() ? Rect(-99999, -99999, 99999, 99999) : clip_stack.back();
        if (commands.empty() ||
            commands.back().texture_id != texture_id ||
            commands.back().clip_rect.min.x != clip.min.x ||
            commands.back().clip_rect.min.y != clip.min.y ||
            commands.back().clip_rect.max.x != clip.max.x ||
            commands.back().clip_rect.max.y != clip.max.y) {
            DrawCommand cmd;
            cmd.index_offset = static_cast<uint32_t>(indices.size());
            cmd.index_count = 0;
            cmd.clip_rect = clip;
            cmd.texture_id = texture_id;
            commands.push_back(cmd);
        }
    }

    void DrawList::PushClipRect(const Rect& rect, bool intersect_with_current) {
        if (clip_stack.empty() || !intersect_with_current) {
            clip_stack.push_back(rect);
        } else {
            const Rect& parent = clip_stack.back();
            Rect intersected(
                std::max(parent.min.x, rect.min.x),
                std::max(parent.min.y, rect.min.y),
                std::min(parent.max.x, rect.max.x),
                std::min(parent.max.y, rect.max.y)
            );
            clip_stack.push_back(intersected);
        }
    }

    void DrawList::PopClipRect() {
        if (!clip_stack.empty()) clip_stack.pop_back();
    }

    void DrawList::AddLine(Vec2 p1, Vec2 p2, Color col, float thickness) {
        EnsureCommand();
        Vec2 dir = p2 - p1;
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len < 0.0001f) return;

        Vec2 norm = Vec2(-dir.y / len, dir.x / len) * (thickness * 0.5f);
        uint32_t c = col.ToU32();
        uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({ p1.x + norm.x, p1.y + norm.y, 0, 0, c, 1.0f });
        vertices.push_back({ p2.x + norm.x, p2.y + norm.y, 0, 0, c, 1.0f });
        vertices.push_back({ p2.x - norm.x, p2.y - norm.y, 0, 0, c, 1.0f });
        vertices.push_back({ p1.x - norm.x, p1.y - norm.y, 0, 0, c, 1.0f });

        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);

        commands.back().index_count += 6;
    }

    void DrawList::AddPolyline(const Vec2* points, int count, Color col, float thickness) {
        if (!points || count < 2) return;
        for (int i = 0; i < count - 1; ++i) {
            AddLine(points[i], points[i + 1], col, thickness);
        }
    }

    void DrawList::AddRectFilled(Vec2 min, Vec2 max, Color col, float rounding) {
        EnsureCommand();
        uint32_t c = col.ToU32();
        uint32_t base = static_cast<uint32_t>(vertices.size());

        if (rounding <= 0.5f) {
            vertices.push_back({ min.x, min.y, 0, 0, c, 1.0f });
            vertices.push_back({ max.x, min.y, 0, 0, c, 1.0f });
            vertices.push_back({ max.x, max.y, 0, 0, c, 1.0f });
            vertices.push_back({ min.x, max.y, 0, 0, c, 1.0f });

            indices.push_back(base);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base);
            indices.push_back(base + 2);
            indices.push_back(base + 3);

            commands.back().index_count += 6;
            return;
        }

        const int corner_pts = 6;
        rounding = std::min(rounding, std::min((max.x - min.x) * 0.5f, (max.y - min.y) * 0.5f));

        Vec2 center(min.x + (max.x - min.x) * 0.5f, min.y + (max.y - min.y) * 0.5f);
        vertices.push_back({ center.x, center.y, 0, 0, c, 1.0f });
        uint32_t center_idx = base;

        Vec2 centers[4] = {
            { min.x + rounding, min.y + rounding },
            { max.x - rounding, min.y + rounding },
            { max.x - rounding, max.y - rounding },
            { min.x + rounding, max.y - rounding }
        };
        float angles[4] = { 3.14159265f, 4.71238898f, 0.0f, 1.57079632f };

        uint32_t first_ring_idx = static_cast<uint32_t>(vertices.size());
        for (int c_idx = 0; c_idx < 4; ++c_idx) {
            for (int i = 0; i <= corner_pts; ++i) {
                float a = angles[c_idx] + (1.57079632f * i / corner_pts);
                float vx = centers[c_idx].x + std::cos(a) * rounding;
                float vy = centers[c_idx].y + std::sin(a) * rounding;
                vertices.push_back({ vx, vy, 0, 0, c, 1.0f });
            }
        }

        uint32_t total_ring = static_cast<uint32_t>(vertices.size()) - first_ring_idx;
        for (uint32_t i = 0; i < total_ring; ++i) {
            uint32_t next = (i + 1) % total_ring;
            indices.push_back(center_idx);
            indices.push_back(first_ring_idx + i);
            indices.push_back(first_ring_idx + next);
            commands.back().index_count += 3;
        }
    }

    void DrawList::AddRect(Vec2 min, Vec2 max, Color col, float rounding, float thickness) {
        if (rounding <= 0.5f) {
            AddLine(min, Vec2(max.x, min.y), col, thickness);
            AddLine(Vec2(max.x, min.y), max, col, thickness);
            AddLine(max, Vec2(min.x, max.y), col, thickness);
            AddLine(Vec2(min.x, max.y), min, col, thickness);
            return;
        }

        const int corner_pts = 6;
        rounding = std::min(rounding, std::min((max.x - min.x) * 0.5f, (max.y - min.y) * 0.5f));
        Vec2 centers[4] = {
            { min.x + rounding, min.y + rounding },
            { max.x - rounding, min.y + rounding },
            { max.x - rounding, max.y - rounding },
            { min.x + rounding, max.y - rounding }
        };
        float angles[4] = { 3.14159265f, 4.71238898f, 0.0f, 1.57079632f };

        std::vector<Vec2> pts;
        for (int c_idx = 0; c_idx < 4; ++c_idx) {
            for (int i = 0; i <= corner_pts; ++i) {
                float a = angles[c_idx] + (1.57079632f * i / corner_pts);
                pts.push_back({ centers[c_idx].x + std::cos(a) * rounding, centers[c_idx].y + std::sin(a) * rounding });
            }
        }
        for (size_t i = 0; i < pts.size(); ++i) {
            size_t next = (i + 1) % pts.size();
            AddLine(pts[i], pts[next], col, thickness);
        }
    }

    void DrawList::AddRectFilledGradient(Vec2 min, Vec2 max, Color c_tl, Color c_tr, Color c_br, Color c_bl) {
        EnsureCommand();
        uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({ min.x, min.y, 0, 0, c_tl.ToU32(), 1.0f });
        vertices.push_back({ max.x, min.y, 0, 0, c_tr.ToU32(), 1.0f });
        vertices.push_back({ max.x, max.y, 0, 0, c_br.ToU32(), 1.0f });
        vertices.push_back({ min.x, max.y, 0, 0, c_bl.ToU32(), 1.0f });

        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);

        commands.back().index_count += 6;
    }

    void DrawList::AddShadow(Vec2 min, Vec2 max, float rounding, float spread, Color col) {
        for (int i = 1; i <= 3; ++i) {
            float cur_spread = spread * (i / 3.0f);
            uint8_t a = static_cast<uint8_t>(col.a * (1.0f - (i - 1) * 0.3f));
            AddRect(min - Vec2(cur_spread, cur_spread), max + Vec2(cur_spread, cur_spread),
                    Color(col.r, col.g, col.b, a), rounding + cur_spread, 1.5f);
        }
    }

    void DrawList::AddGlow(Vec2 min, Vec2 max, float rounding, float spread, Color col) {
        for (int i = 1; i <= 2; ++i) {
            float s = spread * i;
            uint8_t a = static_cast<uint8_t>(col.a / (i * 1.5f));
            AddRect(min - Vec2(s, s), max + Vec2(s, s), Color(col.r, col.g, col.b, a), rounding + s, 1.5f);
        }
    }

    void DrawList::AddCircle(Vec2 center, float radius, Color col, int segments, float thickness) {
        float step = 6.2831853f / segments;
        for (int i = 0; i < segments; ++i) {
            float a1 = i * step;
            float a2 = (i + 1) * step;
            Vec2 p1(center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius);
            Vec2 p2(center.x + std::cos(a2) * radius, center.y + std::sin(a2) * radius);
            AddLine(p1, p2, col, thickness);
        }
    }

    void DrawList::AddCircleFilled(Vec2 center, float radius, Color col, int segments) {
        EnsureCommand();
        uint32_t c = col.ToU32();
        uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({ center.x, center.y, 0, 0, c, 1.0f });
        float step = 6.2831853f / segments;

        for (int i = 0; i <= segments; ++i) {
            float a = i * step;
            vertices.push_back({ center.x + std::cos(a) * radius, center.y + std::sin(a) * radius, 0, 0, c, 1.0f });
        }

        for (int i = 0; i < segments; ++i) {
            indices.push_back(base);
            indices.push_back(base + 1 + i);
            indices.push_back(base + 2 + i);
            commands.back().index_count += 3;
        }
    }

    void DrawList::AddTriangleFilled(Vec2 p1, Vec2 p2, Vec2 p3, Color col) {
        EnsureCommand();
        uint32_t c = col.ToU32();
        uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({ p1.x, p1.y, 0, 0, c, 1.0f });
        vertices.push_back({ p2.x, p2.y, 0, 0, c, 1.0f });
        vertices.push_back({ p3.x, p3.y, 0, 0, c, 1.0f });

        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        commands.back().index_count += 3;
    }

    void DrawList::AddText(Vec2 pos, Color col, const char* text, float scale) {
        if (!text || !*text) return;

        if (Font::HasAtlas()) {
            EnsureCommand(Font::GetAtlasSRV());
            uint32_t c = col.ToU32();

            float cur_x = pos.x;
            float cur_y = pos.y;
            float line_h = Font::GetLineHeight(scale);

            while (*text) {
                char ch = *text++;
                if (ch == '\n') {
                    cur_x = pos.x;
                    cur_y += line_h;
                    continue;
                }

                const GlyphMetric* gm = Font::GetGlyph(ch);
                if (!gm) continue;

                if (ch != ' ' && gm->width > 0.0f && gm->height > 0.0f) {
                    float p0_x = cur_x + gm->x_offset * scale;
                    float p0_y = cur_y + gm->y_offset * scale;
                    float p1_x = p0_x + gm->width * scale;
                    float p1_y = p0_y + gm->height * scale;

                    uint32_t base = static_cast<uint32_t>(vertices.size());
                    vertices.push_back({ p0_x, p0_y, gm->u0, gm->v0, c, 2.0f });
                    vertices.push_back({ p1_x, p0_y, gm->u1, gm->v0, c, 2.0f });
                    vertices.push_back({ p1_x, p1_y, gm->u1, gm->v1, c, 2.0f });
                    vertices.push_back({ p0_x, p1_y, gm->u0, gm->v1, c, 2.0f });

                    indices.push_back(base + 0);
                    indices.push_back(base + 1);
                    indices.push_back(base + 2);
                    indices.push_back(base + 0);
                    indices.push_back(base + 2);
                    indices.push_back(base + 3);
                    commands.back().index_count += 6;
                }

                cur_x += gm->x_advance * scale;
            }
            return;
        }

        EnsureCommand();
        float cur_x = pos.x;
        float cur_y = pos.y;
        float line_h = Font::GetLineHeight(scale);
        float pixel_sz = 1.0f * scale;

        while (*text) {
            char ch = *text++;
            if (ch == '\n') {
                cur_x = pos.x;
                cur_y += line_h;
                continue;
            }

            uint8_t w, h;
            const uint8_t* glyph = Font::GetGlyphBitmap(ch, w, h);
            for (int col_i = 0; col_i < w; ++col_i) {
                uint8_t bits = glyph[col_i];
                for (int row_i = 0; row_i < h; ++row_i) {
                    if (bits & (1 << row_i)) {
                        float px = cur_x + col_i * pixel_sz;
                        float py = cur_y + row_i * pixel_sz;
                        AddRectFilled(Vec2(px, py), Vec2(px + pixel_sz, py + pixel_sz), col, 0.0f);
                    }
                }
            }
            cur_x += Font::GetCharWidth(ch, scale);
        }
    }

    void DrawList::AddImage(void* texture_id, Vec2 p_min, Vec2 p_max, Vec2 uv_min, Vec2 uv_max, Color col, float rounding) {
        if (!texture_id) return;
        if (rounding > 0.5f) {
            AddImageRounded(texture_id, p_min, p_max, rounding, col);
            return;
        }

        EnsureCommand(texture_id);
        uint32_t c = col.ToU32();
        uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({ p_min.x, p_min.y, uv_min.x, uv_min.y, c, 2.0f });
        vertices.push_back({ p_max.x, p_min.y, uv_max.x, uv_min.y, c, 2.0f });
        vertices.push_back({ p_max.x, p_max.y, uv_max.x, uv_max.y, c, 2.0f });
        vertices.push_back({ p_min.x, p_max.y, uv_min.x, uv_max.y, c, 2.0f });

        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
        commands.back().index_count += 6;
    }

    void DrawList::AddImageRounded(void* texture_id, Vec2 p_min, Vec2 p_max, float rounding, Color col) {
        if (!texture_id) return;
        EnsureCommand(texture_id);
        uint32_t c = col.ToU32();
        uint32_t base = static_cast<uint32_t>(vertices.size());

        float w = p_max.x - p_min.x;
        float h = p_max.y - p_min.y;
        float r = std::min(rounding, std::min(w, h) * 0.5f);

        Vec2 center = (p_min + p_max) * 0.5f;
        vertices.push_back({ center.x, center.y, 0.5f, 0.5f, c, 2.0f });

        const int segments = 6;
        const float half_pi = 1.57079632679f;

        Vec2 corners[4] = {
            Vec2(p_max.x - r, p_min.y + r),
            Vec2(p_max.x - r, p_max.y - r),
            Vec2(p_min.x + r, p_max.y - r),
            Vec2(p_min.x + r, p_min.y + r)
        };
        float start_angles[4] = { -half_pi, 0.0f, half_pi, 3.14159265f };

        uint32_t perimeter_start = static_cast<uint32_t>(vertices.size());
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j <= segments; ++j) {
                float angle = start_angles[i] + (half_pi * j / segments);
                Vec2 pt = corners[i] + Vec2(std::cos(angle), std::sin(angle)) * r;
                float u = (pt.x - p_min.x) / w;
                float v = (pt.y - p_min.y) / h;
                vertices.push_back({ pt.x, pt.y, u, v, c, 2.0f });
            }
        }

        uint32_t perimeter_count = static_cast<uint32_t>(vertices.size()) - perimeter_start;
        for (uint32_t i = 0; i < perimeter_count; ++i) {
            uint32_t next = (i + 1) % perimeter_count;
            indices.push_back(base);
            indices.push_back(perimeter_start + i);
            indices.push_back(perimeter_start + next);
            commands.back().index_count += 3;
        }
    }

    void DrawList::AddLiquidGlassPanel(Vec2 min, Vec2 max, float rounding, float time, Color base_glass, Color rim_highlight, Color liquid_accent) {
        float w = max.x - min.x;
        float h = max.y - min.y;
        if (w <= 0.0f || h <= 0.0f) return;

        AddShadow(min, max, rounding, 12.0f, Color(0, 0, 0, 130));
        AddGlow(min, max, rounding, 6.0f, liquid_accent.WithAlpha(18));

        Color c_tl = Color(
            static_cast<uint8_t>(std::min(255, base_glass.r + 4)),
            static_cast<uint8_t>(std::min(255, base_glass.g + 4)),
            static_cast<uint8_t>(std::min(255, base_glass.b + 6)),
            base_glass.a
        );
        Color c_tr = Color(
            static_cast<uint8_t>(std::min(255, base_glass.r + 2)),
            static_cast<uint8_t>(std::min(255, base_glass.g + 2)),
            static_cast<uint8_t>(std::min(255, base_glass.b + 4)),
            base_glass.a
        );
        Color c_br = Color(
            static_cast<uint8_t>(std::max(0, base_glass.r - 4)),
            static_cast<uint8_t>(std::max(0, base_glass.g - 4)),
            static_cast<uint8_t>(std::max(0, base_glass.b - 2)),
            static_cast<uint8_t>(std::min(255, base_glass.a + 12))
        );
        Color c_bl = Color(
            static_cast<uint8_t>(std::max(0, base_glass.r - 3)),
            static_cast<uint8_t>(std::max(0, base_glass.g - 3)),
            static_cast<uint8_t>(std::max(0, base_glass.b - 1)),
            static_cast<uint8_t>(std::min(255, base_glass.a + 8))
        );

        AddRectFilledGradient(min, max, c_tl, c_tr, c_br, c_bl);

        float top_fade_h = std::min(h * 0.35f, 28.0f);
        if (top_fade_h > 2.0f) {
            AddRectFilledGradient(
                min, Vec2(max.x, min.y + top_fade_h),
                Color(255, 255, 255, 10), Color(255, 255, 255, 8),
                Color(255, 255, 255, 0), Color(255, 255, 255, 0)
            );
        }

        AddRect(min, max, rim_highlight, rounding, 1.0f);

        float rim_pad = std::max(2.0f, rounding);
        if (w > rim_pad * 2.0f) {
            AddLine(Vec2(min.x + rim_pad, min.y + 0.5f), Vec2(max.x - rim_pad, min.y + 0.5f), Color(255, 255, 255, 55), 1.0f);
        }

        if (w > 4.0f && h > 4.0f) {
            AddRect(min + Vec2(1.0f, 1.0f), max - Vec2(1.0f, 1.0f), Color(255, 255, 255, 8), std::max(0.0f, rounding - 1.0f), 1.0f);
        }
    }

}
