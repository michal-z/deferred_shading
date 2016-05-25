#pragma once

#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d12.h>
#include <dxgi1_4.h>

const uint32_t kNumBufferedFrames = 3;
#define k_swap_buffer_count 4
#define k_demo_name "eneida"
#define k_demo_resx 1024
#define k_demo_resy 1024
#define k_demo_fullscreen 0

#define SAFE_RELEASE(x) if ((x)) { (x)->Release(); (x) = nullptr; }

class TGuiRenderer;
class TDemo
{
public:
    ~TDemo();
    bool init();
    void update();

    uint32_t BackBufferIndex;
    int Resolution[2];
    double Time;
    float TimeDelta;


private:
    HWND WinHandle;
    IDXGIFactory *FactoryDXGI;
    IDXGISwapChain3 *SwapChain;
    ID3D12Device *Device;

    ID3D12CommandQueue *CmdQueue;
    ID3D12CommandAllocator *CmdAllocator[kNumBufferedFrames];
    ID3D12GraphicsCommandList *CmdList;

    ID3D12DescriptorHeap *SwapBuffersHeap;
    ID3D12Resource *SwapBuffers[k_swap_buffer_count];
    UINT DescriptorSize;
    UINT DescriptorSizeRTV;

    D3D12_VIEWPORT Viewport;
    D3D12_RECT ScissorRect;

    UINT64 CpuCompletedFrames;
    ID3D12Fence *FrameFence;
    HANDLE FrameFenceEvent;

    TGuiRenderer *GuiRenderer;


    static LRESULT CALLBACK winproc(HWND win, UINT msg, WPARAM wparam, LPARAM lparam);
    void updateFrameStats();
    bool initWindowAndD3D12();
};
