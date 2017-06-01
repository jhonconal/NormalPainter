#include "ispcmath.h"

// functions that have gather/scatter operations need to be written in this compile unit.
// (--opt=force-aligned-memory is disabled)

export uniform int RayTrianglesIntersectionArray(
    uniform const float3& pos, uniform const float3& dir,
    uniform const float3 vertices[], uniform const int num_triangles,
    uniform int result[], uniform float distance[])
{
    uniform int total_hit = 0;

    // SIMD pass
    uniform int num_triangles_simd = num_triangles & ~(C - 1);
    for(uniform int bi=0; bi < num_triangles_simd; bi += C) {
        float3 p1, p2, p3;
        // this emits warnings but performace is acceptable.
        p1 = vertices[(bi + I) * 3  + 0];
        p2 = vertices[(bi + I) * 3  + 1];
        p3 = vertices[(bi + I) * 3  + 2];

        float d;
        bool hit = TriangleIntersection(pos, dir, p1, p2, p3, d);
        if(any(hit)) {
            uniform int hita[C]; hita[I] = hit;
            uniform float da[C]; da[I] = d;
            for(uniform int i = 0; i < C; ++i) {
                if(hita[i]) {
                    result[total_hit] = bi + i;
                    distance[total_hit++] = da[i];
                }
            }
        }
    }

    // non-SIMD pass
    for(uniform int ti = num_triangles_simd; ti < num_triangles; ++ti) {
        uniform float3 p1 = vertices[ti * 3 + 0];
        uniform float3 p2 = vertices[ti * 3 + 1];
        uniform float3 p3 = vertices[ti * 3 + 2];

        uniform float d;
        uniform bool hit = TriangleIntersection(pos, dir, p1, p2, p3, d);
        if(hit) {
            result[total_hit] = ti;
            distance[total_hit++] = d;
        }
    }

    return total_hit;
}