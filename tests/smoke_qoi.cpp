// Loads ifmqoi.sph and decodes a 2x2 QOI it encodes itself.
//
//   smoke_qoi <scratch dir>
//
// Pixels (row-major, top-down): red, green, half-transparent blue, fully
// transparent. Alpha is composited onto the background colour, which comes
// from ifmqoi.ini next to the plugin or defaults to E0E0E0.

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <susie.h>
#include <qoi.h>

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); return 1; } } while (0)

typedef int (__stdcall *GETPICTUREINFOW_T)(LPCWSTR, LONG_PTR, unsigned int, SUSIE_PICTUREINFO*);
typedef int (__stdcall *GETPICTUREW_T)(LPCWSTR, LONG_PTR, unsigned int, HLOCAL*, HLOCAL*, SUSIE_PROGRESS, LONG_PTR);

static const uint8_t kPixels[4][4] = {
	{255, 0, 0, 255}, {0, 255, 0, 255},
	{0, 0, 255, 128}, {0, 0, 0, 0},
};

// Every pixel as QOI_OP_RGBA: no encoder cleverness, just a valid stream.
static std::vector<uint8_t> EncodeQoi()
{
	std::vector<uint8_t> q = { 'q', 'o', 'i', 'f', 0, 0, 0, 2, 0, 0, 0, 2, 4, QOI_SRGB };
	for (const auto& px : kPixels) {
		q.push_back(QOI_OP_RGBA);
		q.insert(q.end(), px, px + 4);
	}
	const uint8_t end[QOI_END_MARKER_SIZE] = {0, 0, 0, 0, 0, 0, 0, 1};
	q.insert(q.end(), end, end + QOI_END_MARKER_SIZE);
	return q;
}

static bool WriteFileBytes(const std::wstring& path, const void* data, size_t len)
{
	FILE* fp = _wfopen(path.c_str(), L"wb");
	if (!fp) return false;
	size_t n = fwrite(data, 1, len, fp);
	fclose(fp);
	return n == len;
}

static uint8_t Over(uint8_t src, uint8_t bg, uint8_t a) { return static_cast<uint8_t>((src * a + bg * (255 - a) + 127) / 255); }

// Loads the plugin, decodes qoiPath, and checks the composite against bg.
static int CheckRender(const std::wstring& qoiPath, uint8_t bg)
{
	HMODULE h = LoadLibraryW(PLUGIN_PATH);
	if (!h) fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
	CHECK(h != nullptr);
	auto getPictureW = reinterpret_cast<GETPICTUREW_T>(GetProcAddress(h, "GetPictureW"));
	CHECK(getPictureW);

	HLOCAL hInfo = nullptr, hBm = nullptr;
	CHECK(getPictureW(qoiPath.c_str(), 0, SUSIE_SOURCE_DISK, &hInfo, &hBm, nullptr, 0) == SUSIEERROR_NOERROR);
	auto* bmi = static_cast<BITMAPINFOHEADER*>(LocalLock(hInfo));
	auto* bits = static_cast<uint8_t*>(LocalLock(hBm));
	CHECK(bmi && bits);
	CHECK(bmi->biWidth == 2 && bmi->biHeight == 2 && bmi->biBitCount == 32);
	CHECK(LocalSize(hBm) >= 16);
	// Bottom-up: row 0 of the DIB is the bottom image row.
	const uint8_t* red   = bits + 4 * 2 + 0;
	const uint8_t* green = bits + 4 * 2 + 4;
	const uint8_t* blue  = bits + 0;
	const uint8_t* clear = bits + 4;
	CHECK(red[0] == 0 && red[1] == 0 && red[2] == 255);
	CHECK(green[0] == 0 && green[1] == 255 && green[2] == 0);
	CHECK(blue[0] == Over(255, bg, 128) && blue[1] == Over(0, bg, 128) && blue[2] == Over(0, bg, 128));
	CHECK(clear[0] == bg && clear[1] == bg && clear[2] == bg);
	LocalUnlock(hInfo);
	LocalUnlock(hBm);
	LocalFree(hInfo);
	LocalFree(hBm);
	FreeLibrary(h);
	return 0;
}

