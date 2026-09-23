#include <windows.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "susie.h"
#include "qoi.h"

namespace {

const char* kInfoA[] = { "00IN", "QOI Plug-in Version 0.2 (C) mtkhs", "*.qoi", "Quite OK Image format (*.qoi)" };
constexpr int kInfoCount = 4;

// The DIB has no alpha channel, so pixels are composited onto this colour.
// Overridable per install: [render] background_color=RRGGBB in ifmqoi.ini
// next to the plugin.
constexpr uint32_t kDefaultBackground = 0xE0E0E0u;

// Sibling INI path: same dir/basename as this DLL with .ini extension.
bool OwnIniPath(wchar_t* out, size_t cap)
{
	HMODULE self = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                        reinterpret_cast<LPCWSTR>(&OwnIniPath), &self)) {
		return false;
	}
	DWORD n = GetModuleFileNameW(self, out, static_cast<DWORD>(cap));
	if (n == 0 || n >= cap) return false;
	wchar_t* dot = wcsrchr(out, L'.');
	if (!dot || static_cast<size_t>(dot - out) + 5 > cap) return false;  // need ".ini\0"
	wcscpy(dot, L".ini");
	return true;
}

// Cached on first call. INI changes require reloading the plugin.
uint32_t BackgroundColor()
{
	static uint32_t color = [] {
		wchar_t ini[MAX_PATH];
		if (!OwnIniPath(ini, MAX_PATH)) return kDefaultBackground;
		wchar_t hex[16];
		GetPrivateProfileStringW(L"render", L"background_color", L"", hex, 16, ini);
		if (wcslen(hex) != 6) return kDefaultBackground;
		wchar_t* end = nullptr;
		unsigned long v = wcstoul(hex, &end, 16);
		return (end && *end == L'\0') ? static_cast<uint32_t>(v & 0xFFFFFF) : kDefaultBackground;
	}();
	return color;
}

std::wstring AnsiToWide(const char* ansi)
{
	if (!ansi) return L"";
	int n = MultiByteToWideChar(CP_ACP, 0, ansi, -1, nullptr, 0);
	if (n <= 1) return L"";
	std::wstring out(static_cast<size_t>(n - 1), L'\0');
	MultiByteToWideChar(CP_ACP, 0, ansi, -1, &out[0], n);
	return out;
}

// IsSupported's dw is either a pointer to the first SUSIE_CHECK_SIZE bytes or
// a Windows HANDLE (small integer) cast to void*. User-mode addresses are
// always above 64 KB.
const uint8_t* ToHeadPtr(const void* dw)
{
	return (reinterpret_cast<uintptr_t>(dw) > 0xFFFF) ? static_cast<const uint8_t*>(dw) : nullptr;
}

bool HasQoiExtension(const wchar_t* filename)
{
	const wchar_t* dot = wcsrchr(filename, L'.');
	return dot && _wcsicmp(dot + 1, L"qoi") == 0;
}

bool IsQoi(const uint8_t* p, size_t n)
{
	int pos = 0;
	return n >= QOI_HEADER_SIZE && qoi_read_32(p, &pos) == QOI_MAGIC;
}

std::vector<uint8_t> ReadFile(const wchar_t* path)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) return {};
	const std::streamsize size = f.tellg();
	if (size <= 0) return {};
	std::vector<uint8_t> buf(static_cast<size_t>(size));
	f.seekg(0);
	if (!f.read(reinterpret_cast<char*>(buf.data()), size)) return {};
	return buf;
}

// buf is a path (A or W per isW) for disk input, or the bytes for memory input.
int LoadInput(const void* buf, LONG_PTR len, unsigned int flag, bool isW, std::vector<uint8_t>& data)
{
	switch (flag & SUSIE_SOURCE_MASK) {
	case SUSIE_SOURCE_MEM: {
		if (len <= 0) return SUSIEERROR_INTERNAL;
		const auto* p = static_cast<const uint8_t*>(buf);
		data.assign(p, p + len);
		return SUSIEERROR_NOERROR;
	}
	case SUSIE_SOURCE_DISK: {
		std::wstring path = isW ? std::wstring(static_cast<LPCWSTR>(buf)) : AnsiToWide(static_cast<LPCSTR>(buf));
		data = ReadFile(path.c_str());
		return data.empty() ? SUSIEERROR_FAULTREAD : SUSIEERROR_NOERROR;
	}
	default:
		return SUSIEERROR_NOTSUPPORT;
	}
}

