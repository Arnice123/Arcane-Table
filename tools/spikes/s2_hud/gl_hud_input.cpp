#include "gl_hud_input.hpp"


void GlHudInput::update_layout(const HudLayout& layout, QSize canvas_logical) {
    entries.clear();
    std::uint32_t s = layout.hotbar_slots.size();
    entries.reserve(s);
    for (std::uint32_t i = 0; i < s; ++i) {
        entries.push_back(std::pair<QRectF, ElementId>{layout.hotbar_slots[i], i});
    }
    entries.push_back(std::pair<QRectF, ElementId>{layout.nameplate_rect, s});
}


GlHudInput::ElementId GlHudInput::hit_test(QPoint pos) const {
    for (const auto& [rect, id] : entries) {
        if (rect.contains(pos)) {
            return id;
        }
    }
    return 0;
}