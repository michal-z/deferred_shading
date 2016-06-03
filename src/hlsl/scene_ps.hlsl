#define RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
    "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX)"

struct Input
{
    float4 Position : SV_Position;
    float2 Texcoord : TEXCOORD;
};

[RootSignature(RootSig)]
float4 main(Input i) : SV_Target0
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}

// vim: cindent ft= :
