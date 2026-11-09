#pragma once

struct Vec2 {
    float x;
    float y;
};

Vec2 operator+(const Vec2&, const Vec2&);
Vec2 operator-(const Vec2&, const Vec2&);
Vec2 operator-(const Vec2&);
Vec2 operator*(const Vec2&, float);
Vec2 operator*(float, const Vec2&);

namespace Math {
    float dot(const Vec2&, const Vec2&);
    float cross(const Vec2&, const Vec2&);
    float length(const Vec2&);
    float distance(const Vec2&, const Vec2&);
}