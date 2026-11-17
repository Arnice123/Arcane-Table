#pragma once

// The canonical stress scene for the ARCH-04 visibility spike.
//
// Numbers from the design doc's budget table (section 9.2), plus the map size and
// lengths chosen for the spike (record these in the ADR):
//   - map:     100 x 100 units (1 unit = 1 grid cell)
//   - walls:   500 segments, random start, random direction, length 2-10 units
//   - lights:  100, radius 6-12 units
//   - viewers: 12, radius 12 units
//   - tokens:  1,000 at random positions (the targets of "can I see this token?")
//
// Deterministic: the same seed gives the same scene on every compiler. std::mt19937
// itself is fully specified by the standard, but std::uniform_real_distribution is
// not (each standard library turns the raw numbers into floats its own way), so
// the raw 32-bit output is converted to floats here by hand.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>
#include <span>

#include "arcane/core/geometry/segment.hpp"
#include "arcane/core/vision/visibility_backend.hpp"

struct StressScene {
    std::vector<Segment> walls;     // 500
    std::vector<Viewer>  lights;    // 100
    std::vector<Viewer>  viewers;   // 12
    std::vector<Vec2>    tokens;    // 1000
};

namespace stress_scene_detail {

    inline constexpr float kMapSize = 100.0f;

    inline constexpr std::size_t kWallCount = 500;
    inline constexpr float kWallMinLength = 2.0f;
    inline constexpr float kWallMaxLength = 10.0f;

    inline constexpr std::size_t kLightCount = 100;
    inline constexpr float kLightMinRadius = 6.0f;
    inline constexpr float kLightMaxRadius = 12.0f;

    inline constexpr std::size_t kViewerCount = 12;
    inline constexpr float kViewerRadius = 12.0f;

    inline constexpr std::size_t kTokenCount = 1000;

    inline constexpr float kTwoPi = 6.28318530717958f;

    // Uniform float in [lo, hi), the same on every compiler: the top 24 bits of one
    // mt19937 draw, divided by 2^24, give an exact float in [0, 1).
    inline float uniform(std::mt19937& rng, float lo, float hi) {
        const float unit = static_cast<float>(rng() >> 8) / 16777216.0f;
        return lo + (hi - lo) * unit;
    }

    // A random point anywhere on the map.
    inline Vec2 randomPoint(std::mt19937& rng) {
        const float x = uniform(rng, 0.0f, kMapSize);
        const float y = uniform(rng, 0.0f, kMapSize);
        return Vec2{ x, y };
    }

}  // namespace stress_scene_detail

// In: a seed.  Out: the same scene every time for the same seed.
inline StressScene make_stress_scene(std::uint32_t seed = 1) {
    using namespace stress_scene_detail;

    std::mt19937 rng(seed);
    StressScene scene;

    // Walls: random start, random direction, random length. The far end may stick
    // out past the map edge a little; the spec doesn't require walls to stay inside.
    scene.walls.reserve(kWallCount);
    for (std::size_t i = 0; i < kWallCount; ++i) {
        const Vec2 start = randomPoint(rng);
        const float angle = stress_scene_detail::uniform(rng, 0.0f, kTwoPi);
        const float length = stress_scene_detail::uniform(rng, kWallMinLength, kWallMaxLength);
        const Vec2 end{ start.x + length * std::cos(angle), start.y + length * std::sin(angle) };
        scene.walls.push_back(Segment{ start, end });
    }

    scene.lights.reserve(kLightCount);
    for (std::size_t i = 0; i < kLightCount; ++i) {
        const Vec2 origin = randomPoint(rng);
        const float radius = stress_scene_detail::uniform(rng, kLightMinRadius, kLightMaxRadius);
        scene.lights.push_back(Viewer{ origin, radius });
    }

    scene.viewers.reserve(kViewerCount);
    for (std::size_t i = 0; i < kViewerCount; ++i) {
        scene.viewers.push_back(Viewer{ randomPoint(rng), kViewerRadius });
    }

    scene.tokens.reserve(kTokenCount);
    for (std::size_t i = 0; i < kTokenCount; ++i) {
        scene.tokens.push_back(randomPoint(rng));
    }

    return scene;
}







// In: every wall, and two points.  Out: true if no wall crosses the line a->b.
inline bool oracle_line_of_sight(std::span<const Segment> walls, Vec2 a, Vec2 b) {
    const Segment sightLine{ a, b };
    for (const Segment& wall : walls) {
        if (segments_intersect(wall, sightLine)) return false;
    }
    return true;
}