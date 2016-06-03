struct Input
{
    float4 Position : SV_Position;
    float2 Texcoord : TEXCOORD;
    float4 Color : COLOR;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(Input i) : SV_Target0
{
    return i.Color * gTexture.Sample(gSampler, i.Texcoord);
}

// vim: cindent ft= :
