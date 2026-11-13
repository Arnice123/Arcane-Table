#include "arcane\core\vision\polygon_backend.hpp"
#include "arcane/core/geometry/segment.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>
#include <functional>


namespace {
    constexpr float kPi = 3.14159265358979f;

    // How far (radians) the two side rays sit either side of each corner.
    // The side ray misses the corner by about (distance to corner * kAngleEps). 
    // That gap must be clearly bigger than ray_segment_hit's epsilon, 
    // or the side ray still "hits" the wall and you can't see round the corner. 
    constexpr float kAngleEps = 1e-4f;

    // Evenly spaced rays around the full circle.
    constexpr int kArcSamples = 64;

    // How far to pull each end of a sight line inward before testing it, so a point on a wall's face (or a viewer standing on a wall) isn't counted as blocked.
    // Must be bigger than segments_intersect's epsilon.
    constexpr float kSightTrim = 1e-4f;

    // In:  a point p and a segment s.
    // Out: squared distance from p to the closest point on s.
    float distToSegment2(Vec2 p, const Segment& s) {
        Vec2 d = s.b - s.a;
        float len2 = Math::dot(d, d);

        // check to see if s has 0 length
        if (len2 == 0.0f) {
            Vec2 diff = p - s.a;
            return Math::dot(diff, diff);
        }

        // calculate the closest point to p 
        float t = std::clamp(Math::dot(p - s.a, d) / len2, 0.0f, 1.0f );
        Vec2 closestPoint = s.a + d * t;

        // calculate so diff.x = p.x - cPoint.x (same w/ y)
        Vec2 diff = p - closestPoint;

        return Math::dot(diff, diff);
    }

    // In:  a segment s, a circle (centre, radius), and an output list.
    // Outcome: every point where s crosses the circle's edge is appended to out (0, 1 or 2 points).
    void circleCrossings(const Segment& s, Vec2 centre, float radius, std::vector<Vec2>& out) {
        // Represent segment as P(u) = s.a + u * d, where u is in [0, 1].
        Vec2 d = s.b - s.a;

        // Quadratic coefficient a = d dot d.        
        float a = Math::length_squared(d);
        if (a == 0) return; // Zero-length segment cannot produce any crossings.

        float circleX = s.a.x - centre.x;
        float circleY = s.a.y - centre.y;

        // Substitute P(u) into circle equation: |P(u) - centre|^2 = radius^2 which produces the quadratic: a*u^2 + b*u + c = 0.
        float b = 2 * (circleX * d.x + circleY * d.y);
        float c = circleX * circleX + circleY * circleY - radius * radius;

        // Determine number of intersections using the discriminant.
        float discriminant = b * b - 4 * a * c;

        // Negative discriminant: line does not intersect circle.
        if (discriminant < 0) return;

        // Approximately zero discriminant: tangent intersection (one root).
        if (discriminant < 1e-5f) {
            float u = -b / (2 * a);
            if (u >= 0 && u <= 1) {
                out.push_back(s.a + u * d);
            }
        }
        else {
            // Two intersections.
            float disc = std::sqrt(discriminant);

            // use numerically stable for quadratic formula
            float q = -0.5f * (b + std::copysign(disc, b));

            float u1 = q / a;
            float u2 = c / q;

            if (u1 >= 0 && u1 <= 1) {
                out.push_back(s.a + u1 * d);
            }
            if (u2 >= 0 && u2 <= 1) {
                out.push_back(s.a + u2 * d);
            }
        }
    }

    // -----------------------------------------------------------------------------------------
    // The VisibilityField returned by compute().
    // -----------------------------------------------------------------------------------------
    class PolygonField final : public VisibilityField {
    public:
        // In:  the viewer's origin and radius, and the polygon points (already in angle order).
        // Outcome: all three are stored (take the points by moving them in).
        PolygonField(Vec2 origin, float radius, std::vector<Vec2> pts) {
        }

