#pragma once

#include <optional>
#include <algorithm>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <iostream>

#include "arcane\core\geometry\math.hpp"

struct Segment {
	Vec2 a;
	Vec2 b;
};

// Where does a RAY (origin + direction, going forever one way) first hit a segment?
// In: ray origin, ray dir, a segment
// Out: the distance along the ray to the hit/nothing if it misses
std::optional<float> ray_segment_hit(Vec2 origin, Vec2 dir, const Segment& s, float epsilon = 1e-5);

// Do two segments cross
// Outputs true if the segments intersect
bool segments_intersect(const Segment& s, const Segment& t, float epsilon = 1e-5);