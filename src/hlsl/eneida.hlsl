struct input_t
{
    float time;
    float2 resolution;
};
ConstantBuffer<input_t> cbv_in : register(b0);

float4 vs_full_triangle(uint id : SV_VertexID) : SV_Position
{
    float2 verts[] = { float2(-1.0f, -1.0f), float2(3.0f, -1.0f), float2(-1.0f, 3.0f) };
    return float4(verts[id], 0.0f, 1.0f);
}

float4 ps_sketch0(float4 position : SV_Position) : SV_Target0
{
    float2 st = position.xy / cbv_in.resolution;
    float c = step(0.5f + 0.5f * sin(cbv_in.time), st.x);
    return float4(c, c, c, 1.0f);
}

// vim: cindent ft= :
