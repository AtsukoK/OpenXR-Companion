#pragma once
// Shared by the test files: includes, CHECK and saving pictures for checking by eye.
#include <sstream>
#include <filesystem>
#include <fstream>
#include "../src/panel.h"
#include <dxgi1_4.h>
#include "../src/keys.h"
#include "../src/screen.h"
#include "../src/mirror.h"
#include "../src/game_offsets.h"
#include "../src/overlay.h"
#include "../src/game_input.h"
#include <thread>
#include <cstdio>
#include <objidl.h>
#include <algorithm>
namespace Gdiplus {
using std::max;
using std::min;
} // namespace Gdiplus
#include <gdiplus.h>
using namespace Gdiplus;

inline int fails = 0;
#define CHECK(c, msg)                                                                                                                      \
    do {                                                                                                                                   \
        if (!(c)) {                                                                                                                        \
            printf("FAIL: %s\n", msg);                                                                                                     \
            fails++;                                                                                                                       \
        } else                                                                                                                             \
            printf("ok:   %s\n", msg);                                                                                                     \
    } while (0)

inline void SavePng(const std::vector<uint32_t>& px, int w, int h, const wchar_t* path) {
    Bitmap bmp(w, h, w * 4, PixelFormat32bppPARGB, (BYTE*)px.data());
    CLSID png;
    UINT n = 0, sz = 0;
    GetImageEncodersSize(&n, &sz);
    std::vector<BYTE> buf(sz);
    auto* enc = (ImageCodecInfo*)buf.data();
    GetImageEncoders(n, sz, enc);
    for (UINT i = 0; i < n; i++)
        if (wcscmp(enc[i].MimeType, L"image/png") == 0) png = enc[i].Clsid;
    bmp.Save(path, &png);
}
