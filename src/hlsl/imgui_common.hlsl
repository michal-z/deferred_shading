#define RsImgui \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
    "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX)"

struct VsInput
{
    float2 Position : POSITION;
    float2 Texcoord : TEXCOORD;
    float4 Color : COLOR;
};

struct VsOutput
{
    float4 Position : SV_Position;
    float2 Texcoord : TEXCOORD;
    float4 Color : COLOR;
};

// vim: cindent ft= :
