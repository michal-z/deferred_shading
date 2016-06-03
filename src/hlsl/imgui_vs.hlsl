#include "imgui_common.hlsl"

struct STransform
{
    float4x4 Mat;
};
ConstantBuffer<STransform> gTransform : register(b0);

VsOutput main(VsInput i)
{
    VsOutput o;
    o.Position = mul(gTransform.Mat, float4(i.Position, 0.0f, 1.0f));
    o.Texcoord = i.Texcoord;
    o.Color = i.Color;
    return o;
}

// vim: cindent ft= :
