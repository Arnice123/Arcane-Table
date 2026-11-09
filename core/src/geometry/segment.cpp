#include "arcane/core/geometry/segment.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

std::optional<float> ray_segment_hit(Vec2 origin, Vec2 dir, const Segment& s, float epsilon)
{
    Vec2 v1 = origin - s.a;
    Vec2 v2 = s.b - s.a;
    Vec2 v3{ -dir.y, dir.x };

    float segLen = Math::length(v2);
    float dirLen = Math::length(dir);

    // Check for segment or ray with 0 len
    if (segLen <= epsilon || dirLen <= epsilon)
    {
        return std::nullopt;
    }

    float bot = Math::dot(v2, v3);

    // Ray and segment are parallel
    if (std::fabs(bot) <= epsilon * segLen * dirLen)
    {
        return std::nullopt;
    }

    // Calculate numerators (t1 = t1Num / bot, t2 = t2Num / bot)
    float t1Num = Math::cross(v2, v1);
    float t2Num = Math::dot(v1, v3);

    // Normalize denominator
    if (bot < 0)
    {
        bot = -bot;
        t1Num = -t1Num;
        t2Num = -t2Num;
    }

    // Scale numerators so tolerance is a distance
    float t1Scaled = t1Num * dirLen;
    float t2Scaled = t2Num * segLen;

    // Check whether hit is in front of ray and within segment
    if (t1Scaled >= -epsilon * bot && t2Scaled >= -epsilon * bot && t2Scaled <= (segLen + epsilon) * bot)
    {
        return t1Num / bot;
    }

    return std::nullopt;
}

bool segments_intersect(const Segment& s, const Segment& t, float epsilon)
{
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

    // Return if bounding boxes don't overlap
    if (maxX1 + epsilon < minX2 || maxX2 + epsilon < minX1 || 
        maxY1 + epsilon < minY2 || maxY2 + epsilon < minY1)
        return false;

    float epsSq = epsilon * epsilon;

    // Check whether point lies on segment
    auto point_on_segment = [&](const Vec2& p, const Segment& seg)
        {
            Vec2 d = seg.b - seg.a;
            Vec2 v = p - seg.a;

            float lenSq = Math::dot(d, d);

            // Handle segment is a point
            if (lenSq <= epsSq)
                return Math::dot(v, v) <= epsSq;

            // Find closest point on segment
            float projection = std::clamp(Math::dot(v, d) / lenSq, 0.0f, 1.0f);
            Vec2 diff = p - (seg.a + d * projection);

            return Math::dot(diff, diff) <= epsSq;
        };

    Vec2 sDir = s.b - s.a;
    Vec2 tDir = t.b - t.a;

    float sLenSq = Math::dot(sDir, sDir);
    float tLenSq = Math::dot(tDir, tDir);

    // Check to see if s or t is a point
    bool sPoint = sLenSq <= epsSq;
    bool tPoint = tLenSq <= epsSq;

    // Handles two points as well
    if (sPoint) return point_on_segment(s.a, t);
    if (tPoint) return point_on_segment(t.a, s);

    Vec2 v = t.a - s.a;
    float det = Math::cross(sDir, tDir);

    if (det * det <= epsSq * sLenSq * tLenSq)
    {
        // Check whether they are collinear
        float offset = Math::cross(sDir, v);

        // Bounding boxes already overlap, so collinear segments overlap
        return offset * offset <= epsSq * sLenSq;
    }

    // Calculate intersection numerators
    float sNum = Math::cross(v, tDir);
    float tNum = Math::cross(v, sDir);

    if (det < 0)
    {
        det = -det;
        sNum = -sNum;
        tNum = -tNum;
    }

    float sLen = std::sqrt(sLenSq);
    float tLen = std::sqrt(tLenSq);

    // Scale numerators so tolerance is a distance
    float sScaled = sNum * sLen;
    float tScaled = tNum * tLen;

    // Check whether intersection lies within both segments
    return sScaled >= -epsilon * det && sScaled <= (sLen + epsilon) * det &&
           tScaled >= -epsilon * det && tScaled <= (tLen + epsilon) * det;
}