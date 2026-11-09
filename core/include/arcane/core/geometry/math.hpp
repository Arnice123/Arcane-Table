#pragma once

#include <cmath>

struct Vec2 {
    float x;
    float y;
};

constexpr Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
constexpr Vec2 operator-(Vec2 a, Vec2 b) { return { a.x - b.x, a.y - b.y }; }
constexpr Vec2 operator-(Vec2 v) { return { -v.x, -v.y }; }
constexpr Vec2 operator*(Vec2 v, float s) { return { v.x * s, v.y * s }; }
constexpr Vec2 operator*(float s, Vec2 v) { return v * s; }
constexpr Vec2 operator/(Vec2 v, float s) { return { v.x / s, v.y / s }; }

constexpr bool operator==(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }

constexpr Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }
constexpr Vec2& operator-=(Vec2& a, Vec2 b) { a.x -= b.x; a.y -= b.y; return a; }
constexpr Vec2& operator*=(Vec2& v, float s) { v.x *= s; v.y *= s; return v; }

namespace Math
{
    constexpr float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
    constexpr float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
    constexpr float length_squared(Vec2 v) { return dot(v, v); }
    constexpr float distance_squared(Vec2 a, Vec2 b) { return length_squared(b - a); }
    inline float length(Vec2 v) { return std::sqrt(length_squared(v)); }
    inline float distance(Vec2 a, Vec2 b) { return length(b - a); }
}