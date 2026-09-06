#pragma once

#include <windows.h>

#pragma pack(push, 1)

struct SUSIE_PICTUREINFO {
    long left;
    long top;
    long width;
    long height;
    WORD x_density;
    WORD y_density;
    short colorDepth;
#ifdef _WIN64
    char dummy[2];
#endif
    HLOCAL hInfo;
};

#define SUSIE_PATH_MAX 200
typedef ULONG_PTR susie_time_t;

struct SUSIE_FINFO {
    unsigned char method[8];
    ULONG_PTR position;
    ULONG_PTR compsize;
    ULONG_PTR filesize;
    susie_time_t timestamp;
    char path[SUSIE_PATH_MAX];
    char filename[SUSIE_PATH_MAX];
    unsigned long crc;
#ifdef _WIN64
    char dummy[4];
#endif
};

struct SUSIE_FINFOW {
    unsigned char method[8];
    ULONG_PTR position;
    ULONG_PTR compsize;
    ULONG_PTR filesize;
    susie_time_t timestamp;
    WCHAR path[SUSIE_PATH_MAX];
    WCHAR filename[SUSIE_PATH_MAX];
    unsigned long crc;
#ifdef _WIN64
    char dummy[4];
#endif
};

#pragma pack(pop)

typedef int(__stdcall* SUSIE_PROGRESS)(int nNum, int nDenom, LONG_PTR lData);

constexpr int SUSIEERROR_NOERROR = 0;
constexpr int SUSIEERROR_NOTIMPL = -1;
constexpr int SUSIEERROR_USERCANCEL = 1;
constexpr int SUSIEERROR_UNKNOWNFORMAT = 2;
constexpr int SUSIEERROR_BROKENDATA = 3;
constexpr int SUSIEERROR_NOMEMORY = 4;
constexpr int SUSIEERROR_FAULTMEMORY = 5;
constexpr int SUSIEERROR_FAULTREAD = 6;
constexpr int SUSIEERROR_RESERVED = 7;
constexpr int SUSIEERROR_INTERNAL = 8;
constexpr int SUSIEERROR_FAULTWRITE = 9;
constexpr int SUSIEERROR_EOF = 10;

constexpr unsigned int SUSIE_INPUT_DISK = 0x0000;
constexpr unsigned int SUSIE_INPUT_MEMORY = 0x0001;
constexpr unsigned int SUSIE_OUTPUT_DISK = 0x0000;
constexpr unsigned int SUSIE_OUTPUT_MEMORY = 0x0100;