int wmain(int argc, wchar_t** argv)
{
	CHECK(argc == 2);
	const std::wstring dir = std::wstring(argv[1]) + L"\\αβγ_テスト";
	CreateDirectoryW(dir.c_str(), nullptr);
	const std::wstring qoiPath = dir + L"\\four.qoi";
	const std::vector<uint8_t> qoi = EncodeQoi();
	CHECK(WriteFileBytes(qoiPath, qoi.data(), qoi.size()));

	HMODULE h = LoadLibraryW(PLUGIN_PATH);
	if (!h) fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
	CHECK(h != nullptr);
	auto getPluginInfo   = reinterpret_cast<GETPLUGININFO>(GetProcAddress(h, "GetPluginInfo"));
	auto getPluginInfoW  = reinterpret_cast<GETPLUGININFOW>(GetProcAddress(h, "GetPluginInfoW"));
	auto isSupportedW    = reinterpret_cast<ISSUPPORTEDW>(GetProcAddress(h, "IsSupportedW"));
	auto getPictureInfoW = reinterpret_cast<GETPICTUREINFOW_T>(GetProcAddress(h, "GetPictureInfoW"));
	auto getPictureW     = reinterpret_cast<GETPICTUREW_T>(GetProcAddress(h, "GetPictureW"));
	CHECK(getPluginInfo && getPluginInfoW && isSupportedW && getPictureInfoW && getPictureW);
	CHECK(GetProcAddress(h, "qoi_decode") == nullptr);  // internals stay unexported

	char info[64];
	CHECK(getPluginInfo(0, info, sizeof(info)) == 4 && strcmp(info, "00IN") == 0);
	wchar_t infoW[64];
	CHECK(getPluginInfoW(0, infoW, 0) == 0);  // must not touch buf[-1]

	// dw: head bytes, a HANDLE-like small value, or NULL.
	CHECK(isSupportedW(L"x.bin", qoi.data()) == 1);
	CHECK(isSupportedW(L"x.qoi", "not a qoi") == 0);
	CHECK(isSupportedW(L"x.qoi", reinterpret_cast<const void*>(static_cast<uintptr_t>(0x4))) == 1);
	CHECK(isSupportedW(L"x.png", nullptr) == 0);

	// Header-only info from disk and from memory.
	SUSIE_PICTUREINFO pi{};
	CHECK(getPictureInfoW(qoiPath.c_str(), 0, SUSIE_SOURCE_DISK, &pi) == SUSIEERROR_NOERROR);
	CHECK(pi.width == 2 && pi.height == 2 && pi.colorDepth == 32);
	pi = {};
	CHECK(getPictureInfoW(reinterpret_cast<LPCWSTR>(qoi.data()), static_cast<LONG_PTR>(qoi.size()), SUSIE_SOURCE_MEM, &pi) == SUSIEERROR_NOERROR);
	CHECK(pi.width == 2 && pi.height == 2);

	// Truncated stream: header ok, decode fails.
	HLOCAL hInfo = nullptr, hBm = nullptr;
	CHECK(getPictureW(reinterpret_cast<LPCWSTR>(qoi.data()), QOI_HEADER_SIZE + 3, SUSIE_SOURCE_MEM, &hInfo, &hBm, nullptr, 0) == SUSIEERROR_BROKENDATA);
	CHECK(hInfo == nullptr && hBm == nullptr);
	CHECK(getPictureW(L"\\\\?\\does-not-exist.qoi", 0, SUSIE_SOURCE_DISK, &hInfo, &hBm, nullptr, 0) == SUSIEERROR_FAULTREAD);
	FreeLibrary(h);

	// Default background, then one set through the sibling INI.
	std::wstring ini = PLUGIN_PATH;
	ini.replace(ini.rfind(L'.'), std::wstring::npos, L".ini");
	DeleteFileW(ini.c_str());
	if (CheckRender(qoiPath, 0xE0) != 0) return 1;
	const char kIni[] = "[render]\r\nbackground_color=000000\r\n";
	CHECK(WriteFileBytes(ini, kIni, sizeof(kIni) - 1));
	int rc = CheckRender(qoiPath, 0x00);
	DeleteFileW(ini.c_str());
	if (rc != 0) return 1;

	puts("OK");
	return 0;
}
