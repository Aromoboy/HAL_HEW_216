#pragma once

struct float2
{
        float x;
        float y;

        float2 operator+(const float2 other) const
        {
            return float2(this->x + other.x, this->y + other.y);
        }
};

struct float3
{
        float x;
        float y;
        float z;
};

