#pragma once

#include <random>
#include <string>
#include <vector>

#include <QPointF>
#include <QSize>

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
        qreal offsetX = (pan_x - world.x()) * zoom;
        qreal offsetY = (pan_y - world.y()) * zoom;
        return QPointF(canvas_size.rwidth() / 2 + offsetX, canvas_size.rheight() / 2 + offsetY);
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
        newScene.tokens.reserve(token_count);
        for (std::size_t i = 0; i < token_count; ++i) {
            float x = floatDist(rng);
            float y = floatDist(rng);
            std::string name;
            name.reserve(strLen);
            for (std::size_t j = 0; j < strLen; ++j) {
                name += characters[charDist(rng)];
            }
            float hp_frac = floatDist(rng);
            newScene.tokens.push_back(FakeToken(x,y,name,hp_frac));
        }

        std::size_t hotLen = 10;
        newScene.hotbar.reserve(hotLen);
        for (std::size_t i = 0; i < hotLen; ++i) {
            for (std::size_t j = 0; j < strLen; ++j) {
                newScene.hotbar[i] += characters[charDist(rng)];
            }
        }

        newScene.combat_test.reserve(strLen);
        for (std::size_t i = 0; i < strLen; ++i) {
            newScene.combat_test += characters[charDist(rng)];
        }

        return newScene;
    }
};