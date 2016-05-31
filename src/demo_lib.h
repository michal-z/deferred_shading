#pragma once

#include "demo_common.h"

#define SAFE_RELEASE(x) if ((x)) { (x)->Release(); (x) = nullptr; }

namespace Lib
{

uint8_t *LoadFile(const char *filename, size_t *filesize);
double GetTime();

ID3D12Resource *CreateUploadBuffer(ID3D12Device *device, uint64_t bufferSize);

} // namespace Lib
