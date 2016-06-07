#include "scene_common.hlsl"

[RootSignature(RsStaticMesh)]
float4 main(VsOutput i) : SV_Target0
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}

// vim: cindent ft= :
