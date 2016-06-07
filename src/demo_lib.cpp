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
