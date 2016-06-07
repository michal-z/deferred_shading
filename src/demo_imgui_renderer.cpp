#include "demo_imgui_renderer.h"
#include "demo_lib.h"
#include "imgui.h"

bool CGuiRenderer::CompileShaders()
{
    assert(Device);

    D3D12_INPUT_ELEMENT_DESC descInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    size_t vsSize, psSize;
    const uint8_t *vsCode = Lib::LoadFile("data/shader/imgui_vs.cso", &vsSize);
    const uint8_t *psCode = Lib::LoadFile("data/shader/imgui_ps.cso", &psSize);
    assert(vsCode && psCode);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.InputLayout = { descInputLayout, _countof(descInputLayout) };
    desc.VS = { vsCode, vsSize };
    desc.PS = { psCode, psSize };

    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    desc.SampleMask = UINT_MAX;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;

    HRESULT hr = Device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&Pso));
    if (FAILED(hr)) return false;

    hr = Device->CreateRootSignature(0, vsCode, vsSize, IID_PPV_ARGS(&RootSignature));
    if (FAILED(hr)) return false;

    return true;
}

bool CGuiRenderer::Init(uint32_t numBufferedFrames, ID3D12GraphicsCommandList *cmdList,
                        eastl::vector<ID3D12Resource *> *uploadBuffers)
{
    assert(cmdList && !Device && uploadBuffers);

    HRESULT hr = cmdList->GetDevice(IID_PPV_ARGS(&Device));
    if (FAILED(hr)) return false;

    // descriptor heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
        descHeap.NumDescriptors = 1;
        descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr = Device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&DescriptorHeap));
        if (FAILED(hr)) return false;
    }
    // font texture
    {
        ImGuiIO &io = ImGui::GetIO();

        uint8_t *pixels;
        int32_t width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Width = (UINT64)width;
        textureDesc.Height = (UINT)height;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                             D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                             IID_PPV_ARGS(&FontTex));
        if (FAILED(hr)) return false;

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t numRows;
        uint64_t rowSize, bufferSize;
        Device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &layout, &numRows, &rowSize, &bufferSize);

        ID3D12Resource *uploadBuffer = Lib::CreateUploadBuffer(Device, bufferSize);
        if (!uploadBuffer) return false;
        uploadBuffers->push_back(uploadBuffer);

        D3D12_RANGE range = {};
        uint8_t *ptr;
        uploadBuffer->Map(0, &range, (void **)&ptr);
        for (uint32_t row = 0; row < numRows; ++row)
        {
            memcpy(ptr + row * rowSize, pixels + row * width * 4, width * 4);
        }
        uploadBuffer->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION copyDst = {};
        copyDst.pResource = FontTex;
        copyDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        copyDst.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION copySrc = {};
        copySrc.pResource = uploadBuffer;
        copySrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        copySrc.PlacedFootprint = layout;
        cmdList->CopyTextureRegion(&copyDst, 0, 0, 0, &copySrc, nullptr);

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = FontTex;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = 0;
        cmdList->ResourceBarrier(1, &barrier);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        D3D12_CPU_DESCRIPTOR_HANDLE handle = DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        Device->CreateShaderResourceView(FontTex, &srvDesc, handle);
    }

    if (!CompileShaders()) return false;

    NumBufferedFrames = numBufferedFrames;
    FrameResources = new SFrameResources[NumBufferedFrames];

    for (uint32_t i = 0; i < NumBufferedFrames; ++i)
    {
        SFrameResources *res = &FrameResources[i];

        // constant buffer
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = 64;
        resourceDesc.Height = resourceDesc.DepthOrArraySize = resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                             IID_PPV_ARGS(&res->ConstantBuffer));
        if (FAILED(hr)) return false;

        D3D12_RANGE range = {};
        hr = res->ConstantBuffer->Map(0, &range, &res->ConstantBufferPtr);
        if (FAILED(hr)) return false;
    }

    return true;
}

CGuiRenderer::~CGuiRenderer()
{
    assert(Device && NumBufferedFrames > 0);

    SAFE_RELEASE(Device);
    SAFE_RELEASE(DescriptorHeap);
    SAFE_RELEASE(FontTex);
    SAFE_RELEASE(Pso);
    SAFE_RELEASE(RootSignature);
    for (uint32_t i = 0; i < NumBufferedFrames; ++i)
    {
        SAFE_RELEASE(FrameResources[i].ConstantBuffer);
        SAFE_RELEASE(FrameResources[i].VertexBuffer);
        SAFE_RELEASE(FrameResources[i].IndexBuffer);
    }
    delete[] FrameResources;
}

