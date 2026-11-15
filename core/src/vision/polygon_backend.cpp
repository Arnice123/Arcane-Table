#include "arcane\core\vision\polygon_backend.hpp"
#include "arcane/core/geometry/segment.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>
#include <ranges>

namespace {
    constexpr float kPi = 3.14159265358979f;

    // How far (radians) the two side rays sit either side of each corner.
    // The side ray misses the corner by about (distance to corner * kAngleEps). 
    // That gap must be clearly bigger than ray_segment_hit's epsilon, 
    // or the side ray still "hits" the wall and you can't see round the corner. 
    constexpr float kAngleEps = 1e-4f;

    const float kAngleEpsSin = std::sin(kAngleEps);
    const float kAngleEpsCos = std::cos(kAngleEps);

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
        float t = std::clamp(Math::dot(p - s.a, d) / len2, 0.0f, 1.0f);
        Vec2 closestPoint = s.a + d * t;

        // calculate so diff.x = p.x - cPoint.x (same w/ y)
        Vec2 diff = p - closestPoint;

        return Math::dot(diff, diff);
    }

    // Everything the fast ray test needs about one wall, for one viewer.
    // Built once per nearby wall in step 1 of compute(), then reused for every ray.
    struct WallRecord {
        Vec2 start; // the wall's first endpoint (a)
        Vec2 end;           
        Vec2 dir; // b - a (not normalised)
        float len; // length of dir (0 for a zero-length wall)
        Vec2 toViewer; // viewer origin - start
        float tNum; // distance numerator: cross(dir, toViewer). Same for every ray from this viewer
        float minDist2; // squared distance from the viewer to the wall
    };

    // In:  a wall and the viewer's origin.
    // Out: that wall's record for this viewer.
    WallRecord makeWallRecord(const Segment& s, Vec2 viewer, WallStatic stat, float minDist2) {
        Vec2 toViewer = viewer - s.a;
        return WallRecord{ s.a, s.b, stat.dir, stat.len, toViewer, Math::cross(stat.dir, toViewer), minDist2 };
    }

    // In:  a UNIT ray direction, a wall record, the nearest hit distance found so far
    // Out: the distance to the hit if the ray hits this wall closer than nearest, otherwise nothing.
    std::optional<float> rayHitCloser(Vec2 dir, const WallRecord& w, float nearest, float epsilon = 1e-5f)
    {
        // wall's len must be > 0
        if (w.len < epsilon) return std::nullopt;

        // lines are parallel
        float denom = Math::cross(dir, w.dir);
        if (std::fabs(denom) < epsilon * w.len) return std::nullopt;

        // distance along the ray (numerator): same for every ray, so copy it from the record
        float tNum = w.tNum;

        // position along the wall (numerator): depends on this ray, so compute it here
        float uNum = Math::cross(dir, w.toViewer);

        // make sure denom is positive for easier math: flip all three together
        if (denom < 0)
        {
            denom = -denom;
            tNum = -tNum;
            uNum = -uNum;
        }

        // make sure in front of viewer (uses the distance numerator)
        if (tNum < -epsilon * denom) return std::nullopt;

        // on the wall (uses the wall-position numerator)
        float uLen = uNum * w.len;
        if (uLen < -epsilon * denom || uLen >(w.len + epsilon) * denom) return std::nullopt;

        // will this improve nearest (distance numerator)
        if (tNum >= nearest * denom) return std::nullopt;

        // return distance (distance numerator)
        return std::max(0.0f, tNum / denom);
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

    class PolygonField final : public VisibilityField {
    public:
        // In:  the viewer's origin and radius, and the polygon points (already in angle order).
        // Outcome: all three are stored (take the points by moving them in).
        PolygonField(Vec2 origin, float radius, std::vector<Vec2> pts) : origin_(origin), radius_(radius), pts_(std::move(pts)){ }

        // In:  a point p.
        // Out: true if p is inside the visible polygon.
        //       3. return the flag.
        bool contains(Vec2 point) const override {
            // Reject points outside the radius.
            if (Math::distance_squared(point, origin_) > radius_ * radius_) return false;

            // go around testing pairs of edges
            bool inside = false;
            for (std::size_t i = 0; i < pts_.size(); ++i)
            {
                const Vec2& a = pts_[i];
                const Vec2& b = pts_[(i + 1) % pts_.size()];
                if ((a.y > point.y) == (b.y > point.y)) continue;

                // Find x-coordinate where edge intersects point.y.
                float x = a.x + (point.y - a.y) / (b.y - a.y) * (b.x - a.x);

                // Intersection to the right means ray crossed edge
                if (point.x < x) inside = !inside;
            }
            return inside;
        }

        // Out: a copy of the polygon points.
        std::vector<Vec2> outline() const override {
            return pts_;
        }

    private:
        Vec2 origin_;
        float radius_;
        std::vector<Vec2> pts_;
    };

}  // namespace

// Out: the backend's short name, "polygon".
std::string_view PolygonBackend::name() const { return "polygon"; }

