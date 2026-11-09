#pragma once

#include <vector>
#include <span>
#include <random>

#include "arcane/core/vision/visibility_backend.hpp"

struct StressScene {
	std::vector<Segment> walls,
	std::vector<Viewer> lights;
	std::vector<Viewer> viewers;
	std::vector<Vec2> tokens;
};

// In: a seed.  Out: the same scene every time for the same seed.
StressScene make_stress_scene(std::uint32_t seed)
{
    std::mt19937 rng(seed);

    std::uniform_int_distribution<std::size_t> wall_count(100, 1000);
    std::uniform_int_distribution<std::size_t> light_count(10, 200);
    std::uniform_int_distribution<std::size_t> viewer_count(2, 30);
    std::uniform_int_distribution<std::size_t> token_count(100, 2000);

    StressScene scene;

    std::uniform_real_distribution<float> pos_xy(-250.0f, 250.0f);

    const auto num_walls = wall_count(rng);
    scene.walls.reserve(num_walls);

    for (std::size_t i = 0; i < num_walls; ++i)
    {
        scene.walls.push_back(Segment{Vec2{pos_xy(rng), pos_xy(rng)}, Vec2{pos_xy(rng), pos_xy(rng)} });
    }

    std::uniform_real_distribution<float> light_dist(5.0f, 60.0f);

    const auto num_lights = light_count(rng);
    scene.lights.reserve(num_lights);

    for (std::size_t i = 0; i < num_lights; ++i)
    {
        scene.lights.push_back(Viewer{Vec2{pos_xy(rng), pos_xy(rng)}, light_dist(rng)});
    }

    const auto num_viewers = viewer_count(rng);
    scene.viewers.reserve(num_viewers);

    for (std::size_t i = 0; i < num_viewers; ++i)
    {
        scene.viewers.push_back(Viewer{Vec2{pos_xy(rng), pos_xy(rng)}, light_dist(rng)});
    }

    const auto num_tokens = token_count(rng);
    scene.tokens.reserve(num_tokens);

    for (std::size_t i = 0; i < num_tokens; ++i)
    {
        scene.tokens.push_back(Vec2{pos_xy(rng), pos_xy(rng)});
    }

    return scene;
}

// The ground truth. Slow but obviously correct: the straight line a->b is visible exactly when it crosses no wall. Does no shortcuts.
bool oracle_line_of_sight(std::span<const Segment> walls, Vec2 a, Vec2 b)
{
    Segment sightLine{ a, b };
    for (Segment& wall : walls)
    {
        if (segments_intersect(wall, sightLine))
        {
            return false;
        }
    }
    return true;
}