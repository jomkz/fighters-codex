#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "platform/math3d.h"

using platform::Mat4;
using Catch::Matchers::WithinAbs;

static void Transform(const Mat4& m, float x, float y, float z, float out[4]) {
    const float v[4] = {x, y, z, 1.0f};
    platform::mat4_transform(m, v, out);
}

TEST_CASE("identity leaves vectors unchanged", "[gui][math3d]") {
    float out[4];
    Transform(platform::mat4_identity(), 3.0f, -2.0f, 7.0f, out);
    CHECK_THAT(out[0], WithinAbs(3.0f, 1e-6f));
    CHECK_THAT(out[1], WithinAbs(-2.0f, 1e-6f));
    CHECK_THAT(out[2], WithinAbs(7.0f, 1e-6f));
    CHECK_THAT(out[3], WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("mul composes right-to-left", "[gui][math3d]") {
    // a scales x by 2 (diagonal), b translates x by +1: (a*b)v = a(b(v)).
    Mat4 a = platform::mat4_identity();
    a.m[0] = 2.0f;
    Mat4 b = platform::mat4_identity();
    b.m[12] = 1.0f;

    float out[4];
    Transform(platform::mat4_mul(a, b), 1.0f, 0.0f, 0.0f, out);
    CHECK_THAT(out[0], WithinAbs(4.0f, 1e-6f)); // (1+1)*2

    Transform(platform::mat4_mul(b, a), 1.0f, 0.0f, 0.0f, out);
    CHECK_THAT(out[0], WithinAbs(3.0f, 1e-6f)); // 1*2+1
}

TEST_CASE("look_at maps the target onto the -Z axis (right-handed)", "[gui][math3d]") {
    const float eye[3]    = {0.0f, 0.0f, 5.0f};
    const float target[3] = {0.0f, 0.0f, 0.0f};
    const float up[3]     = {0.0f, 1.0f, 0.0f};
    Mat4 view = platform::mat4_look_at(eye, target, up);

    float out[4];
    Transform(view, 0.0f, 0.0f, 0.0f, out);       // target
    CHECK_THAT(out[0], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(out[1], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(out[2], WithinAbs(-5.0f, 1e-5f));  // in front of the camera

    Transform(view, 0.0f, 0.0f, 5.0f, out);       // the eye itself
    CHECK_THAT(out[0], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(out[1], WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(out[2], WithinAbs(0.0f, 1e-5f));

    Transform(view, 1.0f, 2.0f, 5.0f, out);       // +X stays right, +Y stays up
    CHECK_THAT(out[0], WithinAbs(1.0f, 1e-5f));
    CHECK_THAT(out[1], WithinAbs(2.0f, 1e-5f));
}

TEST_CASE("look_at upper 3x3 is orthonormal", "[gui][math3d]") {
    const float eye[3]    = {12.0f, -3.0f, 7.5f};
    const float target[3] = {-1.0f, 4.0f, 2.0f};
    const float up[3]     = {0.0f, 1.0f, 0.0f};
    Mat4 v = platform::mat4_look_at(eye, target, up);

    // Rows of the rotation block (r0=right, r1=up, r2=back) — unit length,
    // mutually orthogonal.
    const float r[3][3] = {{v.m[0], v.m[4], v.m[8]},
                           {v.m[1], v.m[5], v.m[9]},
                           {v.m[2], v.m[6], v.m[10]}};
    for (int i = 0; i < 3; ++i) {
        float len2 = r[i][0]*r[i][0] + r[i][1]*r[i][1] + r[i][2]*r[i][2];
        CHECK_THAT(len2, WithinAbs(1.0f, 1e-5f));
        for (int j = i + 1; j < 3; ++j) {
            float dot = r[i][0]*r[j][0] + r[i][1]*r[j][1] + r[i][2]*r[j][2];
            CHECK_THAT(dot, WithinAbs(0.0f, 1e-5f));
        }
    }
}

TEST_CASE("perspective maps near/far planes to GL clip z = -1/+1", "[gui][math3d]") {
    // fovY 90°, aspect 1, near 1, far 3.
    Mat4 p = platform::mat4_perspective(3.14159265f / 2.0f, 1.0f, 1.0f, 3.0f);

    float out[4];
    Transform(p, 0.0f, 0.0f, -1.0f, out);         // point on the near plane
    CHECK_THAT(out[2] / out[3], WithinAbs(-1.0f, 1e-5f));

    Transform(p, 0.0f, 0.0f, -3.0f, out);         // point on the far plane
    CHECK_THAT(out[2] / out[3], WithinAbs(1.0f, 1e-5f));

    Transform(p, 1.0f, 0.0f, -1.0f, out);         // 45° edge ray at near
    CHECK_THAT(out[0] / out[3], WithinAbs(1.0f, 1e-5f));

    CHECK_THAT(out[3], WithinAbs(1.0f, 1e-5f));   // w = -z_view
}
