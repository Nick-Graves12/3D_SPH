#include "Vec3.h"
#include <cassert>
#include <cmath>

Vec3 add(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 subtract(const Vec3& a, const Vec3& b)
{
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 multiply(const Vec3& a, float s)
{
    return Vec3{a.x * s, a.y * s, a.z * s};
}

Vec3 divide(const Vec3& a, float s)
{
    assert(s != 0.0f);
    return Vec3{a.x / s, a.y / s, a.z / s};
}

float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float squaredMagnitude(const Vec3& a)
{
    return dot(a, a);
}

float length(const Vec3& a)
{
    return std::sqrt(squaredMagnitude(a));
}

Vec3 normalize(const Vec3& a)
{
    float storedLength = length(a);
    assert(storedLength > 1e-6f);
    return divide(a, storedLength);
}
