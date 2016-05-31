#pragma once

#include "demo_common.h"

class CGuiRenderer
{
public:
    ~CGuiRenderer();
    bool Init(ID3D12GraphicsCommandList *cmdList, uint32_t numBufferedFrames,
              eastl::vector<ID3D12Resource *> *uploadBuffers);
    bool Render(ID3D12GraphicsCommandList *cmdList, uint32_t frameIndex);


private:
    struct SFrameResources
    {
        ID3D12Resource *ConstantBuffer = nullptr;
        void *ConstantBufferPtr = nullptr;

        ID3D12Resource *VertexBuffer = nullptr;
        void *VertexBufferPtr = nullptr;
        uint32_t VertexBufferSize = 0;

        ID3D12Resource *IndexBuffer = nullptr;
        void *IndexBufferPtr = nullptr;
        uint32_t IndexBufferSize = 0;
    };

    ID3D12Device *Device = nullptr;
    ID3D12RootSignature *RootSignature = nullptr;
    ID3D12PipelineState *Pso = nullptr;
    ID3D12Resource *FontTex = nullptr;
    uint32_t NumBufferedFrames = 0;
    SFrameResources *FrameResources = nullptr;
    ID3D12DescriptorHeap *DescriptorHeap = nullptr;

    bool CompileShaders();
};