// In:  every wall in the map.
// Outcome: walls_ holds a copy of them and index_ is built over that copy.
void PolygonBackend::set_occluders(std::span<const Segment> walls) {
    walls_.assign(walls.begin(), walls.end());
    wallStatic_.clear();
    wallStatic_.reserve(walls.size());
    for (const auto& w : walls_) {
        Vec2 dir = w.b - w.a;
        wallStatic_.push_back({ dir, Math::length(dir) });
    }
    index_.bulkLoad(walls_);
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
//    - unit direction from the angle (cos, sin); it must be unit length so that ray_segment_hit's result is a distance
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
    std::vector<WallRecord> nearbyWalls;
    float radius2 = viewer.radius * viewer.radius;
    index_.query(rstar::Rect::fromSegment(Segment{ Vec2{viewer.origin - viewer.radius}, Vec2{viewer.origin + viewer.radius} }), [&](const rstar::Rect&, const std::uint32_t& i) {
        float dist2 = distToSegment2(viewer.origin, walls_[i]);
        if (dist2 <= radius2) nearbyWalls.push_back(makeWallRecord(walls_[i], viewer.origin, wallStatic_[i], dist2));
        });
    std::sort(nearbyWalls.begin(), nearbyWalls.end(), [](const auto& a, const auto& b) {return a.minDist2 < b.minDist2; });

    std::vector<Vec2> targetPoints;
    for (const auto& w : nearbyWalls) {
        Vec2 diffA = w.start - viewer.origin;
        Vec2 diffB = w.end - viewer.origin;
        if (Math::dot(diffA, diffA) <= radius2) targetPoints.push_back(w.start);
        if (Math::dot(diffB, diffB) <= radius2) targetPoints.push_back(w.end);
        circleCrossings(Segment{ w.start, w.end}, viewer.origin, viewer.radius, targetPoints);
    }

    std::sort(targetPoints.begin(), targetPoints.end(), [](const auto& a, const auto& b) {return std::tie(a.x, a.y) < std::tie(b.x, b.y); });
    auto new_end = std::unique(targetPoints.begin(), targetPoints.end());
    targetPoints.erase(new_end, targetPoints.end());

    const float step = 2 * kPi / kArcSamples;
    std::vector<std::pair<Vec2, float>> rays;
    rays.reserve(kArcSamples + 3 * targetPoints.size());
    auto calcAngle = [&](float curAngle) {return std::pair<Vec2, float>{ Vec2{ std::cos(curAngle), std::sin(curAngle) }, curAngle };};
    for (int i = 0; i < kArcSamples; ++i)
    {
        rays.push_back(calcAngle(i * step - kPi));
    }
    
    for (const auto& w : targetPoints) {
        float curAngle = std::atan2(w.y - viewer.origin.y, w.x - viewer.origin.x);
        float cosAngle = std::cos(curAngle);
        float sinAngle = std::sin(curAngle);
        rays.push_back(std::pair<Vec2, float>(Vec2{cosAngle, sinAngle}, curAngle));
        rays.push_back(std::pair<Vec2, float>(Vec2{cosAngle * kAngleEpsCos - sinAngle * kAngleEpsSin, cosAngle * kAngleEpsSin + sinAngle * kAngleEpsCos }, curAngle + kAngleEps));
        rays.push_back(std::pair<Vec2, float>(Vec2{cosAngle * kAngleEpsCos + sinAngle * kAngleEpsSin, sinAngle * kAngleEpsCos - cosAngle * kAngleEpsSin }, curAngle - kAngleEps));
    }

    std::sort(rays.begin(), rays.end(), [](const auto& a, const auto& b) { return a.second < b.second; });

    std::vector<Vec2> hitPoints;
    for (const auto& ray : rays) {
        float nearestHit = viewer.radius;
        for (const auto& w : nearbyWalls) {
            if (w.minDist2 > nearestHit * nearestHit) break;
            std::optional<float> rayHit = rayHitCloser(ray.first, w, nearestHit);
            if (!rayHit) continue;
            nearestHit = rayHit.value();
        }        
        hitPoints.push_back(viewer.origin + nearestHit * ray.first);
    }
    std::unique_ptr<PolygonField> f = std::make_unique<PolygonField>(viewer.origin, viewer.radius, std::move(hitPoints));
    return f;
}

// In:  two points a and b.
// Out: true if no wall blocks the straight line between them.
bool PolygonBackend::line_of_sight(Vec2 a, Vec2 b) const {
    // make sure that the points aren't right beside each other
    float l2 = Math::distance_squared(a, b);
    if (l2 < 4 * kSightTrim * kSightTrim) return true;

    // pull the points by kSightTrim towards each other
    Vec2 offset = (b - a) * (kSightTrim / std::sqrt(l2));
    Segment sightLine{ a + offset, b - offset };

    bool blocked = false;
    // if the segment intersects between the wall and sightline then something is in the way
    index_.query(rstar::Rect::fromSegment(sightLine), [&](const rstar::Rect&, const std::uint32_t& i) {
        if (blocked) return;
        if (segments_intersect(sightLine, walls_[i])) blocked = true;
        });

    return !blocked;
}