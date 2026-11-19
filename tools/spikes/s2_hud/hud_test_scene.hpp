#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include <QPointF>
#include <QRectF>
#include <QSize>

struct FakeToken {
    float world_x;
    float world_y;
    std::string name;
    float hp_frac;      // 0..1
};

struct Camera {
    float pan_x = 640.0f;   // center of the 1280x720 world
    float pan_y = 360.0f;
    float zoom  = 1.0f;

    QPointF world_to_screen(QPointF world, QSize canvas_size) const {
        return QPointF(canvas_size.width()  / 2.0 + (world.x() - pan_x) * zoom,
                       canvas_size.height() / 2.0 + (world.y() - pan_y) * zoom);
    }

    QPointF screen_to_world(QPointF screen, QSize canvas_size) const {
        return QPointF((screen.x() - canvas_size.width()  / 2.0) / zoom + pan_x,
                       (screen.y() - canvas_size.height() / 2.0) / zoom + pan_y);
    }
};

struct HudLayout {
    std::array<QRectF, 10> hotbar_slots;
    QRectF combat_strip;
    QRectF nameplate_rect;
    QRectF hp_bar_rect;

    static HudLayout compute(QSize canvas_size) {
        HudLayout layout{};

        const float w = canvas_size.width();
        const float h = canvas_size.height();

        constexpr float SLOT_SIZE = 48.0f;
        constexpr float SLOT_GAP = 4.0f;
        constexpr int   SLOT_COUNT = 10;
        constexpr float MARGIN = 16.0f;

        const float total_width = SLOT_COUNT * SLOT_SIZE + (SLOT_COUNT - 1) * SLOT_GAP;
        const float start_x = (w - total_width) / 2.0f;
        const float y = h - SLOT_SIZE - MARGIN;

        for (int i = 0; i < SLOT_COUNT; ++i)
            layout.hotbar_slots[i] = QRectF(start_x + i * (SLOT_SIZE + SLOT_GAP), y, SLOT_SIZE, SLOT_SIZE);

        constexpr float COMBAT_W = 600.0f, COMBAT_H = 32.0f;
        layout.combat_strip = QRectF((w - COMBAT_W) / 2.0f, MARGIN, COMBAT_W, COMBAT_H);

        // plate spans y -24..-10, so the bar (y -14..-11) is inside it
        layout.nameplate_rect = QRectF(-24, -24, 48, 14);
        layout.hp_bar_rect    = QRectF(-20, -14, 40, 3);
        return layout;
    }
};

struct HudTestScene {
    std::vector<FakeToken> tokens;
    std::vector<std::string> hotbar;
    std::string combat_text;

    static constexpr float WORLD_W = 1280.0f;
    static constexpr float WORLD_H = 720.0f;

    static HudTestScene make(std::uint32_t seed, std::size_t token_count) {
        static const std::string chars =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

        std::mt19937 rng(seed);
        std::uniform_int_distribution<std::size_t> charDist(0, chars.size() - 1);
        std::uniform_real_distribution<float> xDist(0.0f, WORLD_W);
        std::uniform_real_distribution<float> yDist(0.0f, WORLD_H);
        std::uniform_real_distribution<float> hpDist(0.0f, 1.0f);

        auto random_string = [&](std::size_t len) {
            std::string s;
            s.reserve(len);
            for (std::size_t i = 0; i < len; ++i) s += chars[charDist(rng)];
            return s;
        };

        HudTestScene scene{};
        scene.tokens.reserve(token_count);
        for (std::size_t i = 0; i < token_count; ++i) {
            float x = xDist(rng);
            float y = yDist(rng);
            std::string name = random_string(10);
            float hp = hpDist(rng);
            scene.tokens.push_back({x, y, std::move(name), hp});
        }

        scene.hotbar.reserve(10);
        for (int i = 0; i < 10; ++i)
            scene.hotbar.push_back(random_string(4));

        scene.combat_text = random_string(40);
        return scene;
    }
};