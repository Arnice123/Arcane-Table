#pragma once

#include <random>
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <utility>

#include <QPointF>
#include <QSize>
#include <QRectF>

struct FakeToken
{
    // position on map in world units
	float world_x;
	float world_y;

    // nameplate name
	std::string name;

    // 0..1 for healthbar
	float hp_frac;
};

struct Camera {
    // world point at the center of screen
	float pan_x;
	float pan_y;
	float zoom;

    // In: world coords and canvas size in pixels
    // Out: Pixel coords
    QPointF world_to_screen(QPointF world, QSize canvas_size) const {
        qreal offsetX = (world.x() - pan_x) * zoom;
        qreal offsetY = (world.y() - pan_y) * zoom;
        return QPointF(canvas_size.rwidth() / 2 + offsetX, canvas_size.rheight() / 2 + offsetY);
    }

    QPointF screen_to_world(QPointF screen, QSize canvas_size) const {
        return QPointF(
            (screen.x() - canvas_size.width() / 2.0) / zoom + pan_x,
            (screen.y() - canvas_size.height() / 2.0) / zoom + pan_y
            );
    }
};

struct HudLayout {
    // SCREEN-space (logical px). Fixed on the window, do not move with the camera.
    std::array<QRectF, 10> hotbar_slots;   // 48x48 each, bottom center, 4 px gaps
    QRectF combat_strip;                   // about 600x32, top center

    // WORLD-space (world units), relative to a token's position. Move and scale with the map.
    QRectF nameplate_rect;                 // about 48x14, floating just above the token
    QRectF hp_bar_rect;                    // about 40x3 inside it; fill width = hp_fraction * width

    // Builds the layout for a given window size.
    static HudLayout compute(QSize canvas_size) {
        HudLayout layout{};

        const float w = canvas_size.width();
        const float h = canvas_size.width();

        // Hotbar
        constexpr float SLOT_SIZE = 48.0f;
        constexpr float SLOT_GAP = 4.0f;
        constexpr int SLOT_COUNT = 10;

        constexpr float TOP_MARGIN = 16.0f;

        const float total_width = SLOT_COUNT * SLOT_SIZE + (SLOT_COUNT - 1) * SLOT_GAP;

        const float start_x = (w - total_width) / 2.0f;
        const float y = h - SLOT_SIZE - TOP_MARGIN;

        for (int i = 0; i < SLOT_COUNT; ++i) {
            const float x = start_x + i * (SLOT_SIZE + SLOT_GAP);
            layout.hotbar_slots[i] = QRectF(x,y,SLOT_SIZE, SLOT_SIZE);
        }

        constexpr float COMBAT_WIDTH = 600.0f;
        constexpr float COMBAT_HEIGHT = 32.0f;


        layout.combat_strip = QRectF((w - COMBAT_WIDTH) / 2.0f, TOP_MARGIN, COMBAT_WIDTH,COMBAT_HEIGHT);

        // World-space elements
        layout.nameplate_rect = QRectF(-24, -30, 48, 14);
        layout.hp_bar_rect = QRectF(-20, -14, 40, 3);


        return layout;
    }
};

struct HudTestScene {
	std::vector<FakeToken> tokens;
	std::vector<std::string> hotbar;
	std::string combat_test;

    // In: a seed and token count
    // Out: the scene
    static HudTestScene make(std::uint32_t seed, std::size_t token_count) {
        const std::string characters =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

        std::mt19937 rng(seed);
        HudTestScene newScene{};

        std::size_t strLen = 20;
        std::uniform_int_distribution<std::size_t> charDist(0, characters.size() - 1);
        std::uniform_real_distribution<float> floatDist(0, token_count / 2);
        std::uniform_real_distribution<float> hpDist(0.0f, 1.0f);
        newScene.tokens.reserve(token_count);
        for (std::size_t i = 0; i < token_count; ++i) {
            float x = floatDist(rng);
            float y = floatDist(rng);
            std::string name;
            name.reserve(strLen);
            for (std::size_t j = 0; j < strLen; ++j) {
                name += characters[charDist(rng)];
            }
            float hp_frac = hpDist(rng);
            newScene.tokens.push_back(FakeToken{x,y,name,hp_frac});
        }

        std::size_t hotLen = 10;
        newScene.hotbar.reserve(hotLen);
        for (std::size_t i = 0; i < hotLen; ++i) {
            std::string item;
            item.reserve(strLen);

            for (std::size_t j = 0; j < strLen; ++j) {
                item += characters[charDist(rng)];
            }
            newScene.hotbar.push_back(std::move(item));
        }

        newScene.combat_test.reserve(strLen);
        for (std::size_t i = 0; i < strLen; ++i) {
            newScene.combat_test += characters[charDist(rng)];
        }

        return newScene;
    }
};