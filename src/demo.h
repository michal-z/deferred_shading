#pragma once

#include "demo_common.h"

const uint32_t kNumBufferedFrames = 3;
const uint32_t kNumSwapBuffers = 4;
const char *kDemoName = "Deferred Shading Demo";
const uint32_t kDemoResX = 1280;
const uint32_t kDemoResY = 720;

class CGuiRenderer;
class CSceneResources;

class CDemo
{
public:
    ~CDemo();
    bool Init();
    void Update();

    uint32_t BackBufferIndex;
    uint32_t FrameIndex;
    uint32_t Resolution[2];
    double Time;
    float TimeDelta;

private:
    struct SFrameResources
    {
        ID3D12CommandAllocator *CmdAllocator = nullptr;
        ID3D12Resource *PerFrameCb = nullptr;
        void *PerFrameCbPtr;
    };
    SFrameResources FrameResources[kNumBufferedFrames];

    HWND WinHandle = nullptr;
    IDXGIFactory *FactoryDXGI = nullptr;
    IDXGISwapChain3 *SwapChain = nullptr;
    ID3D12Device *Device = nullptr;

    ID3D12CommandQueue *CmdQueue = nullptr;
    ID3D12GraphicsCommandList *CmdList = nullptr;

    ID3D12DescriptorHeap *SwapBuffersHeap = nullptr;
    ID3D12Resource *SwapBuffers[kNumSwapBuffers] = {};
    ID3D12DescriptorHeap *DsvHeap = nullptr;
    ID3D12Resource *DsvImage = nullptr;
    uint32_t DescriptorSize;
    uint32_t DescriptorSizeRTV;

    ID3D12DescriptorHeap *SrvHeap = nullptr;

    ID3D12RootSignature *StaticMeshRs = nullptr;
    ID3D12PipelineState *ScenePso = nullptr;

    D3D12_VIEWPORT Viewport;
    D3D12_RECT ScissorRect;

    uint64_t CpuCompletedFrames = 0;
    ID3D12Fence *FrameFence = nullptr;
    HANDLE FrameFenceEvent = nullptr;

    CGuiRenderer *GuiRenderer = nullptr;
    CSceneResources *SceneResources = nullptr;

    void Render();
    static LRESULT CALLBACK Winproc(HWND win, UINT msg, WPARAM wparam, LPARAM lparam);
    void UpdateFrameStats();
    void RenderScene();

    bool InitWindowAndD3D12();
    bool InitPipelineStates();
    bool InitConstantBuffers();
    bool InitDescriptorHeap();
};
