#include "demo_mipmaps.h"
#include "demo_lib.h"

bool CMipmapGenerator::Init(ID3D12Device *device)
{
    assert(device);
    Device = device;

    size_t csSize;
    const uint8_t *csCode = Lib::LoadFile("data/shader/gen_mipmap_cs.cso", &csSize);
    assert(csCode);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeDesc = {};
    pipeDesc.CS = { csCode, csSize };

    HRESULT hr = Device->CreateComputePipelineState(&pipeDesc, IID_PPV_ARGS(&Pso));
    if (FAILED(hr)) return false;

    hr = Device->CreateRootSignature(0, csCode, csSize, IID_PPV_ARGS(&RootSig));
    if (FAILED(hr)) return false;

    D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
    descHeap.NumDescriptors = 5;
    descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = Device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&SrvUavHeap));
    if (FAILED(hr)) return false;


    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = 512; // FIXME: this must be max size of all textures level 1
    textureDesc.Height = 512;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    for (uint32_t i = 0; i < 4; ++i)
    {
        hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                             D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                             IID_PPV_ARGS(&MipTextures[i]));
        if (FAILED(hr)) return false;

        textureDesc.Width /= 2;
        textureDesc.Height /= 2;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = SrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    for (uint32_t i = 0; i < 4; ++i)
    {
        Device->CreateUnorderedAccessView(MipTextures[i], nullptr, &uavDesc, cpuHandle);
        cpuHandle.ptr += Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    return true;
}

void CMipmapGenerator::Deinit()
{
}

bool CMipmapGenerator::Generate(ID3D12Resource *texture, ID3D12GraphicsCommandList *cmdList)
{
    if (!texture) return true; // FIXME: ???
    assert(texture);
    assert(cmdList);

    D3D12_RESOURCE_DESC textureDesc = texture->GetDesc();

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = SrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = SrvUavHeap->GetGPUDescriptorHandleForHeapStart();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = -1;
    Device->CreateShaderResourceView(texture, &srvDesc, cpuHandle);


    cmdList->SetPipelineState(Pso);
    cmdList->SetDescriptorHeaps(1, &SrvUavHeap);
    cmdList->SetComputeRootSignature(RootSig);
    cmdList->SetComputeRootDescriptorTable(1, gpuHandle);

    struct
    {
        uint32_t SrcMipLevel;
        uint32_t NumMipLevels;
        XMFLOAT2 TexelSize;
    } rootConst = { 0, 4, XMFLOAT2(0.5f / textureDesc.Width, 0.5f / textureDesc.Height) };

    cmdList->SetComputeRoot32BitConstant(0, 0, 0);
    cmdList->Dispatch((UINT)textureDesc.Width / 2 / 8, textureDesc.Height / 2 / 8, 1);


    D3D12_RESOURCE_BARRIER barrier[5] = {};
    for (uint32_t i = 0; i < 4; ++i)
    {
        barrier[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier[i].Transition.pResource = MipTextures[i];
        barrier[i].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier[i].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier[i].Transition.Subresource = 0;
    }
    barrier[4].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[4].Transition.pResource = texture;
    barrier[4].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier[4].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier[4].Transition.Subresource = 1;
    cmdList->ResourceBarrier(_countof(barrier), barrier);


    for (uint32_t i = 0; i < 4; ++i)
    {
        D3D12_TEXTURE_COPY_LOCATION copyDst = {};
        copyDst.pResource = texture;
        copyDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        copyDst.SubresourceIndex = i + 1;

        D3D12_TEXTURE_COPY_LOCATION copySrc = {};
        copySrc.pResource = MipTextures[i];
        copySrc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        copySrc.SubresourceIndex = 0;

        D3D12_BOX srcBox = { 0, 0, 0, (UINT)(textureDesc.Width >> (i + 1)), (textureDesc.Height >> (i + 1)), 1 };
        cmdList->CopyTextureRegion(&copyDst, 0, 0, 0, &copySrc, &srcBox);
    }

    for (uint32_t i = 0; i < 4; ++i)
    {
        barrier[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier[i].Transition.pResource = MipTextures[i];
        barrier[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier[i].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier[i].Transition.Subresource = 0;
    }
    barrier[4].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[4].Transition.pResource = texture;
    barrier[4].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier[4].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier[4].Transition.Subresource = 1;
    cmdList->ResourceBarrier(_countof(barrier), barrier);

    return true;
}
