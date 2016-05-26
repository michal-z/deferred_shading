struct VSInput
{
    float2 Position : POSITION;
    float2 Texcoord : TEXCOORD;
    float4 Color : COLOR;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float2 Texcoord : TEXCOORD;
    float4 Color : COLOR;
};

struct STransform
{
    float4x4 Mat;
};
ConstantBuffer<STransform> gTransform : register(b0);

VSOutput VSImgui(VSInput i)
{
    VSOutput o;
    o.Position = mul(gTransform.Mat, float4(i.Position, 0.0f, 1.0f));
    o.Texcoord = i.Texcoord;
    o.Color = i.Color;
    return o;
}


Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 PSImgui(VSOutput i) : SV_Target0
{
    return i.Color * gTexture.Sample(gSampler, i.Texcoord);
}

// vim: cindent ft= :
