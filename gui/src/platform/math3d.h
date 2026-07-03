#pragma once
#include <cmath>

namespace platform {

// Minimal column-major 4x4 float matrix for the SH preview, using OpenGL
// conventions throughout: right-handed look_at, clip-space z in [-1, 1]
// (do not transcribe Direct3D's [0, 1] convention), and column-major
// storage so uniforms upload without a transpose.
struct Mat4 {
    float m[16]; // m[col * 4 + row]
};

inline Mat4 mat4_identity() {
    Mat4 r = {};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

// r = a * b (b is applied to a vector first).
inline Mat4 mat4_mul(const Mat4& a, const Mat4& b) {
    Mat4 r = {};
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                r.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
    return r;
}

// out = m * v (4-component column vector).
inline void mat4_transform(const Mat4& m, const float v[4], float out[4]) {
    for (int row = 0; row < 4; ++row)
        out[row] = m.m[0 * 4 + row] * v[0] + m.m[1 * 4 + row] * v[1] +
                   m.m[2 * 4 + row] * v[2] + m.m[3 * 4 + row] * v[3];
}

// Right-handed view matrix (gluLookAt semantics: camera looks down -Z).
inline Mat4 mat4_look_at(const float eye[3], const float target[3],
                         const float up[3]) {
    float f[3] = {target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]};
    float fl   = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (fl > 0.0f) { f[0] /= fl; f[1] /= fl; f[2] /= fl; }

    float s[3] = {f[1] * up[2] - f[2] * up[1],
                  f[2] * up[0] - f[0] * up[2],
                  f[0] * up[1] - f[1] * up[0]};
    float sl   = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    if (sl > 0.0f) { s[0] /= sl; s[1] /= sl; s[2] /= sl; }

    float u[3] = {s[1] * f[2] - s[2] * f[1],
                  s[2] * f[0] - s[0] * f[2],
                  s[0] * f[1] - s[1] * f[0]};

    Mat4 r = mat4_identity();
    r.m[0] = s[0];  r.m[4] = s[1];  r.m[8]  = s[2];
    r.m[1] = u[0];  r.m[5] = u[1];  r.m[9]  = u[2];
    r.m[2] = -f[0]; r.m[6] = -f[1]; r.m[10] = -f[2];
    r.m[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
    r.m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    r.m[14] =  (f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
    return r;
}

// GL perspective projection (gluPerspective semantics, clip z in [-1, 1]).
inline Mat4 mat4_perspective(float fovYRad, float aspect, float znear,
                             float zfar) {
    float f = 1.0f / std::tan(fovYRad * 0.5f);
    Mat4 r  = {};
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (zfar + znear) / (znear - zfar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
    return r;
}

} // namespace platform
