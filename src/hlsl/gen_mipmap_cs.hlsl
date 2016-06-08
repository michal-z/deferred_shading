#define RsMipmap \
    "DescriptorTable(SRV(t0), UAV(u0))"

Texture2D<float4> gLevel0 : register(t0);
RWTexture2D<float4> gLevel1 : register(u0);

[RootSignature(RsMipmap)]
[numthreads(32, 32, 1)]
void main(uint3 globalIdx : SV_DispatchThreadID)
{
    uint x = globalIdx.x * 2;
    uint y = globalIdx.y * 2;

    float4 s00 = gLevel0[uint2(x, y)];
    float4 s10 = gLevel0[uint2(x + 1, y)];
    float4 s01 = gLevel0[uint2(x, y + 1)];
    float4 s11 = gLevel0[uint2(x + 1, y + 1)];

    gLevel1[globalIdx.xy] = 0.25f * (s00 + s01 + s10 + s11);
}

// vim: cindent ft= :