bool CGuiRenderer::Render(ID3D12GraphicsCommandList *cmdList, uint32_t frameIndex)
{
    assert(cmdList && frameIndex < NumBufferedFrames);

    ImGui::Render();

    ImDrawData *drawData = ImGui::GetDrawData();
    ImGuiIO &io = ImGui::GetIO();
    HRESULT hr;
    SFrameResources *res = &FrameResources[frameIndex];

    int32_t fbWidth = (int32_t)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int32_t fbHeight = (int32_t)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    drawData->ScaleClipRects(io.DisplayFramebufferScale);

    // create/resize vertex buffer
    if (res->VertexBufferSize == 0 || res->VertexBufferSize < drawData->TotalVtxCount * sizeof(ImDrawVert))
    {
        SAFE_RELEASE(res->VertexBuffer);

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = drawData->TotalVtxCount * sizeof(ImDrawVert);
        resourceDesc.Height = resourceDesc.DepthOrArraySize = resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                             IID_PPV_ARGS(&res->VertexBuffer));
        if (FAILED(hr)) return false;

        D3D12_RANGE range = {};
        if (FAILED(res->VertexBuffer->Map(0, &range, &res->VertexBufferPtr))) return false;
        res->VertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    }
    // create/resize index buffer
    if (res->IndexBufferSize == 0 || res->IndexBufferSize < drawData->TotalIdxCount * sizeof(ImDrawIdx))
    {
        SAFE_RELEASE(res->IndexBuffer);

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = drawData->TotalIdxCount * sizeof(ImDrawIdx);
        resourceDesc.Height = resourceDesc.DepthOrArraySize = resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                             IID_PPV_ARGS(&res->IndexBuffer));
        if (FAILED(hr)) return false;

        D3D12_RANGE range = {};
        if (FAILED(res->IndexBuffer->Map(0, &range, &res->IndexBufferPtr))) return false;
        res->IndexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);
    }
    // update vertex and index buffers
    {
        ImDrawVert *vtxDst = (ImDrawVert *)res->VertexBufferPtr;
        ImDrawIdx *idxDst = (ImDrawIdx *)res->IndexBufferPtr;

        for (uint32_t n = 0; n < (uint32_t)drawData->CmdListsCount; ++n)
        {
            ImDrawList *drawList = drawData->CmdLists[n];
            memcpy(vtxDst, &drawList->VtxBuffer[0], drawList->VtxBuffer.size() * sizeof(ImDrawVert));
            memcpy(idxDst, &drawList->IdxBuffer[0], drawList->IdxBuffer.size() * sizeof(ImDrawIdx));
            vtxDst += drawList->VtxBuffer.size();
            idxDst += drawList->IdxBuffer.size();
        }
    }
    // update constant buffer
    {
        const float L = 0.0f;
        const float R = (float)fbWidth;
        const float B = (float)fbHeight;
        const float T = 0.0f;
        const float mvp[4][4] =
        {
            { 2.0f / (R - L),    0.0f,              0.0f,    0.0f },
            { 0.0f,              2.0f / (T - B),    0.0f,    0.0f },
            { 0.0f,              0.0f,              0.5f,    0.0f },
            { (R + L) / (L - R), (T + B) / (B - T), 0.5f,    1.0f },
        };
        memcpy(res->ConstantBufferPtr, mvp, sizeof(mvp));
    }

    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)fbWidth, (float)fbHeight, 0.0f, 1.0f };
    cmdList->RSSetViewports(1, &viewport);

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->SetPipelineState(Pso);
    cmdList->SetDescriptorHeaps(1, &DescriptorHeap);
    cmdList->SetGraphicsRootSignature(RootSignature);
    cmdList->SetGraphicsRootConstantBufferView(0, res->ConstantBuffer->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootDescriptorTable(1, DescriptorHeap->GetGPUDescriptorHandleForHeapStart());


    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = res->VertexBuffer->GetGPUVirtualAddress();
    vbView.StrideInBytes = sizeof(ImDrawVert);
    vbView.SizeInBytes = drawData->TotalVtxCount * vbView.StrideInBytes;

    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = res->IndexBuffer->GetGPUVirtualAddress();
    ibView.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    ibView.SizeInBytes = drawData->TotalIdxCount * sizeof(ImDrawIdx);

    cmdList->IASetVertexBuffers(0, 1, &vbView);
    cmdList->IASetIndexBuffer(&ibView);


    int32_t vtxOffset = 0;
    uint32_t idxOffset = 0;
    for (uint32_t n = 0; n < (uint32_t)drawData->CmdListsCount; ++n)
    {
        ImDrawList *drawList = drawData->CmdLists[n];

        for (uint32_t cmd = 0; cmd < (uint32_t)drawList->CmdBuffer.size(); ++cmd)
        {
            ImDrawCmd *pcmd = &drawList->CmdBuffer[cmd];

            if (pcmd->UserCallback)
            {
                pcmd->UserCallback(drawList, pcmd);
            }
            else
            {
                D3D12_RECT r = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };
                cmdList->RSSetScissorRects(1, &r);
                cmdList->DrawIndexedInstanced(pcmd->ElemCount, 1, idxOffset, vtxOffset, 0);
            }
            idxOffset += pcmd->ElemCount;
        }
        vtxOffset += drawList->VtxBuffer.size();
    }

    return true;
}
