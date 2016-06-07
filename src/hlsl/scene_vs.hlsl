#include "scene_common.hlsl"

struct SPerFrame
{
    float4x4 ViewProjMat;
};
ConstantBuffer<SPerFrame> CbvPerFrame : register(b0);

[RootSignature(RsStaticMesh)]
VsOutput main(VsInput i)
{
    VsOutput o;
    o.Position = mul(float4(i.Position, 1.0f), CbvPerFrame.ViewProjMat);
    o.Texcoord = i.Texcoord;
    return o;
}

// vim: cindent ft= :
