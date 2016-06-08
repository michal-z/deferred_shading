#include "scene_common.hlsl"

Texture2D gDiffuseTexture : register(t0);
SamplerState gSampler : register(s0);

[RootSignature(RsStaticMesh)]
float4 main(VsOutput i) : SV_Target0
{
    return gDiffuseTexture.SampleLevel(gSampler, i.Texcoord, 1);
    //return gDiffuseTexture.Sample(gSampler, i.Texcoord);
}

// vim: cindent ft= :
