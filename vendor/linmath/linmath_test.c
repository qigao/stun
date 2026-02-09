#include "tinytest.h"
#include "linmath.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define LINMATH_EPS 0.0001f

// Helper macro for float comparison
#define check_close(a, b) do { \
    float _a = (float)(a); \
    float _b = (float)(b); \
    float _diff = fabsf(_a - _b); \
    if (_diff >= LINMATH_EPS) { \
        printf("DEBUG FAIL: a=%g b=%g diff=%g EPS=%f\n", (double)_a, (double)_b, (double)_diff, (double)LINMATH_EPS); \
        check(0, "Mismatch: Actual: %.10f, Expected: %.10f, Diff: %.10f", (double)_a, (double)_b, (double)_diff); \
    } \
} while(0)

#define VEC2_SET(v, val) do { v[0]=val; v[1]=val; } while(0)
#define VEC3_SET(v, val) do { v[0]=val; v[1]=val; v[2]=val; } while(0)
#define VEC4_SET(v, val) do { v[0]=val; v[1]=val; v[2]=val; v[3]=val; } while(0)

#define VEC_CHECK_CLOSE(n, a, b) do { \
    for (int i=0; i<n; ++i) { \
        check_close(a[i], b[i]); \
    } \
} while(0)

spec("linmath") {
    
    describe("sanity") {
        it("should have working abs") {
            check(fabsf(1.0f) == 1.0f, "fabsf broken");
            check(fabsf(-1.0f) == 1.0f, "fabsf broken neg");
            check(fabsf(0.0f) == 0.0f, "fabsf broken zero");
            check(LINMATH_EPS == 0.0001f, "EPS broken");
            check(fabsf(0.000000f - 0.000000f) < LINMATH_EPS, "Basic zero check failure");
        }
    }

    describe("vec2") {
        it("should calculate inner product") {
            vec2 v;
            VEC2_SET(v, 1.0f);
            float inner = vec2_mul_inner(v, v);
            check_close(inner, 2.0f);
        }

        it("should calculate length") {
            vec2 v;
            VEC2_SET(v, 1.0f);
            float len = vec2_len(v);
            check_close(len, sqrtf(2.0f));
        }

        it("should normalize") {
            vec2 v = {3.0f, 4.0f};
            vec2 r;
            vec2_norm(r, v);
            check_close(vec2_len(r), 1.0f);
        }
    }

    describe("vec3") {
        it("should calculate inner product") {
            vec3 v;
            VEC3_SET(v, 1.0f);
            float inner = vec3_mul_inner(v, v);
            check_close(inner, 3.0f);
        }

        it("should calculate length") {
            vec3 v;
            VEC3_SET(v, 1.0f);
            float len = vec3_len(v);
            check_close(len, sqrtf(3.0f));
        }

        it("should normalize") {
            vec3 v = {1.0f, 2.0f, 3.0f};
            vec3 r;
            vec3_norm(r, v);
            check_close(vec3_len(r), 1.0f);
        }

        it("should calculate cross product") {
            vec3 v1 = {1.0f, 0.0f, 0.0f};
            vec3 v2 = {0.0f, 1.0f, 0.0f};
            vec3 r;
            vec3_mul_cross(r, v1, v2);
            vec3 expected = {0.0f, 0.0f, 1.0f};
            VEC_CHECK_CLOSE(3, r, expected);

            vec3_mul_cross(r, v1, v1);
            vec3 zero = {0.0f, 0.0f, 0.0f};
            VEC_CHECK_CLOSE(3, r, zero);
        }
    }

    describe("vec4") {
        it("should calculate inner product") {
            vec4 v;
            VEC4_SET(v, 1.0f);
            float inner = vec4_mul_inner(v, v);
            check_close(inner, 4.0f);
        }

        it("should calculate length") {
            vec4 v;
            VEC4_SET(v, 1.0f);
            float len = vec4_len(v);
            check_close(len, sqrtf(4.0f));
        }

        it("should normalize") {
            vec4 v = {1.0f, 2.0f, 3.0f, 4.0f};
            vec4 r;
            vec4_norm(r, v);
            check_close(vec4_len(r), 1.0f);
        }

         it("should calculate cross product (extended)") {
            vec4 v1 = {1.0f, 0.0f, 0.0f, 1.0f};
            vec4 v2 = {0.0f, 1.0f, 0.0f, 1.0f};
            vec4 r;
            vec4_mul_cross(r, v1, v2);
            vec4 expected = {0.0f, 0.0f, 1.0f, 1.0f};
            VEC_CHECK_CLOSE(4, r, expected);
        }
    }

    describe("matrix") {
        it("mat4x4 identity") {
            mat4x4 M;
            mat4x4_identity(M);
            for(int i=0; i<4; ++i) {
                for(int j=0; j<4; ++j) {
                    if (i == j) check_close(M[i][j], 1.0f);
                    else check_close(M[i][j], 0.0f);
                }
            }
        }
    }

    describe("quaternion") {
        it("should rotate correctly") {
            vec3 axis = {0.0f, 1.0f, 0.0f};
            quat q;
            float theta = (float)M_PI_4;
            quat_rotate(q, theta, axis);
            quat q_expected = {0.0f, sinf(theta/2.0f), 0.0f, cosf(theta/2.0f)};
            VEC_CHECK_CLOSE(4, q, q_expected);
        }

        it("should conjugate") {
            quat q, q_conj, q_ref;
            vec3 axis = {0.0f, 1.0f, 0.0f};
            float theta = 0.5f;
            quat_rotate(q, theta, axis);
            quat_conj(q_conj, q);
            quat_rotate(q_ref, -theta, axis);
            VEC_CHECK_CLOSE(4, q_conj, q_ref);
        }

        it("should rotate vec3") {
             quat q;
             vec3 axis = {0.0f, 1.0f, 0.0f};
             float theta = (float)(M_PI / 2.0); // 90 degrees
             quat_rotate(q, theta, axis);
             
             vec3 v = {1.0f, 0.0f, 0.0f};
             vec3 r;
             quat_mul_vec3(r, q, v);
             // Rotating (1,0,0) 90 deg around Y should be (0,0,-1)
             vec3 expected = {0.0f, 0.0f, -1.0f};
             printf("q: %f %f %f %f\n", q[0], q[1], q[2], q[3]);
             printf("v: %f %f %f\n", v[0], v[1], v[2]);
             printf("r: %f %f %f\n", r[0], r[1], r[2]);
             VEC_CHECK_CLOSE(3, r, expected);
        }

        it("should multiply matrix by quaternion") {
            mat4x4 M, R;
            mat4x4_identity(M);
            
            quat q;
            vec3 axis = {0.0f, 1.0f, 0.0f};
            float theta = (float)(M_PI / 2.0);
            quat_rotate(q, theta, axis);

            mat4x4o_mul_quat(R, M, q);

            quat q_conj;
            quat_conj(q_conj, q);
            mat4x4 M_restored;
            mat4x4o_mul_quat(M_restored, R, q_conj);
            
            for(int i=0; i<4; ++i) VEC_CHECK_CLOSE(4, M[i], M_restored[i]);
        }
        
         it("quat_from_mat4x4 (roundtrip)") {
            quat q;
            vec3 axis = {0.0f, 1.0f, 0.0f}; // Simple axis
            float theta = (float)M_PI_4;
            quat_rotate(q, theta, axis);
            
            mat4x4 M;
            mat4x4_from_quat(M, q);
            
            quat q2;
            quat_from_mat4x4(q2, M);
            VEC_CHECK_CLOSE(4, q, q2);
        }
    }
}
