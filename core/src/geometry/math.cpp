#include "arcane/core/geometry/math.hpp"

Vec2 operator+(const Vec2& a, const Vec2& b)
{
    return { a.x + b.x, a.y + b.y };
}

Vec2 operator-(const Vec2& a, const Vec2& b)
{
    return { a.x - b.x, a.y - b.y };
}

Vec2 operator-(const Vec2& v)
{
    return { -v.x, -v.y };
}

Vec2 operator*(const Vec2& v, float scalar)
{
    return { v.x * scalar, v.y * scalar };
}

Vec2 operator*(float scalar, const Vec2& v)
{
    return v * scalar;
}

float Math::dot(const Vec2& a, const Vec2& b)
{
    return a.x * b.x + a.y * b.y;
}

float Math::cross(const Vec2& a, const Vec2& b)
{
    return a.x * b.y - a.y * b.x;
}

float Math::length(const Vec2& v)
{
    return std::hypot(v.x, v.y);
}

float Math::distance(const Vec2& a, const Vec2& b)
{
    return std::hypot(b.x - a.x, b.y - a.y);
}