#include "arcane\core\geometry\segment.hpp"

std::optional<float> ray_segment_hit(Vec2 origin, Vec2 dir, const Segment& s)
{

}

bool segments_intersect(const Segment& s, const Segment& t, float epsilon)
{
    // Check for degenerate segments (points)
    bool sPoint = Math::distance(s.a, s.b) <= epsilon;
    bool tPoint = Math::distance(t.a, t.b) <= epsilon;

    // Handle two points
    if (sPoint && tPoint)
        return Math::distance(s.a, t.a) <= epsilon;

    // Check whether point lies on segment
    auto point_on_segment = [&](const Vec2& p, const Segment& seg)
        {
            Vec2 d = seg.b - seg.a;
            Vec2 v = p - seg.a;

            float len = Math::length(d);

            // Check perpendicular distance from line
            if (std::fabs(Math::cross(d, v)) > epsilon * len)
                return false;

            // Check whether point lies within segment bounds
            float projection = Math::dot(v, d);

            return projection >= -epsilon * len &&
                projection <= Math::dot(d, d) + epsilon * len;
        };

    if (sPoint) return point_on_segment(s.a, t);
    if (tPoint) return point_on_segment(t.a, s);

    // Calculate line 1 coefficients: c1 = a1*x + b1*y
    float a1 = s.b.y - s.a.y;
    float b1 = s.a.x - s.b.x;
    float c1 = a1 * s.a.x + b1 * s.a.y;

    // Calculate line 2 coefficients: c2 = a2*x + b2*y
    float a2 = t.b.y - t.a.y;
    float b2 = t.a.x - t.b.x;
    float c2 = a2 * t.a.x + b2 * t.a.y;

    float det = a1 * b2 - a2 * b1;

    float detScale = Math::length({ a1, b1 }) *
        Math::length({ a2, b2 });

    if (std::fabs(det) <= epsilon * detScale)
    {
        // Lines are parallel; check whether they are collinear
        if (std::fabs(a1 * t.a.x + b1 * t.a.y - c1) > epsilon * Math::length({ a1, b1 }))
            return false;

        // Check whether bounding boxes overlap
        return !(std::max(std::min(s.a.x, s.b.x), std::min(t.a.x, t.b.x)) >
            std::min(std::max(s.a.x, s.b.x), std::max(t.a.x, t.b.x)) + epsilon ||
            std::max(std::min(s.a.y, s.b.y), std::min(t.a.y, t.b.y)) >
            std::min(std::max(s.a.y, s.b.y), std::max(t.a.y, t.b.y)) + epsilon);
    }

    // Calculate intersection numerators
    float xNum = b2 * c1 - b1 * c2;
    float yNum = a1 * c2 - a2 * c1;

    // Calculate bounding box of segment s
    float minX1 = std::min(s.a.x, s.b.x);
    float maxX1 = std::max(s.a.x, s.b.x);
    float minY1 = std::min(s.a.y, s.b.y);
    float maxY1 = std::max(s.a.y, s.b.y);

    // Calculate bounding box of segment t
    float minX2 = std::min(t.a.x, t.b.x);
    float maxX2 = std::max(t.a.x, t.b.x);
    float minY2 = std::min(t.a.y, t.b.y);
    float maxY2 = std::max(t.a.y, t.b.y);

    // Normalize determinant
    if (det < 0)
    {
        det = -det;
        xNum = -xNum;
        yNum = -yNum;
    }

    // Convert coordinate tolerance into numerator tolerance
    float tol = epsilon * det;

    // Check whether intersection lies within both segments
    return xNum >= (minX1 - epsilon) * det - tol &&
        xNum <= (maxX1 + epsilon) * det + tol &&
        yNum >= (minY1 - epsilon) * det - tol &&
        yNum <= (maxY1 + epsilon) * det + tol &&
        xNum >= (minX2 - epsilon) * det - tol &&
        xNum <= (maxX2 + epsilon) * det + tol &&
        yNum >= (minY2 - epsilon) * det - tol &&
        yNum <= (maxY2 + epsilon) * det + tol;
}