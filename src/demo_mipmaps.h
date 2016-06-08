#pragma once

#include "demo_common.h"

class CMipmapGenerator
{
public:
    ~CMipmapGenerator() { Deinit(); }
    bool Init(ID3D12Device *device);
    void Deinit();
    bool Generate(ID3D12Resource *texture, ID3D12GraphicsCommandList *cmdList);

private:
    ID3D12Device *Device = nullptr;
    ID3D12PipelineState *Pso = nullptr;
    ID3D12RootSignature *RootSig = nullptr;
    ID3D12DescriptorHeap *SrvUavHeap = nullptr;
    ID3D12Resource *MipTexture = nullptr;
};
