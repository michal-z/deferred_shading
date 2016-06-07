#include "demo_lib.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

uint8_t *Lib::LoadFile(const char *filename, size_t *filesize)
{
    if (!filename || !filesize) return nullptr;

    HANDLE file = CreateFile(filename, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return nullptr;

    DWORD size = GetFileSize(file, nullptr);
    if (size == INVALID_FILE_SIZE)
    {
        CloseHandle(file);
        return nullptr;
    }

    uint8_t *data = new uint8_t[size];

    DWORD bytes;
    BOOL res = ReadFile(file, data, size, &bytes, nullptr);
    if (!res || bytes != size)
    {
        CloseHandle(file);
        delete[] data;
        return nullptr;
    }

    CloseHandle(file);
    *filesize = size;
    return data;
}

double Lib::GetTime()
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

ID3D12Resource *Lib::CreateUploadBuffer(ID3D12Device *device, uint64_t bufferSize)
{
    assert(device);
    assert(bufferSize > 0);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = bufferSize;
    bufferDesc.Height = bufferDesc.DepthOrArraySize = bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ID3D12Resource *uploadBuffer = nullptr;
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                    IID_PPV_ARGS(&uploadBuffer));
    return uploadBuffer;
}


ID3D12Resource *Lib::CreateTextureFromMemory(uint32_t imgW, uint32_t imgH, uint8_t *imgPix,
                                             D3D12_RESOURCE_STATES requestedState,
                                             ID3D12GraphicsCommandList *cmdList,
                                             ID3D12Resource **uploadBuffer)
{
    assert(imgW > 0 && imgH > 0);
    assert(imgPix);
    assert(cmdList);
    assert(uploadBuffer);

    ID3D12Device *device;
    HRESULT hr = cmdList->GetDevice(IID_PPV_ARGS(&device));
    if (FAILED(hr))
    {
        stbi_image_free(imgPix);
        return nullptr;
    }

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = (UINT64)imgW;
    textureDesc.Height = (UINT)imgH;
    textureDesc.DepthOrArraySize = textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    ID3D12Resource *texture;
    hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                         D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                         IID_PPV_ARGS(&texture));
    if (FAILED(hr))
    {
        stbi_image_free(imgPix);
        return nullptr;
    }

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
    uint32_t numRows;
    uint64_t rowSize, bufferSize;
    device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &layout, &numRows, &rowSize, &bufferSize);

    *uploadBuffer = CreateUploadBuffer(device, bufferSize);
    if (*uploadBuffer == nullptr)
    {
        texture->Release();
        stbi_image_free(imgPix);
        return nullptr;
    }

    D3D12_RANGE range = {};
    uint8_t *ptr;
    (*uploadBuffer)->Map(0, &range, (void **)&ptr);
    for (uint32_t row = 0; row < numRows; ++row)
    {
        memcpy(ptr + row * rowSize, imgPix + row * imgW * 4, imgW * 4);
    }
    (*uploadBuffer)->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION copyDst = {};
    copyDst.pResource = texture;
    copyDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    copyDst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION copySrc = {};
    copySrc.pResource = *uploadBuffer;
    copySrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    copySrc.PlacedFootprint = layout;
    cmdList->CopyTextureRegion(&copyDst, 0, 0, 0, &copySrc, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = requestedState;
    barrier.Transition.Subresource = 0;
    cmdList->ResourceBarrier(1, &barrier);

    return texture;
}

ID3D12Resource *Lib::CreateTextureFromFile(const char *filename, D3D12_RESOURCE_STATES requestedState,
                                           ID3D12GraphicsCommandList *cmdList,
                                           ID3D12Resource **uploadBuffer)
{
    assert(filename);
    assert(cmdList);
    assert(uploadBuffer);

    int32_t imgW, imgH, imgN;
    uint8_t *imgPix = stbi_load(filename, &imgW, &imgH, &imgN, 4);
    if (!imgPix) return nullptr;

    ID3D12Resource *texture = CreateTextureFromMemory(imgW, imgH, imgPix, requestedState,
                                                      cmdList, uploadBuffer);
    stbi_image_free(imgPix);
    return texture;
}
