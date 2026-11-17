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

        PolygonField(Vec2 origin, float radius, std::vector<Vec2> pts, std::vector<float> angles) : origin_(origin), radius_(radius), pts_(std::move(pts)), angles_(std::move(angles)){ }

        // In:  a point p.
        // Out: true if p is inside the visible polygon.
        bool contains(Vec2 point) const override {
            // ensure we have enough angles
            if (angles_.size() < 3) return false;

            // Reject points outside the radius.
            if (Math::distance_squared(point, origin_) > radius_ * radius_) return false;

            // compute angle between point and origin
            float pAngle = std::atan2(point.y - origin_.y, point.x - origin_.x);

            // find the angle in the list of angles
            auto it = std::upper_bound(angles_.begin(), angles_.end(), pAngle);
            
            // if its at the end/begin set start = angle size, end = 0 otherwise start = i - 1, end = i
            std::size_t start, end;
            if (it == angles_.end() || it == angles_.begin()) { start = angles_.size() - 1; end = 0; }
            else { start = std::distance(angles_.begin(), it) - 1; end = start + 1; }
            point = point - origin_;
            // is the point left or right of the edge
            return (Math::cross(pts_[end] - pts_[start], point - pts_[start]) >= 0);
            /*
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
            return inside;*/
        }

        // Out: a copy of the polygon points.
        std::vector<Vec2> outline() const override {
            std::vector<Vec2> ptCpy;
            ptCpy.reserve(pts_.size());
            for (const auto& p : pts_) {
                ptCpy.push_back(p + origin_);
            }
            return ptCpy;
        }

    private:
        Vec2 origin_;
        float radius_;
        std::vector<Vec2> pts_;
        std::vector<float> angles_;
    };

}  // namespace

// Out: the backend's short name, "polygon".
std::string_view PolygonBackend::name() const { return "polygon"; }

// In:  every wall in the map.
// Outcome: walls_ holds a copy of them and index_ is built over that copy.
void PolygonBackend::set_occluders(std::span<const Segment> walls) {
    // import in the list of walls
    walls_.assign(walls.begin(), walls.end());
    wallStatic_.clear();
    wallStatic_.reserve(walls.size());
    for (const auto& w : walls_) {
        Vec2 dir = w.b - w.a;
        wallStatic_.push_back({ dir, Math::length(dir) });
    }
    index_.bulkLoad(walls_);

    // compute what walls overlap
    std::vector<std::vector<std::pair<float, Vec2>>> cutWallsList;
    cutWallsList.resize(walls_.size());

    // for each wall
    for (std::uint32_t i = 0; i < walls_.size(); ++i) {
        // query which walls overlap
        index_.query(rstar::Rect::fromSegment(walls_[i]), [&](const rstar::Rect& box, const std::uint32_t& j) {
            if (j <= i) return; // test each pair once

            // if denomerator really small ignore
            float denom = Math::cross(wallStatic_[i].dir, wallStatic_[j].dir);
            if (std::fabs(denom) <= 1e-5 * wallStatic_[i].len * wallStatic_[j].len) return;
            denom = 1 / denom;

            // calculate s & t
            Vec2 w = walls_[j].a - walls_[i].a;
            float s = Math::cross(w, wallStatic_[j].dir) * denom;
            float t = Math::cross(w, wallStatic_[i].dir) * denom;

            // ensure between 0 < s, t < 1
            if (s < 1e-5 || t < 1e-5 || s > 1 - 1e-5 || t > 1 - 1e-5) return;

            // calculate colision point = start + s * dir
            Vec2 cPoint = walls_[i].a + s * wallStatic_[i].dir;
            cutWallsList[i].push_back(std::pair<float, Vec2>{s, cPoint});
            cutWallsList[j].push_back(std::pair<float, Vec2>{t, cPoint});
            });
    }

    std::vector<Segment> newWalls;
    newWalls.reserve(walls_.size());
    for (std::size_t i = 0; i < cutWallsList.size(); ++i) {
        // if there are no intersections keep as is
        if (cutWallsList[i].size() == 0) {
            newWalls.push_back(walls_[i]);
            continue;
        }
        // sort by colision point
        std::sort(cutWallsList[i].begin(), cutWallsList[i].end(), [](const auto& a, const auto& b) {return a.first < b.first;});

        // walk through constructing lines that aren't just points
        Vec2 start = walls_[i].a;      
        for (std::size_t j = 0; j < cutWallsList[i].size(); ++j) {
            Segment newSeg{ start, cutWallsList[i][j].second };
            if (Math::distance_squared(newSeg.a, newSeg.b) > 1e-10) {
                newWalls.push_back(newSeg); 
                start = cutWallsList[i][j].second;
            }
        }

        // add the last endpoint
        Segment newSeg{ start, walls_[i].b};
        if (Math::distance_squared(newSeg.a, newSeg.b) > 1e-10) { // make sure its not a point
            newWalls.push_back(newSeg);
        }
        else if (start != walls_[i].a) // if it is a point overwrite the last one
        {
            std::size_t endIndx = newWalls.size() - 1;
            newWalls[endIndx] = Segment{ newWalls[endIndx].a, walls_[i].b };
        }
        else { // if we haven't written any then just push the old wall
            newWalls.push_back(walls_[i]);
        }
    }

    // update walls and tree
    walls_.assign(newWalls.begin(), newWalls.end());
    wallStatic_.clear();
    wallStatic_.reserve(walls_.size());
    for (const auto& w : walls_) {
        Vec2 dir = w.b - w.a;
        wallStatic_.push_back({ dir, Math::length(dir) });
    }
    index_.bulkLoad(walls_);
}