int GetPictureInfoImpl(const void* buf, LONG_PTR len, unsigned int flag, bool isW, SUSIE_PICTUREINFO* lpInfo)
{
	std::vector<uint8_t> data;
	int rc = LoadInput(buf, len, flag, isW, data);
	if (rc != SUSIEERROR_NOERROR) return rc;
	if (!IsQoi(data.data(), data.size())) return SUSIEERROR_UNKNOWNFORMAT;

	qoi_desc desc;
	if (!qoi_read_header(data.data(), static_cast<int>(data.size()), &desc)) return SUSIEERROR_BROKENDATA;

	lpInfo->left = 0;
	lpInfo->top = 0;
	lpInfo->width = static_cast<long>(desc.width);
	lpInfo->height = static_cast<long>(desc.height);
	lpInfo->x_density = 0;
	lpInfo->y_density = 0;
	lpInfo->colorDepth = desc.channels * 8;
	lpInfo->hInfo = nullptr;
	return SUSIEERROR_NOERROR;
}

inline uint8_t Composite(uint8_t src, uint8_t bg, uint8_t alpha)
{
	return static_cast<uint8_t>((src * alpha + bg * (255 - alpha) + 127) / 255);
}

int GetPictureImpl(const void* buf, LONG_PTR len, unsigned int flag, bool isW,
                   HLOCAL* pHBInfo, HLOCAL* pHBm, SUSIE_PROGRESS progress, LONG_PTR lData)
{
	*pHBInfo = nullptr;
	*pHBm = nullptr;

	std::vector<uint8_t> data;
	int rc = LoadInput(buf, len, flag, isW, data);
	if (rc != SUSIEERROR_NOERROR) return rc;
	if (!IsQoi(data.data(), data.size())) return SUSIEERROR_UNKNOWNFORMAT;

	if (progress && progress(0, 1, lData) != 0) return SUSIEERROR_USERCANCEL;

	qoi_desc desc;
	auto* pixels = static_cast<uint8_t*>(qoi_decode(data.data(), static_cast<int>(data.size()), &desc, 4));
	if (!pixels) return SUSIEERROR_BROKENDATA;

	// 32bpp rows are 4-byte aligned by construction.
	const size_t bitmapSize = static_cast<size_t>(desc.width) * desc.height * 4;
	HLOCAL hInfo = LocalAlloc(LMEM_MOVEABLE | LMEM_ZEROINIT, sizeof(BITMAPINFOHEADER));
	HLOCAL hBits = LocalAlloc(LMEM_MOVEABLE, bitmapSize);
	auto* bmi = hInfo ? static_cast<BITMAPINFOHEADER*>(LocalLock(hInfo)) : nullptr;
	auto* bits = hBits ? static_cast<uint8_t*>(LocalLock(hBits)) : nullptr;
	if (!bmi || !bits) {
		if (bmi) LocalUnlock(hInfo);
		if (bits) LocalUnlock(hBits);
		if (hInfo) LocalFree(hInfo);
		if (hBits) LocalFree(hBits);
		free(pixels);
		return SUSIEERROR_EMPTYMEMORY;
	}

	bmi->biSize = sizeof(BITMAPINFOHEADER);
	bmi->biWidth = static_cast<LONG>(desc.width);
	bmi->biHeight = static_cast<LONG>(desc.height);
	bmi->biPlanes = 1;
	bmi->biBitCount = 32;
	bmi->biCompression = BI_RGB;
	bmi->biSizeImage = static_cast<DWORD>(bitmapSize);

	const uint32_t bg = BackgroundColor();
	const uint8_t bgR = static_cast<uint8_t>(bg >> 16), bgG = static_cast<uint8_t>(bg >> 8), bgB = static_cast<uint8_t>(bg);
	for (uint32_t y = 0; y < desc.height; y++) {
		const uint8_t* src = pixels + static_cast<size_t>(y) * desc.width * 4;
		uint8_t* dst = bits + static_cast<size_t>(desc.height - 1 - y) * desc.width * 4;  // bottom-up
		for (uint32_t x = 0; x < desc.width; x++, src += 4, dst += 4) {
			const uint8_t a = src[3];
			dst[0] = Composite(src[2], bgB, a);
			dst[1] = Composite(src[1], bgG, a);
			dst[2] = Composite(src[0], bgR, a);
			dst[3] = 255;
		}
	}

	LocalUnlock(hInfo);
	LocalUnlock(hBits);
	free(pixels);

	if (progress) progress(1, 1, lData);

	*pHBInfo = hInfo;
	*pHBm = hBits;
	return SUSIEERROR_NOERROR;
}

}  // namespace

