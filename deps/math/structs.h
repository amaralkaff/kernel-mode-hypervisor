#pragma once

class vec4 {
public:
    float x, y, z, w;
};

struct vec3
{
    float x, y, z;

    vec3() : x(0), y(0), z(0) {}  // Add this line
    vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    inline float operator[](int i) const {
        return ((float*)this)[i];
    }

    inline vec3& operator-=(float v) {
        x -= v; y -= v; z -= v; return *this;
    }

    inline vec3 operator*(float v) const {
        return vec3(x * v, y * v, z * v);
    }

    inline vec3 operator/(float v) const
    {
        return vec3(x / v, y / v, z / v);
    }

    inline vec3& operator+=(const vec3& v) {
        x += v.x; y += v.y; z += v.z; return *this;
    }

    inline vec3 operator-(const vec3& v) const {
        return vec3(x - v.x, y - v.y, z - v.z);
    }

    inline vec3 operator+(const vec3& v) const {
        return vec3(x + v.x, y + v.y, z + v.z);
    }

    inline bool operator==(const vec3& v) const {
        return x == v.x && y == v.y && z == v.z;
    }

    inline void normalize()
    {
        if (x > 89.0f) x = 89.0f;
        if (x < -89.0f) x = -89.0f;
        while (y > 180.f) y -= 360.f;
        while (y < -180.f) y += 360.f;
    }
};

struct vec2
{
    float x, y;

    vec2() : x(0), y(0) {}  // Add this line
    vec2(float x, float y) : x(x), y(y) {}
};

struct Matrix4x4 {
    union {
        struct {
            float        _11, _12, _13, _14;
            float        _21, _22, _23, _24;
            float        _31, _32, _33, _34;
            float        _41, _42, _43, _44;

        }; float m[4][4];
    };
};

#define Assert( _exp ) ((void)0)

struct Matrix {
    float matrix[16];
};

struct matrix3x4_t
{
    matrix3x4_t() {}
    matrix3x4_t(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23)
    {
        m_flMatVal[0][0] = m00;	m_flMatVal[0][1] = m01; m_flMatVal[0][2] = m02; m_flMatVal[0][3] = m03;
        m_flMatVal[1][0] = m10;	m_flMatVal[1][1] = m11; m_flMatVal[1][2] = m12; m_flMatVal[1][3] = m13;
        m_flMatVal[2][0] = m20;	m_flMatVal[2][1] = m21; m_flMatVal[2][2] = m22; m_flMatVal[2][3] = m23;
    }

    float* operator[](int i) { Assert((i >= 0) && (i < 3)); return m_flMatVal[i]; }
    const float* operator[](int i) const { Assert((i >= 0) && (i < 3)); return m_flMatVal[i]; }
    float* Base() { return &m_flMatVal[0][0]; }
    const float* Base() const { return &m_flMatVal[0][0]; }

    float m_flMatVal[3][4];
};

struct view_matrix_t
{
    float matrix[16];
};