// In:  a viewer (origin, radius).
// Out: a PolygonField describing what the viewer can see.
std::unique_ptr<VisibilityField> PolygonBackend::compute(const Viewer& viewer) const {
    // find walls close enough to matter, make sure they are in the circle
    std::vector<WallRecord> nearbyWalls;
    float radius2 = viewer.radius * viewer.radius;
    index_.query(rstar::Rect::fromSegment(Segment{ Vec2{viewer.origin - viewer.radius}, Vec2{viewer.origin + viewer.radius} }), [&](const rstar::Rect&, const std::uint32_t& i) {
        float dist2 = distToSegment2(viewer.origin, walls_[i]);
        if (dist2 <= radius2) nearbyWalls.push_back(makeWallRecord(walls_[i], viewer.origin, wallStatic_[i], dist2));
        });

    // sort nearest first, so the ray loop can stop once no remaining wall can be closer
    std::sort(nearbyWalls.begin(), nearbyWalls.end(), [](const auto& a, const auto& b) {return a.minDist2 < b.minDist2; });

    // collect the points the outline can turn at: endpoints inside the circle and where walls cross the circle
    std::vector<Vec2> targetPoints;
    for (const auto& w : nearbyWalls) {
        Vec2 diffA = w.start - viewer.origin;
        Vec2 diffB = w.end - viewer.origin;
        if (Math::dot(diffA, diffA) <= radius2) targetPoints.push_back(w.start);
        if (Math::dot(diffB, diffB) <= radius2) targetPoints.push_back(w.end);
        circleCrossings(Segment{ w.start, w.end }, viewer.origin, viewer.radius, targetPoints);
    }

    // remove duplicate targets (corners shared by two walls), so each corner only gets one set of rays
    std::sort(targetPoints.begin(), targetPoints.end(), [](const auto& a, const auto& b) {return std::tie(a.x, a.y) < std::tie(b.x, b.y); });
    auto new_end = std::unique(targetPoints.begin(), targetPoints.end());
    targetPoints.erase(new_end, targetPoints.end());

    // evenly spaced rays around the full circle, from -pi, so the radius edge is drawn as an arc. each ray is (unit direction, angle)
    const float step = 2 * kPi / kArcSamples;
    std::vector<std::pair<Vec2, float>> rays;
    rays.reserve(kArcSamples + 3 * targetPoints.size());
    auto calcAngle = [&](float curAngle) {return std::pair<Vec2, float>{ Vec2{ std::cos(curAngle), std::sin(curAngle) }, curAngle };};
    for (int i = 0; i < kArcSamples; ++i)
    {
        rays.push_back(calcAngle(i * step - kPi));
    }

    // three rays per target: straight at it, and rotated by +/- kAngleEps to see just past each side of the corner
    for (const auto& w : targetPoints) {
        // direction to the target, skipping targets at the viewer's own position (no direction)
        Vec2 offset(w.x - viewer.origin.x, w.y - viewer.origin.y);
        float sqrLen = Math::length_squared(offset);
        if (sqrLen < 1e-10) continue;
        float len = 1 / std::sqrt(sqrLen);
        Vec2 dir(offset.x * len, offset.y * len);

        // the angle is only needed to sort the rays
        float curAngle = std::atan2(offset.y, offset.x);
        rays.push_back(std::pair<Vec2, float>(Vec2{ dir.x, dir.y }, curAngle));
        rays.push_back(std::pair<Vec2, float>(Vec2{ dir.x * kAngleEpsCos - dir.y * kAngleEpsSin, dir.x * kAngleEpsSin + dir.y * kAngleEpsCos }, curAngle + kAngleEps));
        rays.push_back(std::pair<Vec2, float>(Vec2{ dir.x * kAngleEpsCos + dir.y * kAngleEpsSin, dir.y * kAngleEpsCos - dir.x * kAngleEpsSin }, curAngle - kAngleEps));
    }

    // sort by angle so the hit points come out in order around the viewer
    std::sort(rays.begin(), rays.end(), [](const auto& a, const auto& b) { return a.second < b.second; });

    // cast each ray: nearest wall hit, or the radius if nothing is closer
    std::vector<Vec2> hitPoints;
    std::vector<float> angles;
    hitPoints.reserve(rays.size());
    angles.reserve(rays.size());
    for (const auto& ray : rays) {
        float nearestHit = viewer.radius;
        for (const auto& w : nearbyWalls) {
            // walls are sorted by distance, so once one is further than the current hit, all the rest are too
            if (w.minDist2 > nearestHit * nearestHit) break;
            std::optional<float> rayHit = rayHitCloser(ray.first, w, nearestHit);
            if (!rayHit) continue;
            nearestHit = rayHit.value();
        }

        // store the hit relative to the viewer (keeps float precision when walls are very close)
        hitPoints.push_back(nearestHit * ray.first);
        angles.push_back(ray.second);
    }

    // the hit points, in angle order, are the polygon
    std::unique_ptr<PolygonField> f = std::make_unique<PolygonField>(viewer.origin, viewer.radius, std::move(hitPoints), std::move(angles));
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

    // if the segment intersects between the wall and sightline then something is in the way
    bool blocked = index_.anySegment(sightLine, [&](const rstar::Rect&, const std::uint32_t& i) -> bool {
        if (segments_intersect(sightLine, walls_[i])) return true;
        return false;
        });

    return !blocked;
}