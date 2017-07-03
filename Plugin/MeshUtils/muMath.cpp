#include "pch.h"
#include "muMath.h"
#include "muRawVector.h"

#ifdef muMath_AddNamespace
namespace mu {
#endif

const float PI = 3.14159265358979323846264338327950288419716939937510f;
const float Deg2Rad = PI / 180.0f;
const float Rad2Deg = 1.0f / (PI / 180.0f);


static inline void ComputeTriangleTangentBasis(
    const float3 vertices[3], const float2 uv[3], float3 dst_tangent[3], float3 dst_binormal[3])
{
    float p[] = { vertices[1].x - vertices[0].x, vertices[1].y - vertices[0].y, vertices[1].z - vertices[0].z };
    float q[] = { vertices[2].x - vertices[0].x, vertices[2].y - vertices[0].y, vertices[2].z - vertices[0].z };

    float s[] = { uv[1].x - uv[0].x, uv[2].x - uv[0].x };
    float t[] = { uv[1].y - uv[0].y, uv[2].y - uv[0].y };

    float div = s[0] * t[1] - s[1] * t[0];
    float areaMult = std::abs(div);

    float3 tangent, binormal;
    if (areaMult >= 1e-8)
    {
        float r = 1.0f / div;

        s[0] *= r;  t[0] *= r;
        s[1] *= r;  t[1] *= r;

        tangent = normalize({
            t[1] * p[0] - t[0] * q[0],
            t[1] * p[1] - t[0] * q[1],
            t[1] * p[2] - t[0] * q[2]
        }) * areaMult;

        binormal = normalize({
            s[0] * q[0] - s[1] * p[0],
            s[0] * q[1] - s[1] * p[1],
            s[0] * q[2] - s[1] * p[2]
        }) * areaMult;
    }
    else {
        tangent = binormal = float3::zero();
    }

    const int kNextIndex[][2] = { { 2, 1 },{ 0, 2 },{ 1, 0 } };
    for (int v = 0; v < 3; ++v)
    {
        float3 edge1 = {
            vertices[kNextIndex[v][0]].x - vertices[v].x,
            vertices[kNextIndex[v][0]].y - vertices[v].y,
            vertices[kNextIndex[v][0]].z - vertices[v].z
        };
        float3 edge2 = {
            vertices[kNextIndex[v][1]].x - vertices[v].x,
            vertices[kNextIndex[v][1]].y - vertices[v].y,
            vertices[kNextIndex[v][1]].z - vertices[v].z
        };
        float angle = dot(normalize(edge1), normalize(edge2));
        float w = std::acos(clamp11(angle));

        dst_tangent[v] = tangent * w;
        dst_binormal[v] = binormal * w;
    }
}

static inline float4 OrthogonalizeTangent(float3 tangent, float3 binormal, float3 normal)
{
    float NdotT = dot(normal, tangent);
    tangent = {
        tangent.x - NdotT * normal.x,
        tangent.y - NdotT * normal.y,
        tangent.z - NdotT * normal.z
    };
    float magT = length(tangent);
    tangent = tangent / magT;

    float NdotB = dot(normal, binormal);
    float TdotB = dot(tangent, binormal) * magT;
    binormal = {
        binormal.x - NdotB * normal.x - TdotB * tangent.x,
        binormal.y - NdotB * normal.y - TdotB * tangent.y,
        binormal.z - NdotB * normal.z - TdotB * tangent.z
    };
    float magB = length(binormal);
    binormal = binormal / magB;


    const float kNormalizeEpsilon = 1e-6;

    if (magT <= kNormalizeEpsilon || magB <= kNormalizeEpsilon)
    {
        float3 axis1, axis2;

        float dpXN = std::abs(dot({ 1.0f, 0.0f, 0.0f }, normal));
        float dpYN = std::abs(dot({ 0.0f, 1.0f, 0.0f }, normal));
        float dpZN = std::abs(dot({ 0.0f, 0.0f, 1.0f }, normal));

        if (dpXN <= dpYN && dpXN <= dpZN)
        {
            axis1 = { 1.0f, 0.0f, 0.0f };
            if (dpYN <= dpZN)
                axis2 = { 0.0f, 1.0f, 0.0f };
            else
                axis2 = { 0.0f, 0.0f, 1.0f };
        }
        else if (dpYN <= dpXN && dpYN <= dpZN)
        {
            axis1 = { 0.0f, 1.0f, 0.0f };
            if (dpXN <= dpZN)
                axis2 = { 1.0f, 0.0f, 0.0f };
            else
                axis2 = { 0.0f, 0.0f, 1.0f };
        }
        else
        {
            axis1 = { 0.0f, 0.0f, 1.0f };
            if (dpXN <= dpYN)
                axis2 = { 1.0f, 0.0f, 0.0f };
            else
                axis2 = { 0.0f, 1.0f, 0.0f };
        }

        tangent = normalize(axis1 - normal * dot(normal, axis1));
        binormal = normalize(axis2 - normal * dot(normal, axis2) - normalize(tangent) * dot(tangent, axis2));
    }

    return { tangent.x, tangent.y, tangent.z,
        dot(cross(normal, tangent), binormal) > 0.0f ? 1.0f : -1.0f };
}


void generate_tangents(float4 *dst,
    const float3 *vertices, const float3 *normals, const float2 *uv, const int *indices, int num_triangles, int num_vertices)
{
    RawVector<float3> tangents, binormals;
    tangents.resize(num_vertices);
    binormals.resize(num_vertices);
    tangents.zeroclear();
    binormals.zeroclear();

    int ic = num_triangles * 3;
    int vc = num_vertices;

    for (int ti = 0; ti < ic; ti += 3) {
        int idx[] = { indices[ti + 0], indices[ti + 1], indices[ti + 2] };
        float3 v[3] = { vertices[idx[0]], vertices[idx[1]], vertices[idx[2]] };
        float2 u[3] = { uv[idx[0]], uv[idx[1]], uv[idx[2]] };

        float3 t[3];
        float3 b[3];
        ComputeTriangleTangentBasis(v, u, t, b);

        for (int i = 0; i < 3; ++i) {
            tangents[idx[i]] += t[i];
            binormals[idx[i]] += b[i];
        }
    }

    for (int vi = 0; vi < vc; ++vc) {
        dst[vc] = OrthogonalizeTangent(tangents[vc], binormals[vc], normals[vc]);
    }
}

#ifdef muMath_AddNamespace
}
#endif
