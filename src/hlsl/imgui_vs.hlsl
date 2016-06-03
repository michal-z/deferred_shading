struct Input
{
    float2 Position : POSITION;
    float2 Texcoord : TEXCOORD;
    float4 Color : COLOR;
};

struct Output
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

Output main(Input i)
{
    Output o;
    o.Position = mul(gTransform.Mat, float4(i.Position, 0.0f, 1.0f));
    o.Texcoord = i.Texcoord;
    o.Color = i.Color;
    return o;
}

// vim: cindent ft= :
