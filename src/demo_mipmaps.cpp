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
    descHeap.NumDescriptors = 2;
    descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = Device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&SrvUavHeap));
    if (FAILED(hr)) return false;


    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = 1024; // FIXME: this must be max size of all textures
    textureDesc.Height = 1024;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                         D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                         IID_PPV_ARGS(&MipTexture));

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = SrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    Device->CreateUnorderedAccessView(MipTexture, nullptr, &uavDesc, cpuHandle);

    return true;
}

void CMipmapGenerator::Deinit()
{
}

bool CMipmapGenerator::Generate(ID3D12Resource *texture, ID3D12GraphicsCommandList *cmdList)
{
    if (!texture) return true;
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
    srvDesc.Texture2D.MipLevels = 1;
    Device->CreateShaderResourceView(texture, &srvDesc, cpuHandle);


    cmdList->SetPipelineState(Pso);
    cmdList->SetDescriptorHeaps(1, &SrvUavHeap);
    cmdList->SetComputeRootSignature(RootSig);
    cmdList->SetComputeRootDescriptorTable(0, gpuHandle);
    cmdList->Dispatch((UINT)textureDesc.Width / 2 / 32, textureDesc.Height / 2 / 32, 1);


    D3D12_RESOURCE_BARRIER barrier[2] = {};
    barrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[0].Transition.pResource = MipTexture;
    barrier[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier[0].Transition.Subresource = 0;
    barrier[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[1].Transition.pResource = texture;
    barrier[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier[1].Transition.Subresource = 1;
    cmdList->ResourceBarrier(_countof(barrier), barrier);


    D3D12_TEXTURE_COPY_LOCATION copyDst = {};
    copyDst.pResource = texture;
    copyDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    copyDst.SubresourceIndex = 1;

    D3D12_TEXTURE_COPY_LOCATION copySrc = {};
    copySrc.pResource = MipTexture;
    copySrc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    copySrc.SubresourceIndex = 0;

    D3D12_BOX srcBox = { 0, 0, 0, (UINT)textureDesc.Width/2, textureDesc.Height/2, 1 };
    cmdList->CopyTextureRegion(&copyDst, 0, 0, 0, &copySrc, &srcBox);

    barrier[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[0].Transition.pResource = MipTexture;
    barrier[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier[0].Transition.Subresource = 0;
    barrier[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier[1].Transition.pResource = texture;
    barrier[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier[1].Transition.Subresource = 1;
    cmdList->ResourceBarrier(_countof(barrier), barrier);

    return true;
}
