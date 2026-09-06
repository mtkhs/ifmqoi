#include "susie.h"
#include "qoi.h"
#include <windows.h>
#include <vector>
#include <fstream>

static std::vector<uint8_t> read_file(LPCSTR filename)
{
	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file) {
		return {};
	}

	const std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<uint8_t> buffer(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
		return {};
	}

	return buffer;
}

static bool is_qoi_format(const void* data, size_t size)
{
	if (size < QOI_HEADER_SIZE) {
		return false;
	}

	const auto* bytes = static_cast<const uint8_t*>(data);
	const uint32_t magic = (static_cast<uint32_t>(bytes[0]) << 24) |
	                      (static_cast<uint32_t>(bytes[1]) << 16) |
	                      (static_cast<uint32_t>(bytes[2]) << 8) |
	                      static_cast<uint32_t>(bytes[3]);

	return magic == QOI_MAGIC;
}

static int SusieIsSupportedFromFile(const char*, const void* dw)
{
	if (dw == nullptr) {
		return 0;
	}
	return is_qoi_format(dw, 2048) ? 1 : 0;
}

static int SusieGetPictureInfoFromFile(const char* buf, LONG_PTR len, unsigned int flag, SUSIE_PICTUREINFO* lpInfo)
{
	if (lpInfo == nullptr) {
		return SUSIEERROR_INTERNAL;
	}

	std::vector<uint8_t> data;

	if ((flag & 0x07) == SUSIE_INPUT_DISK) {
		data = read_file(buf);
		if (data.empty()) {
			return SUSIEERROR_FAULTREAD;
		}
	} else if ((flag & 0x07) == SUSIE_INPUT_MEMORY) {
		if (buf == nullptr || len <= 0) {
			return SUSIEERROR_INTERNAL;
		}
		data.assign(reinterpret_cast<const uint8_t*>(buf),
		           reinterpret_cast<const uint8_t*>(buf) + len);
	} else {
		return SUSIEERROR_INTERNAL;
	}

	if (!is_qoi_format(data.data(), data.size())) {
		return SUSIEERROR_UNKNOWNFORMAT;
	}

	qoi_desc desc;
	void* pixels = qoi_decode(data.data(), static_cast<int>(data.size()), &desc, 0);
	if (pixels == nullptr) {
		return SUSIEERROR_BROKENDATA;
	}

	free(pixels);

	lpInfo->left = 0;
	lpInfo->top = 0;
	lpInfo->width = desc.width;
	lpInfo->height = desc.height;
	lpInfo->x_density = 0;
	lpInfo->y_density = 0;
	lpInfo->colorDepth = desc.channels * 8;
	lpInfo->hInfo = nullptr;

	return SUSIEERROR_NOERROR;
}

static int SusieGetPictureFromFile(const char* buf, LONG_PTR len, unsigned int flag, HLOCAL* pHBInfo, HLOCAL* pHBm, SUSIE_PROGRESS lpProgressCallback, LONG_PTR lData)
{
	if (pHBInfo == nullptr || pHBm == nullptr) {
		return SUSIEERROR_INTERNAL;
	}

	*pHBInfo = nullptr;
	*pHBm = nullptr;

	std::vector<uint8_t> data;

	if ((flag & 0x07) == SUSIE_INPUT_DISK) {
		data = read_file(buf);
		if (data.empty()) {
			return SUSIEERROR_FAULTREAD;
		}
	} else if ((flag & 0x07) == SUSIE_INPUT_MEMORY) {
		if (buf == nullptr || len <= 0) {
			return SUSIEERROR_INTERNAL;
		}
		data.assign(reinterpret_cast<const uint8_t*>(buf),
		           reinterpret_cast<const uint8_t*>(buf) + len);
	} else {
		return SUSIEERROR_INTERNAL;
	}

	if (!is_qoi_format(data.data(), data.size())) {
		return SUSIEERROR_UNKNOWNFORMAT;
	}

	if (lpProgressCallback != nullptr) {
		if (lpProgressCallback(0, 1, lData) != 0) {
			return SUSIEERROR_USERCANCEL;
		}
	}

	qoi_desc desc;
	void* pixels = qoi_decode(data.data(), static_cast<int>(data.size()), &desc, 4);
	if (pixels == nullptr) {
		return SUSIEERROR_BROKENDATA;
	}

	const DWORD info_size = sizeof(BITMAPINFOHEADER);
	*pHBInfo = LocalAlloc(LMEM_MOVEABLE, info_size);
	if (*pHBInfo == nullptr) {
		free(pixels);
		return SUSIEERROR_NOMEMORY;
	}

	auto* bmi = static_cast<BITMAPINFOHEADER*>(LocalLock(*pHBInfo));
	if (bmi == nullptr) {
		LocalFree(*pHBInfo);
		free(pixels);
		return SUSIEERROR_NOMEMORY;
	}

	bmi->biSize = sizeof(BITMAPINFOHEADER);
	bmi->biWidth = desc.width;
	bmi->biHeight = desc.height;
	bmi->biPlanes = 1;
	bmi->biBitCount = 32;
	bmi->biCompression = BI_RGB;
	bmi->biSizeImage = 0;
	bmi->biXPelsPerMeter = 0;
	bmi->biYPelsPerMeter = 0;
	bmi->biClrUsed = 0;
	bmi->biClrImportant = 0;

	LocalUnlock(*pHBInfo);

	const DWORD bitmap_size = desc.width * desc.height * 4;
	*pHBm = LocalAlloc(LMEM_MOVEABLE, bitmap_size);
	if (*pHBm == nullptr) {
		LocalFree(*pHBInfo);
		free(pixels);
		return SUSIEERROR_NOMEMORY;
	}

	auto* bitmap = static_cast<uint8_t*>(LocalLock(*pHBm));
	if (bitmap == nullptr) {
		LocalFree(*pHBInfo);
		LocalFree(*pHBm);
		free(pixels);
		return SUSIEERROR_NOMEMORY;
	}

	auto* src = static_cast<uint8_t*>(pixels);
	for (uint32_t y = 0; y < desc.height; y++) {
		for (uint32_t x = 0; x < desc.width; x++) {
			const uint32_t src_idx = ((desc.height - 1 - y) * desc.width + x) * 4;
			const uint32_t dst_idx = (y * desc.width + x) * 4;

			const float alpha = src[src_idx + 3] / 255.0f;
			const float inv_alpha = 1.0f - alpha;
			constexpr float bg_color = 224.0f;

			bitmap[dst_idx + 0] = static_cast<uint8_t>(src[src_idx + 2] * alpha + bg_color * inv_alpha);
			bitmap[dst_idx + 1] = static_cast<uint8_t>(src[src_idx + 1] * alpha + bg_color * inv_alpha);
			bitmap[dst_idx + 2] = static_cast<uint8_t>(src[src_idx + 0] * alpha + bg_color * inv_alpha);
			bitmap[dst_idx + 3] = 255;
		}
	}

	LocalUnlock(*pHBm);
	free(pixels);

	if (lpProgressCallback != nullptr) {
		lpProgressCallback(1, 1, lData);
	}

	return SUSIEERROR_NOERROR;
}

