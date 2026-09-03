#pragma once

struct Vec3
{
    float x;
    float y;
    float z;
};

Vec3 add(const Vec3& a, const Vec3& b);
Vec3 subtract(const Vec3&a, const Vec3& b);
Vec3 multiply(const Vec3& a, float s);
Vec3 divide(const Vec3&a, float s);
float dot(const Vec3& a, const Vec3& b);
float squaredMagnitude(const Vec3& a);
float length(const Vec3& a);
Vec3 normalize(const Vec3& a);
