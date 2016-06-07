#define RsStaticMesh \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
    "DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_PIXEL), " \
    "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX), " \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, visibility = SHADER_VISIBILITY_PIXEL)"

// vim: cindent ft= :