extern "C" {

int __stdcall GetPluginInfo(int infono, LPSTR buf, int buflen)
{
	if (!buf || buflen <= 0) return 0;
	if (infono < 0 || infono >= kInfoCount) { buf[0] = '\0'; return 0; }
	int n = static_cast<int>(strlen(kInfoA[infono]));
	if (n >= buflen) n = buflen - 1;
	memcpy(buf, kInfoA[infono], n);
	buf[n] = '\0';
	return n;
}

int __stdcall GetPluginInfoW(int infono, LPWSTR buf, int buflen)
{
	if (!buf || buflen <= 0) return 0;
	char bufA[256];
	int n = GetPluginInfo(infono, bufA, sizeof(bufA));
	if (n == 0) { buf[0] = L'\0'; return 0; }
	MultiByteToWideChar(CP_ACP, 0, bufA, -1, buf, buflen);
	buf[buflen - 1] = L'\0';
	return static_cast<int>(wcslen(buf));
}

int __stdcall IsSupported(LPCSTR filename, const void* dw)
{
	try {
		if (!filename) return 0;
		const uint8_t* head = ToHeadPtr(dw);
		if (head) return IsQoi(head, SUSIE_CHECK_SIZE) ? 1 : 0;
		return HasQoiExtension(AnsiToWide(filename).c_str()) ? 1 : 0;
	} catch (...) { return 0; }
}

int __stdcall IsSupportedW(LPCWSTR filename, const void* dw)
{
	if (!filename) return 0;
	const uint8_t* head = ToHeadPtr(dw);
	if (head) return IsQoi(head, SUSIE_CHECK_SIZE) ? 1 : 0;
	return HasQoiExtension(filename) ? 1 : 0;
}

int __stdcall GetPictureInfo(LPCSTR buf, LONG_PTR len, unsigned int flag, SUSIE_PICTUREINFO* lpInfo)
{
	try {
		if (!buf || !lpInfo) return SUSIEERROR_INTERNAL;
		return GetPictureInfoImpl(buf, len, flag, false, lpInfo);
	} catch (...) { return SUSIEERROR_INTERNAL; }
}

int __stdcall GetPictureInfoW(LPCWSTR buf, LONG_PTR len, unsigned int flag, SUSIE_PICTUREINFO* lpInfo)
{
	try {
		if (!buf || !lpInfo) return SUSIEERROR_INTERNAL;
		return GetPictureInfoImpl(buf, len, flag, true, lpInfo);
	} catch (...) { return SUSIEERROR_INTERNAL; }
}

int __stdcall GetPicture(LPCSTR buf, LONG_PTR len, unsigned int flag, HLOCAL* pHBInfo, HLOCAL* pHBm, SUSIE_PROGRESS progress, LONG_PTR lData)
{
	try {
		if (!buf || !pHBInfo || !pHBm) return SUSIEERROR_INTERNAL;
		return GetPictureImpl(buf, len, flag, false, pHBInfo, pHBm, progress, lData);
	} catch (...) { return SUSIEERROR_INTERNAL; }
}

int __stdcall GetPictureW(LPCWSTR buf, LONG_PTR len, unsigned int flag, HLOCAL* pHBInfo, HLOCAL* pHBm, SUSIE_PROGRESS progress, LONG_PTR lData)
{
	try {
		if (!buf || !pHBInfo || !pHBm) return SUSIEERROR_INTERNAL;
		return GetPictureImpl(buf, len, flag, true, pHBInfo, pHBm, progress, lData);
	} catch (...) { return SUSIEERROR_INTERNAL; }
}

}  // extern "C"
