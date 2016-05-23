#pragma once

#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d12.h>

#define k_swap_buffer_count 4
#define k_demo_name "eneida"
#define k_demo_resx 1024
#define k_demo_resy 1024
#define k_demo_fullscreen 0

#define SAFE_RELEASE(x) if ((x)) { (x)->Release(); (x) = nullptr; }

struct descriptor_heap_t
{
    ID3D12DescriptorHeap *heap;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
};

struct demo_t
{
    int swap_buffer_index;
    int resolution[2];
    double time;
    float time_delta;
    HWND hwnd;
    IDXGIFactory *dxgi_factory;
    IDXGISwapChain *dxgi_swap_chain;
    ID3D12Device *device;
    ID3D12CommandQueue *cmd_queue;
    ID3D12CommandAllocator *cmd_allocator;
    ID3D12GraphicsCommandList *cmd_list;
    ID3D12RootSignature *root_signature;
    descriptor_heap_t rtv_dh;
    descriptor_heap_t cbv_srv_uav_dh;
    UINT rtv_dh_size;
    UINT cbv_srv_uav_dh_size;
    ID3D12Resource *swap_buffer[k_swap_buffer_count];
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissor_rect;
    ID3D12PipelineState *pso;
    ID3D12Fence *fence;
    UINT64 fence_value;
    HANDLE fence_event;
    ID3D12Resource *constant_buffer;
};

bool init(demo_t *demo);
void deinit(demo_t *demo);
void update(demo_t *demo);
