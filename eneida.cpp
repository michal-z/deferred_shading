#include "eneida.h"

extern "C" { int _fltused; }

static LRESULT CALLBACK
winproc(HWND win, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_DESTROY:
        case WM_KEYDOWN:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(win, msg, wparam, lparam);
}

static double
get_time()
{
    static LARGE_INTEGER freq = {};
    static LARGE_INTEGER counter0 = {};

    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&counter0);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart - counter0.QuadPart) / (double)freq.QuadPart;
}

static void
update_frame_stats(demo_t *demo)
{
    static double prev_time = -1.0;
    static double prev_fps_time = 0.0;
    static int fps_frame = 0;

    if (prev_time < 0.0) {
        prev_time = get_time();
        prev_fps_time = prev_time;
    }

    demo->time = get_time();
    demo->time_delta = (float)(demo->time - prev_time);
    prev_time = demo->time;

    if ((demo->time - prev_fps_time) >= 1.0) {
        double fps = fps_frame / (demo->time - prev_fps_time);
        double us = (1.0 / fps) * 1000000.0;
        char text[256];
        wsprintf(text, "[%d fps  %d us] %s", (int)fps, (int)us, k_demo_name);
        SetWindowText(demo->hwnd, text);
        prev_fps_time = demo->time;
        fps_frame = 0;
    }
    fps_frame++;
}

static int
init(demo_t *demo)
{
    WNDCLASS winclass = {};
    winclass.lpfnWndProc = winproc;
    winclass.hInstance = GetModuleHandle(nullptr);
    winclass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    winclass.lpszClassName = k_demo_name;
    if (!RegisterClass(&winclass)) return 0;

    if (k_demo_fullscreen) {
        demo->resolution[0] = GetSystemMetrics(SM_CXSCREEN);
        demo->resolution[1] = GetSystemMetrics(SM_CYSCREEN);
        ShowCursor(FALSE);
    }

    RECT rect = { 0, 0, demo->resolution[0], demo->resolution[1] };
    if (!AdjustWindowRect(&rect, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX, FALSE))
        return 0;

    demo->hwnd = CreateWindow(k_demo_name, k_demo_name,
                              WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              rect.right - rect.left, rect.bottom - rect.top,
                              nullptr, nullptr, nullptr, 0);
    if (!demo->hwnd) return 0;

    HRESULT res = CreateDXGIFactory1(IID_PPV_ARGS(&demo->dxgi_factory));
    if (FAILED(res)) return 0;

#ifdef _DEBUG
    ID3D12Debug *dbg = nullptr;
    D3D12GetDebugInterface(IID_PPV_ARGS(&dbg));
    if (dbg) {
        dbg->EnableDebugLayer();
        SAFE_RELEASE(dbg);
    }
#endif

    res = D3D12CreateDevice(0, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&demo->device));
    if (FAILED(res)) return 0;

    D3D12_COMMAND_QUEUE_DESC desc_cmd_queue = {};
    desc_cmd_queue.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc_cmd_queue.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc_cmd_queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    res = demo->device->CreateCommandQueue(&desc_cmd_queue, IID_PPV_ARGS(&demo->cmd_queue));
    if (FAILED(res)) return 0;

    DXGI_SWAP_CHAIN_DESC desc_swap_chain = {};
    desc_swap_chain.BufferCount = k_swap_buffer_count;
    desc_swap_chain.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc_swap_chain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc_swap_chain.OutputWindow = demo->hwnd;
    desc_swap_chain.SampleDesc.Count = 1;
    desc_swap_chain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc_swap_chain.Windowed = (k_demo_fullscreen ? FALSE : TRUE);
    res = demo->dxgi_factory->CreateSwapChain(demo->cmd_queue, &desc_swap_chain, &demo->dxgi_swap_chain);
    if (FAILED(res)) return 0;

    demo->rtv_dh_size = demo->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    demo->cbv_srv_uav_dh_size = demo->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // RTV descriptor heap (includes swap buffer descriptors)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc_heap = {};
        desc_heap.NumDescriptors = k_swap_buffer_count;
        desc_heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        res = demo->device->CreateDescriptorHeap(&desc_heap, IID_PPV_ARGS(&demo->rtv_dh.heap));
        if (FAILED(res)) return 0;

        demo->rtv_dh.cpu_handle = demo->rtv_dh.heap->GetCPUDescriptorHandleForHeapStart();

        for (int i = 0; i < k_swap_buffer_count; ++i) {
            res = demo->dxgi_swap_chain->GetBuffer(i, IID_PPV_ARGS(&demo->swap_buffer[i]));
            if (FAILED(res)) return 0;
            D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = demo->rtv_dh.cpu_handle;
            rtv_handle.ptr += i * demo->rtv_dh_size;
            demo->device->CreateRenderTargetView(demo->swap_buffer[i], nullptr, rtv_handle);
        }
    }

    demo->viewport = { 0.0f, 0.0f, (float)demo->resolution[0], (float)demo->resolution[1], 0.0f, 1.0f };

    demo->scissor_rect.left = 0;
    demo->scissor_rect.top = 0;
    demo->scissor_rect.right = demo->resolution[0];
    demo->scissor_rect.bottom = demo->resolution[1];

    return 1;
}

static void
deinit(demo_t *demo)
{
    SAFE_RELEASE(demo->dxgi_swap_chain);
    SAFE_RELEASE(demo->cmd_queue);
    SAFE_RELEASE(demo->device);
    SAFE_RELEASE(demo->dxgi_factory);
}

static HANDLE glob_heap;
static void *
mem_alloc(size_t byte_count)
{
    void *data = HeapAlloc(glob_heap, 0, byte_count);
    return data;
}

static void
mem_free(void *ptr)
{
    HeapFree(glob_heap, 0, ptr);
}

void
start()
{
    glob_heap = GetProcessHeap();

    demo_t demo = {};
    demo.resolution[0] = k_demo_resx;
    demo.resolution[1] = k_demo_resy;

    if (!init(&demo)) {
        deinit(&demo);
        ExitProcess(1);
    }
    /*
    if (!demo_init(&demo)) {
        demo_deinit(&demo);
        deinit(&demo);
        ExitProcess(1);
    }
    */

    MSG msg = { 0 };
    for (;;) {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) break;
        } else {
            update_frame_stats(&demo);
            //demo_update(&demo);
        }
    }

    //demo_deinit(&demo);
    deinit(&demo);
    ExitProcess(0);
}
