#pragma once

#include "demo_common.h"
#include "stb_image.h"

#define SAFE_RELEASE(x) if ((x)) { (x)->Release(); (x) = nullptr; }

namespace Lib
{

uint8_t *LoadFile(const char *filename, size_t *filesize);
double GetTime();

ID3D12Resource *CreateUploadBuffer(ID3D12Device *device, uint64_t bufferSize);

ID3D12Resource *CreateTextureFromMemory(uint32_t imgW, uint32_t imgH, uint32_t imgN, uint8_t *imgPix,
                                        D3D12_RESOURCE_STATES requestedState,
                                        ID3D12GraphicsCommandList *cmdList,
                                        ID3D12Resource **uploadBuffer);
ID3D12Resource *CreateTextureFromFile(const char *filename, D3D12_RESOURCE_STATES requestedState,
                                      ID3D12GraphicsCommandList *cmdList,
                                      ID3D12Resource **uploadBuffer);

} // namespace Lib