extern "C" int __stdcall GetPluginInfo(int infono, LPSTR buf, int buflen)
{
	if ( infono && (buflen < 64) ) infono = -1;
	switch (infono){
		case 0:
			lstrcpyA(buf, "00IN");
			break;
		case 1:
			lstrcpyA(buf, "QOI Plug-in Version 0.1 (C) mtkhs");
			break;
		case 2:
			lstrcpyA(buf, "*.qoi");
			break;
		case 3:
			lstrcpyA(buf, "Quite OK Image format (*.qoi)");
			break;
		default:
			buf[0] = '\0';
			break;
	}
	return static_cast<int>(lstrlenA(buf));
}

extern "C" int __stdcall GetPluginInfoW(int infono, LPWSTR buf, int buflen)
{
	char bufA[1024];

	if ( infono && (buflen < 64) ){
		buf[0] = L'\0';
		return 0;
	}
	GetPluginInfo(infono, bufA, 1024);
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, bufA, -1, buf, buflen);
	buf[buflen - 1] = L'\0';
	return static_cast<int>(lstrlenW(buf));
}

extern "C" int __stdcall IsSupported(LPCSTR filename, const void* dw)
{
	return SusieIsSupportedFromFile(filename, dw);
}

extern "C" int __stdcall IsSupportedW(LPCWSTR filename, const void* dw)
{
	char filenameA[MAX_PATH];
	WideCharToMultiByte(CP_ACP, 0, filename, -1, filenameA, MAX_PATH, nullptr, nullptr);
	return SusieIsSupportedFromFile(filenameA, dw);
}

extern "C" int __stdcall GetPictureInfo(LPCSTR buf, LONG_PTR len, unsigned int flag, SUSIE_PICTUREINFO* lpInfo)
{
	return SusieGetPictureInfoFromFile(buf, len, flag, lpInfo);
}

extern "C" int __stdcall GetPictureInfoW(LPCWSTR buf, LONG_PTR len, unsigned int flag, SUSIE_PICTUREINFO* lpInfo)
{
	if ((flag & 0x07) == SUSIE_INPUT_DISK) {
		char bufA[MAX_PATH];
		WideCharToMultiByte(CP_ACP, 0, buf, -1, bufA, MAX_PATH, nullptr, nullptr);
		return SusieGetPictureInfoFromFile(bufA, len, flag, lpInfo);
	} else {
		return SusieGetPictureInfoFromFile(reinterpret_cast<const char*>(buf), len, flag, lpInfo);
	}
}

extern "C" int __stdcall GetPicture(LPCSTR buf, LONG_PTR len, unsigned int flag, HLOCAL* pHBInfo, HLOCAL* pHBm, SUSIE_PROGRESS lpProgressCallback, LONG_PTR lData)
{
	return SusieGetPictureFromFile(buf, len, flag, pHBInfo, pHBm, lpProgressCallback, lData);
}

extern "C" int __stdcall GetPictureW(LPCWSTR buf, LONG_PTR len, unsigned int flag, HLOCAL* pHBInfo, HLOCAL* pHBm, SUSIE_PROGRESS lpProgressCallback, LONG_PTR lData)
{
	if ((flag & 0x07) == SUSIE_INPUT_DISK) {
		char bufA[MAX_PATH];
		WideCharToMultiByte(CP_ACP, 0, buf, -1, bufA, MAX_PATH, nullptr, nullptr);
		return SusieGetPictureFromFile(bufA, len, flag, pHBInfo, pHBm, lpProgressCallback, lData);
	} else {
		return SusieGetPictureFromFile(reinterpret_cast<const char*>(buf), len, flag, pHBInfo, pHBm, lpProgressCallback, lData);
	}
}
