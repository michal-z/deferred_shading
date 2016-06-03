#include "imgui_common.hlsl"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VsOutput i) : SV_Target0
{
    return i.Color * gTexture.Sample(gSampler, i.Texcoord);
}

// vim: cindent ft= :
