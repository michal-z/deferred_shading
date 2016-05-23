#include "eneida.h"

uint8_t *
load_file(const char *filename, size_t *filesize)
{
    if (!filename || !filesize) return nullptr;

    HANDLE file = CreateFile(filename, GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return nullptr;

    DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return nullptr;
    }

    uint8_t *data = new uint8_t[size + 1];

    DWORD bytes;
    BOOL res = ReadFile(file, data, size, &bytes, nullptr);
    if (!res || bytes != size) {
        CloseHandle(file);
        delete[] data;
        return nullptr;
    }

    CloseHandle(file);
    *filesize = size;
    return data;
}

static bool
compile_shaders(demo_t *demo)
{
    size_t vs_size, ps_size;
    uint8_t *vs_code = load_file("data/shader/vs_full_triangle.cso", &vs_size);
    uint8_t *ps_code = load_file("data/shader/ps_sketch0.cso", &ps_size);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = demo->root_signature;
    pso_desc.VS = { vs_code, vs_size };
    pso_desc.PS = { ps_code, ps_size };
    pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count = 1;
    HRESULT r = demo->device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&demo->pso));
    if (FAILED(r)) return false;
    return true;
}

static void
transition_barrier(ID3D12GraphicsCommandList *cmdlist, ID3D12Resource *resource,
                   D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    D3D12_RESOURCE_BARRIER barrier_desc = {};
    barrier_desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier_desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier_desc.Transition.pResource = resource;
    barrier_desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier_desc.Transition.StateBefore = state_before;
    barrier_desc.Transition.StateAfter = state_after;
    cmdlist->ResourceBarrier(1, &barrier_desc);
}

static void
generate_device_commands(demo_t *demo)
{
    demo->cmd_allocator->Reset();

    ID3D12GraphicsCommandList *cmdlist = demo->cmd_list;

    cmdlist->Reset(demo->cmd_allocator, demo->pso);
    cmdlist->SetDescriptorHeaps(1, &demo->cbv_srv_uav_dh.heap);

    cmdlist->SetGraphicsRootSignature(demo->root_signature);
    cmdlist->RSSetViewports(1, &demo->viewport);
    cmdlist->RSSetScissorRects(1, &demo->scissor_rect);

    transition_barrier(cmdlist, demo->swap_buffer[demo->swap_buffer_index], D3D12_RESOURCE_STATE_PRESENT,
                       D3D12_RESOURCE_STATE_RENDER_TARGET);

    float clear_color[] = { 0.0f, 0.2f, 0.4f, 1.0f };

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = demo->rtv_dh.cpu_handle;
    rtv_handle.ptr += demo->swap_buffer_index * demo->rtv_dh_size;

    cmdlist->SetGraphicsRootDescriptorTable(0, demo->cbv_srv_uav_dh.gpu_handle);

    cmdlist->ClearRenderTargetView(rtv_handle, clear_color, 0, NULL);
    cmdlist->OMSetRenderTargets(1, &rtv_handle, TRUE, NULL);
    cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cmdlist->DrawInstanced(3, 1, 0, 0);

    transition_barrier(cmdlist, demo->swap_buffer[demo->swap_buffer_index], D3D12_RESOURCE_STATE_RENDER_TARGET,
                       D3D12_RESOURCE_STATE_PRESENT);

    cmdlist->Close();
}

static void
wait_for_device(demo_t *demo)
{
    UINT64 value = demo->fence_value;
    demo->cmd_queue->Signal(demo->fence, value);
    demo->fence_value++;

    if (demo->fence->GetCompletedValue() < value) {
        demo->fence->SetEventOnCompletion(value, demo->fence_event);
        WaitForSingleObject(demo->fence_event, INFINITE);
    }
}

