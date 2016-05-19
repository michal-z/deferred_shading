//-----------------------------------------------------------------------------
static void *
load_binary_file(const char *filename, size_t *filesize)
{
    if (!filename || !filesize) return NULL;

    HANDLE file = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;

    DWORD size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return NULL;
    }

    void *data = mem_alloc(size);
    if (!data) {
        CloseHandle(file);
        return NULL;
    }

    DWORD bytes;
    BOOL res = ReadFile(file, data, size, &bytes, NULL);
    if (!res || bytes != size) {
        CloseHandle(file);
        mem_free(data);
        return NULL;
    }

    CloseHandle(file);
    *filesize = size;
    return data;
}
//-----------------------------------------------------------------------------
static int
compile_shaders(demo_t *demo)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
        .pRootSignature = demo->root_signature,
        .VS = { g_vs_full_triangle, sizeof(g_vs_full_triangle) },
        .PS = { g_ps_sketch0, sizeof(g_ps_sketch0) },
        .RasterizerState.FillMode = D3D12_FILL_MODE_SOLID,
        .RasterizerState.CullMode = D3D12_CULL_MODE_NONE,
        .BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
        .SampleMask = UINT_MAX,
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc.Count = 1
    };
    HRESULT r = ID3D12Device_CreateGraphicsPipelineState(demo->device, &pso_desc, &IID_ID3D12PipelineState,
                                                         (void **)&demo->pso);
    if (FAILED(r)) return 0;

    return 1;
}
//-----------------------------------------------------------------------------
static void
resource_barrier(ID3D12GraphicsCommandList *cmd_list, ID3D12Resource *resource,
                 D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    D3D12_RESOURCE_BARRIER barrier_desc = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition.pResource = resource,
        .Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        .Transition.StateBefore = state_before,
        .Transition.StateAfter = state_after
    };
    ID3D12GraphicsCommandList_ResourceBarrier(cmd_list, 1, &barrier_desc);
}
//-----------------------------------------------------------------------------
static void
generate_device_commands(demo_t *demo)
{
    ID3D12CommandAllocator_Reset(demo->cmd_allocator);

    ID3D12GraphicsCommandList_Reset(demo->cmd_list, demo->cmd_allocator, demo->pso);
    ID3D12GraphicsCommandList_SetDescriptorHeaps(demo->cmd_list, 1, &demo->cbv_srv_uav_dh.heap);

    ID3D12GraphicsCommandList_SetGraphicsRootSignature(demo->cmd_list, demo->root_signature);
    ID3D12GraphicsCommandList_RSSetViewports(demo->cmd_list, 1, &demo->viewport);
    ID3D12GraphicsCommandList_RSSetScissorRects(demo->cmd_list, 1, &demo->scissor_rect);

    resource_barrier(demo->cmd_list, demo->swap_buffer[demo->swap_buffer_index], D3D12_RESOURCE_STATE_PRESENT,
                     D3D12_RESOURCE_STATE_RENDER_TARGET);

    float clear_color[] = { 0.0f, 0.2f, 0.4f, 1.0f };

    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = demo->rtv_dh.cpu_handle;
    rtv_handle.ptr += demo->swap_buffer_index * demo->rtv_dh_size;

    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(demo->cmd_list, 0, demo->cbv_srv_uav_dh.gpu_handle);

    ID3D12GraphicsCommandList_ClearRenderTargetView(demo->cmd_list, rtv_handle, clear_color, 0, NULL);
    ID3D12GraphicsCommandList_OMSetRenderTargets(demo->cmd_list, 1, &rtv_handle, TRUE, NULL);
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(demo->cmd_list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12GraphicsCommandList_DrawInstanced(demo->cmd_list, 3, 1, 0, 0);

    resource_barrier(demo->cmd_list, demo->swap_buffer[demo->swap_buffer_index], D3D12_RESOURCE_STATE_RENDER_TARGET,
                     D3D12_RESOURCE_STATE_PRESENT);

    ID3D12GraphicsCommandList_Close(demo->cmd_list);
}
//-----------------------------------------------------------------------------
static void
wait_for_device(demo_t *demo)
{
    UINT64 value = demo->fence_value;
    ID3D12CommandQueue_Signal(demo->cmd_queue, demo->fence, value);
    demo->fence_value++;

    if (ID3D12Fence_GetCompletedValue(demo->fence) < value) {
        ID3D12Fence_SetEventOnCompletion(demo->fence, value, demo->fence_event);
        WaitForSingleObject(demo->fence_event, INFINITE);
    }
}
//-----------------------------------------------------------------------------
static int
demo_init(demo_t *demo)
{
    HRESULT res = ID3D12Device_CreateCommandAllocator(demo->device, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                      &IID_ID3D12CommandAllocator, &demo->cmd_allocator);
    if (FAILED(res)) return 0;

    // root signature
    {
        D3D12_DESCRIPTOR_RANGE descriptor_range[1] = {
            {
                .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                .NumDescriptors = 1,
                .BaseShaderRegister = 0,
                .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
            }
        };
        D3D12_ROOT_PARAMETER param = {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable.NumDescriptorRanges = ARRAYSIZE(descriptor_range),
            .DescriptorTable.pDescriptorRanges = descriptor_range,
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        };
        D3D12_ROOT_SIGNATURE_DESC root_signature_desc = { .NumParameters = 1, .pParameters = &param };

        ID3DBlob *blob = NULL;
        res = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL);
        res |= ID3D12Device_CreateRootSignature(demo->device, 0, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob),
                                                &IID_ID3D12RootSignature, (void **)&demo->root_signature);
        SAFE_RELEASE(blob);
        if (FAILED(res)) return 0;
    }
    // CBV_SRV_UAV descriptor heap (visible by shaders)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc_heap = {
            .NumDescriptors = 4,
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        };
        res = ID3D12Device_CreateDescriptorHeap(demo->device, &desc_heap, &IID_ID3D12DescriptorHeap,
                                                (void **)&demo->cbv_srv_uav_dh.heap);
        if (FAILED(res)) return 0;
        demo->cbv_srv_uav_dh.cpu_handle = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(demo->cbv_srv_uav_dh.heap);
        demo->cbv_srv_uav_dh.gpu_handle = ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(demo->cbv_srv_uav_dh.heap);
    }
    // constant buffer
    {
        D3D12_HEAP_PROPERTIES props_heap = { .Type = D3D12_HEAP_TYPE_UPLOAD };
        D3D12_RESOURCE_DESC desc_buffer = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Width = 1024,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .SampleDesc.Count = 1,
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR
        };
        res = ID3D12Device_CreateCommittedResource(demo->device, &props_heap, D3D12_HEAP_FLAG_NONE, &desc_buffer,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                   &IID_ID3D12Resource, (void **)&demo->constant_buffer);
        if (FAILED(res)) return 0;
    }
    // descriptors (cbv_srv_uav_dh)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor = demo->cbv_srv_uav_dh.cpu_handle;

        D3D12_CONSTANT_BUFFER_VIEW_DESC desc_cbuffer;
        desc_cbuffer.BufferLocation = ID3D12Resource_GetGPUVirtualAddress(demo->constant_buffer);
        desc_cbuffer.SizeInBytes = 16 * 1024;

        ID3D12Device_CreateConstantBufferView(demo->device, &desc_cbuffer, descriptor);
    }

    res = ID3D12Device_CreateFence(demo->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&demo->fence);
    if (FAILED(res)) return 0;
    demo->fence_value = 1;

    demo->fence_event = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
    if (!demo->fence_event) return 0;

    if (!compile_shaders(demo)) return 0;

    res = ID3D12Device_CreateCommandList(demo->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, demo->cmd_allocator,
                                         NULL, &IID_ID3D12GraphicsCommandList, (void **)&demo->cmd_list);
    if (FAILED(res)) return 0;

    ID3D12GraphicsCommandList_Close(demo->cmd_list);

    ID3D12CommandList *cmd_lists[] = { (ID3D12CommandList *)demo->cmd_list };
    ID3D12CommandQueue_ExecuteCommandLists(demo->cmd_queue, ARRAYSIZE(cmd_lists), cmd_lists);

    wait_for_device(demo);
    //audio_start(&demo->audio);

    return 1;
}
//-----------------------------------------------------------------------------
static void
demo_deinit(demo_t *demo)
{
    if (demo->fence_event) {
        wait_for_device(demo);
        CloseHandle(demo->fence_event);
    }

    SAFE_RELEASE(demo->cmd_allocator);
}
//-----------------------------------------------------------------------------
static void
demo_update(demo_t *demo)
{
    generate_device_commands(demo);

    float *ptr;
    ID3D12Resource_Map(demo->constant_buffer, 0, NULL, (void **)&ptr);
    ptr[0] = (float)demo->time;
    ptr[1] = (float)demo->resolution[0];
    ptr[2] = (float)demo->resolution[1];
    ID3D12Resource_Unmap(demo->constant_buffer, 0, NULL);

    ID3D12CommandList *cmd_lists[] = { (ID3D12CommandList *)demo->cmd_list };
    ID3D12CommandQueue_ExecuteCommandLists(demo->cmd_queue, ARRAYSIZE(cmd_lists), cmd_lists);

    IDXGISwapChain_Present(demo->dxgi_swap_chain, 0, 0);
    demo->swap_buffer_index = (demo->swap_buffer_index + 1) % k_swap_buffer_count;

    wait_for_device(demo);
}
//-----------------------------------------------------------------------------
