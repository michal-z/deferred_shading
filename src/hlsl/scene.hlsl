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

struct SPerFrame
{
    float4x4 ViewProjMat;
};
ConstantBuffer<SPerFrame> CbvPerFrame : register(b0);

VsOutput VsScene(VsInput i)
{
    VsOutput o;
    o.Position = mul(float4(i.Position, 1.0f), CbvPerFrame.ViewProjMat);
    o.Texcoord = i.Texcoord;
    return o;
}


float4 PsScene(VsOutput i) : SV_Target0
{
    return float4(0.0f, 1.0f, 0.0f, 1.0f);
}

// vim: cindent ft= :
