#include "static_mesh_rootsig.hlsl"

struct VsInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 Texcoord : TEXCOORD;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

struct VsOutput
{
    float4 Position : SV_Position;
    float2 Texcoord : TEXCOORD;
};

// vim: cindent ft= :
