#pragma once

#include "demo_common.h"

const uint32_t kNumBufferedFrames = 3;
const uint32_t kNumSwapBuffers = 4;
const char *kDemoName = "Deferred Shading Demo";
const uint32_t kDemoResX = 1024;
const uint32_t kDemoResY = 1024;

class CGuiRenderer;
class CDemo
{
public:
    ~CDemo();
    bool Init();
    void Update();

    uint32_t BackBufferIndex = 0;
    uint32_t FrameIndex = 0;
    uint32_t Resolution[2];
    double Time;
    float TimeDelta;


private:
    HWND WinHandle = nullptr;
    IDXGIFactory *FactoryDXGI = nullptr;
    IDXGISwapChain3 *SwapChain = nullptr;
    ID3D12Device *Device = nullptr;

    ID3D12CommandQueue *CmdQueue = nullptr;
    ID3D12CommandAllocator *CmdAllocator[kNumBufferedFrames] = {};
    ID3D12GraphicsCommandList *CmdList = nullptr;

    ID3D12DescriptorHeap *SwapBuffersHeap = nullptr;
    ID3D12Resource *SwapBuffers[kNumSwapBuffers] = {};
    uint32_t DescriptorSize;
    uint32_t DescriptorSizeRTV;

    D3D12_VIEWPORT Viewport;
    D3D12_RECT ScissorRect;

    uint64_t CpuCompletedFrames;
    ID3D12Fence *FrameFence = nullptr;
    HANDLE FrameFenceEvent = nullptr;

    CGuiRenderer *GuiRenderer = nullptr;


    void Render();
    static LRESULT CALLBACK Winproc(HWND win, UINT msg, WPARAM wparam, LPARAM lparam);
    void UpdateFrameStats();
    bool InitWindowAndD3D12();
};
