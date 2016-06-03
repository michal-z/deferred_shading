#define RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
    "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX)"

struct Input
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 Texcoord : TEXCOORD;
    float3 Tangent : TANGENT;
    float3 Bitangent : BITANGENT;
};

struct Output
{
    float4 Position : SV_Position;
    float2 Texcoord : TEXCOORD;
};

struct SPerFrame
{
    float4x4 ViewProjMat;
};
ConstantBuffer<SPerFrame> CbvPerFrame : register(b0);

[RootSignature(RootSig)]
Output main(Input i)
{
    Output o;
    o.Position = mul(float4(i.Position, 1.0f), CbvPerFrame.ViewProjMat);
    o.Texcoord = i.Texcoord;
    return o;
}

// vim: cindent ft= :
