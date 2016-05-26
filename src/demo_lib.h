#pragma once

#include <stdint.h>

#define SAFE_RELEASE(x) if ((x)) { (x)->Release(); (x) = nullptr; }

namespace DLib
{

uint8_t *LoadFile(const char *filename, size_t *filesize);
double GetTime();

}
