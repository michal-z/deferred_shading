#include "eneida.h"
#include <new>
#include "EABase/eabase.h"
#include "EASTL/vector.h"

void *operator new[](size_t size, const char* /*name*/, int /*flags*/,
                     unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return malloc(size);
}

void *operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* /*name*/,
                     int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
    return _aligned_offset_malloc(size, alignment, alignmentOffset);
}

TDemo::~TDemo()
{
    // wait for the GPU
    if (CmdQueue && FrameFence && FrameFenceEvent)
    {
        CmdQueue->Signal(FrameFence, CpuCompletedFrames + 10);
        FrameFence->SetEventOnCompletion(CpuCompletedFrames + 10, FrameFenceEvent);
        WaitForSingleObject(FrameFenceEvent, INFINITE);
    }

    //gui_renderer_deinit(demo->gui_renderer);

    if (FrameFenceEvent)
    {
        CloseHandle(FrameFenceEvent);
    }
    SAFE_RELEASE(SwapChain);
    SAFE_RELEASE(CmdQueue);
    SAFE_RELEASE(Device);
    SAFE_RELEASE(FactoryDXGI);
}

bool TDemo::init()
{
    return initWindowAndD3D12();
}

void TDemo::update()
{
    updateFrameStats();
}

LRESULT CALLBACK TDemo::winproc(HWND win, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_DESTROY:
        case WM_KEYDOWN:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(win, msg, wparam, lparam);
}

static double GetTime()
{
    static LARGE_INTEGER freq = {};
    static LARGE_INTEGER counter0 = {};

    if (freq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&counter0);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart - counter0.QuadPart) / (double)freq.QuadPart;
}

void TDemo::updateFrameStats()
{
    static double prevTime = -1.0;
    static double prevFpsTime = 0.0;
    static int fpsFrame = 0;

    if (prevTime < 0.0)
    {
        prevTime = GetTime();
        prevFpsTime = prevTime;
    }

    Time = GetTime();
    TimeDelta = (float)(Time - prevTime);
    prevTime = Time;

    if ((Time - prevFpsTime) >= 1.0)
    {
        double fps = fpsFrame / (Time - prevFpsTime);
        double us = (1.0 / fps) * 1000000.0;
        char text[256];
        wsprintf(text, "[%d fps  %d us] %s", (int)fps, (int)us, k_demo_name);
        SetWindowText(WinHandle, text);
        prevFpsTime = Time;
        fpsFrame = 0;
    }
    fpsFrame++;
}

bool TDemo::initWindowAndD3D12()
{
    WNDCLASS winclass = {};
    winclass.lpfnWndProc = winproc;
    winclass.hInstance = GetModuleHandle(nullptr);
    winclass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    winclass.lpszClassName = k_demo_name;
    if (!RegisterClass(&winclass)) return false;

    if (k_demo_fullscreen)
    {
        Resolution[0] = GetSystemMetrics(SM_CXSCREEN);
        Resolution[1] = GetSystemMetrics(SM_CYSCREEN);
        ShowCursor(FALSE);
    }

    RECT rect = { 0, 0, Resolution[0], Resolution[1] };
    if (!AdjustWindowRect(&rect, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX, FALSE))
        return false;

    WinHandle = CreateWindow(k_demo_name, k_demo_name,
                             WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             rect.right - rect.left, rect.bottom - rect.top,
                             nullptr, nullptr, nullptr, 0);
    if (!WinHandle) return false;

    HRESULT res = CreateDXGIFactory1(IID_PPV_ARGS(&FactoryDXGI));
    if (FAILED(res)) return false;

#ifdef _DEBUG
    ID3D12Debug *dbg = nullptr;
    D3D12GetDebugInterface(IID_PPV_ARGS(&dbg));
    if (dbg)
    {
        dbg->EnableDebugLayer();
        SAFE_RELEASE(dbg);
    }
#endif

    res = D3D12CreateDevice(0, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&Device));
    if (FAILED(res)) return false;

    D3D12_COMMAND_QUEUE_DESC descCmdQueue = {};
    descCmdQueue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    descCmdQueue.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    descCmdQueue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    res = Device->CreateCommandQueue(&descCmdQueue, IID_PPV_ARGS(&CmdQueue));
    if (FAILED(res)) return false;

    DXGI_SWAP_CHAIN_DESC descSwapChain = {};
    descSwapChain.BufferCount = k_swap_buffer_count;
    descSwapChain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    descSwapChain.OutputWindow = WinHandle;
    descSwapChain.SampleDesc.Count = 1;
    descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    descSwapChain.Windowed = (k_demo_fullscreen ? FALSE : TRUE);
    IDXGISwapChain *swapChain;
    res = FactoryDXGI->CreateSwapChain(CmdQueue, &descSwapChain, &swapChain);
    if (FAILED(res)) return false;

    res = swapChain->QueryInterface(IID_PPV_ARGS(&SwapChain));
    SAFE_RELEASE(swapChain);
    if (FAILED(res)) return false;

    BackBufferIndex = SwapChain->GetCurrentBackBufferIndex();

    DescriptorSizeRTV = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    DescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // RTV descriptor heap (includes swap buffer descriptors)
    {
        D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
        descHeap.NumDescriptors = k_swap_buffer_count;
        descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        res = Device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&SwapBuffersHeap));
        if (FAILED(res)) return false;

        D3D12_CPU_DESCRIPTOR_HANDLE handle = SwapBuffersHeap->GetCPUDescriptorHandleForHeapStart();

        for (uint32_t i = 0; i < k_swap_buffer_count; ++i)
        {
            res = SwapChain->GetBuffer(i, IID_PPV_ARGS(&SwapBuffers[i]));
            if (FAILED(res)) return false;

            Device->CreateRenderTargetView(SwapBuffers[i], nullptr, handle);
            handle.ptr += DescriptorSizeRTV;
        }
    }

    Viewport = { 0.0f, 0.0f, (float)Resolution[0], (float)Resolution[1], 0.0f, 1.0f };
    ScissorRect = { 0, 0, Resolution[0], Resolution[1] };

    res = Device->CreateFence(1, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&FrameFence));
    if (FAILED(res)) return false;

    FrameFenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    if (!FrameFenceEvent) return false;

    for (uint32_t i = 0; i < kNumBufferedFrames; ++i)
    {
        Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&CmdAllocator[i]));
        if (FAILED(res)) return false;
    }

    res = Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CmdAllocator[0],
                                    nullptr, IID_PPV_ARGS(&CmdList));
    if (FAILED(res)) return false;


    //demo->gui_renderer = gui_renderer_init(demo->cmdlist, k_frame_count);
    //if (!demo->gui_renderer) return false;


    CmdList->Close();
    ID3D12CommandList *cmdLists[] = { (ID3D12CommandList *)CmdList };
    CmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    CmdQueue->Signal(FrameFence, 0);
    FrameFence->SetEventOnCompletion(0, FrameFenceEvent);
    WaitForSingleObject(FrameFenceEvent, INFINITE);

    return true;
}

int CALLBACK WinMain(HINSTANCE hinst, HINSTANCE hprevinst, LPSTR cmdline, int cmdshow)
{
    TDemo *demo = new TDemo{};
    demo->Resolution[0] = k_demo_resx;
    demo->Resolution[1] = k_demo_resy;

    if (!demo->init())
    {
        delete demo;
        return 1;
    }

    MSG msg = {};
    for (;;)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) break;
        }
        else
        {
            demo->update();
        }
    }

    delete demo;
    return 0;
}