bool
init(demo_t *demo)
{
    HRESULT res = demo->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                       IID_PPV_ARGS(&demo->cmd_allocator));
    if (FAILED(res)) return false;

    // root signature
    {
        D3D12_DESCRIPTOR_RANGE descriptor_range = {};
        descriptor_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        descriptor_range.NumDescriptors = 1;
        descriptor_range.BaseShaderRegister = 0;
        descriptor_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.DescriptorTable.NumDescriptorRanges = 1;
        param.DescriptorTable.pDescriptorRanges = &descriptor_range;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
        root_signature_desc.NumParameters = 1;
        root_signature_desc.pParameters = &param;

        ID3DBlob *blob = nullptr;
        res = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr);
        res |= demo->device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
                                                 IID_PPV_ARGS(&demo->root_signature));
        SAFE_RELEASE(blob);
        if (FAILED(res)) return false;
    }
    // CBV_SRV_UAV descriptor heap (visible by shaders)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc_heap = {};
        desc_heap.NumDescriptors = 4;
        desc_heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc_heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        res = demo->device->CreateDescriptorHeap(&desc_heap, IID_PPV_ARGS(&demo->cbv_srv_uav_dh.heap));
        if (FAILED(res)) return false;
        demo->cbv_srv_uav_dh.cpu_handle = demo->cbv_srv_uav_dh.heap->GetCPUDescriptorHandleForHeapStart();
        demo->cbv_srv_uav_dh.gpu_handle = demo->cbv_srv_uav_dh.heap->GetGPUDescriptorHandleForHeapStart();
    }
    // constant buffer
    {
        D3D12_HEAP_PROPERTIES props_heap = {};
        props_heap.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc_buffer = {};
        desc_buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc_buffer.Width = 1024;
        desc_buffer.Height = 1;
        desc_buffer.DepthOrArraySize = 1;
        desc_buffer.MipLevels = 1;
        desc_buffer.SampleDesc.Count = 1;
        desc_buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        res = demo->device->CreateCommittedResource(&props_heap, D3D12_HEAP_FLAG_NONE, &desc_buffer,
                                                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                    IID_PPV_ARGS(&demo->constant_buffer));
        if (FAILED(res)) return false;
    }
    // descriptors (cbv_srv_uav_dh)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor = demo->cbv_srv_uav_dh.cpu_handle;

        D3D12_CONSTANT_BUFFER_VIEW_DESC desc_cbuffer;
        desc_cbuffer.BufferLocation = demo->constant_buffer->GetGPUVirtualAddress();
        desc_cbuffer.SizeInBytes = 16 * 1024;

        demo->device->CreateConstantBufferView(&desc_cbuffer, descriptor);
    }

    res = demo->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&demo->fence));
    if (FAILED(res)) return false;
    demo->fence_value = 1;

    demo->fence_event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    if (!demo->fence_event) return false;

    if (!compile_shaders(demo)) return false;

    res = demo->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, demo->cmd_allocator,
                                          nullptr, IID_PPV_ARGS(&demo->cmd_list));
    if (FAILED(res)) return false;

    demo->cmd_list->Close();

    ID3D12CommandList *cmd_lists[] = { (ID3D12CommandList *)demo->cmd_list };
    demo->cmd_queue->ExecuteCommandLists(ARRAYSIZE(cmd_lists), cmd_lists);

    wait_for_device(demo);
    //audio_start(&demo->audio);

    return true;
}

void
deinit(demo_t *demo)
{
    if (demo->fence_event) {
        wait_for_device(demo);
        CloseHandle(demo->fence_event);
    }

    SAFE_RELEASE(demo->cmd_allocator);
}

void
update(demo_t *demo)
{
    generate_device_commands(demo);

    float *ptr;
    demo->constant_buffer->Map(0, nullptr, (void **)&ptr);
    ptr[0] = (float)demo->time;
    ptr[1] = (float)demo->resolution[0];
    ptr[2] = (float)demo->resolution[1];
    demo->constant_buffer->Unmap(0, nullptr);

    ID3D12CommandList *cmd_lists[] = { (ID3D12CommandList *)demo->cmd_list };
    demo->cmd_queue->ExecuteCommandLists(ARRAYSIZE(cmd_lists), cmd_lists);

    demo->dxgi_swap_chain->Present(0, 0);
    demo->swap_buffer_index = (demo->swap_buffer_index + 1) % k_swap_buffer_count;

    wait_for_device(demo);
}