        // In:  a point p.
        // Out: true if p is inside the visible polygon.
        // Algo: 1. if p is further from origin_ than radius_ (compare squared distances) → false.
        //          The arc is made of short straight edges, so this keeps the circle edge exact.
        //       2. even-odd rule: start with an "inside" flag set to false. Walk every edge of pts_
        //          (each point paired with the one before it, wrapping round so the last pairs with
        //          the first). For an edge whose two ends are on opposite sides of p's y (one above,
        //          one not): find the x where the edge crosses that y; if it's to the right of p,
        //          flip the flag.
        //       3. return the flag.
        bool contains(Vec2 point) const override {
        }

        // Out: a copy of the polygon points.
        std::vector<Vec2> outline() const override {
        }

    private:
        Vec2 origin_;
        float radius_;
        std::vector<Vec2> pts_;
    };

}  // namespace

// Out: the backend's short name, "polygon".
std::string_view PolygonBackend::name() const {
}

// In:  every wall in the map.
// Outcome: walls_ holds a copy of them and index_ is built over that copy.
// Algo: copy the span into walls_ (the caller's span may not outlive this call),
//       then bulk-load index_ from walls_ with the overload that stores each segment's index.
//       All walls arrive at once, so bulk loading, not inserting one at a time.
void PolygonBackend::set_occluders(std::span<const Segment> walls) {
}

// In:  a viewer (origin, radius).
// Out: a PolygonField describing what the viewer can see.
// Algo:
// 1. Nearby walls → a list of walls that reach into the circle
//    - query index_ with the square around the viewer: origin minus radius to origin plus radius
//      on both axes
//    - the tree only compares boxes, so for each index it returns, keep that wall only if
//      distToSegment2(origin, wall) is within radius squared
//    - everything below uses only this list, never the full wall list
//
// 2. Points the polygon can turn at → a list of target points
//    - for each nearby wall:
//        * each of its two endpoints that lies inside the circle
//        * the points from circleCrossings(wall, origin, radius, ...) where it crosses the circle edge
//
// 3. Ray angles → a list of angles
//    - kArcSamples evenly spaced angles covering the full turn (from -pi, step 2*pi / count)
//    - for each target point: its angle from the origin (atan2 of target minus origin),
//      plus that angle - kAngleEps and that angle + kAngleEps  → 3 angles per target
//    - reserving room first is worthwhile: 3 per target plus kArcSamples
//
// 4. Cast each ray → a list of (angle, hit point) pairs
//    - unit direction from the angle (cos, sin); it must be unit length so that
//      ray_segment_hit's result is a distance
//    - the nearest distance starts at the radius
//    - for each nearby wall: if ray_segment_hit gives a result, clamp it to at least 0
//      (it can be a hair negative); if that's smaller than the nearest so far, it's the new nearest
//    - the hit point is origin + direction * nearest; record it with its angle
//
// 5. Sort the pairs by angle. Their points, in that order, are the polygon.
//    (An angle can be a hair past ±pi because of the side rays. That's fine: the polygon
//     wraps round, so it still ends up next to its neighbours.)
//
// 6. Copy the points out in that order and return a new PolygonField(origin, radius, points).
std::unique_ptr<VisibilityField> PolygonBackend::compute(const Viewer& viewer) const {
}

// In:  two points a and b.
// Out: true if no wall blocks the straight line between them.
// Algo:
// 1. Make the sight line a→b, pulled inward by kSightTrim at both ends:
//    move each end toward the other along the unit direction a→b.
//    If a and b are closer together than 2 * kSightTrim, nothing can be between them → true.
// 2. Query index_ with the box around the trimmed line (Rect has a helper for a segment's box).
// 3. Keep a "blocked" flag, starting false. In the query callback: if it's already set, do
//    nothing (query can't stop early). Otherwise, if segments_intersect(trimmed line, that wall)
//    is true, set it.
// 4. Return the opposite of the flag.
bool PolygonBackend::line_of_sight(Vec2 a, Vec2 b) const {
}