#include "demo_lib.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

uint8_t *DLib::LoadFile(const char *filename, size_t *filesize)
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

double DLib::GetTime()
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
