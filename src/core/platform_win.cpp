// Windows implementation of the platform layer — the real account-switch surface.
// Ports the winreg / subprocess(CREATE_NO_WINDOW) / EnumWindows / ShellExecute
// bits of switcher.py and steam_paths.py. Windows is the priority platform.

#if defined(_WIN32)

#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>  // CreateToolhelp32Snapshot / PROCESSENTRY32W (process listing)

#include <string>
#include <vector>

namespace ss::platform {

namespace {

HKEY hiveHandle(Hive h) { return h == Hive::CurrentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE; }

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

// CREATE_NO_WINDOW: keep tasklist / steam -shutdown from flashing a console when
// we run under the windowless GUI binary (port of switcher._no_window()).
constexpr DWORD kNoWindow = CREATE_NO_WINDOW;

std::wstring quoteJoin(const std::vector<std::string>& argv) {
    std::wstring cmd;
    for (size_t i = 0; i < argv.size(); ++i) {
        if (i) cmd += L' ';
        cmd += L'"' + widen(argv[i]) + L'"';
    }
    return cmd;
}

bool spawn(const std::vector<std::string>& argv, bool wait, const std::string& cwd = {}) {
    if (argv.empty()) return false;
    std::wstring cmd = quoteJoin(argv);
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    std::wstring wdir = widen(cwd);
    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE,
                        kNoWindow, nullptr, cwd.empty() ? nullptr : wdir.c_str(), &si, &pi))
        return false;
    if (wait) WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

}  // namespace

std::optional<std::string> regReadString(Hive hive, const std::string& subkey, const std::string& name) {
    HKEY key;
    if (RegOpenKeyExW(hiveHandle(hive), widen(subkey).c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return std::nullopt;
    wchar_t data[1024]; DWORD len = sizeof(data); DWORD type = 0;
    LONG r = RegQueryValueExW(key, widen(name).c_str(), nullptr, &type, (LPBYTE)data, &len);
    RegCloseKey(key);
    if (r != ERROR_SUCCESS || type != REG_SZ) return std::nullopt;
    return narrow(std::wstring(data, len / sizeof(wchar_t) ? (len / sizeof(wchar_t)) - 1 : 0));
}

std::optional<uint32_t> regReadDword(Hive hive, const std::string& subkey, const std::string& name) {
    HKEY key;
    if (RegOpenKeyExW(hiveHandle(hive), widen(subkey).c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return std::nullopt;
    DWORD data = 0, len = sizeof(data), type = 0;
    LONG r = RegQueryValueExW(key, widen(name).c_str(), nullptr, &type, (LPBYTE)&data, &len);
    RegCloseKey(key);
    if (r != ERROR_SUCCESS || type != REG_DWORD) return std::nullopt;
    return static_cast<uint32_t>(data);
}

bool regWriteString(Hive hive, const std::string& subkey, const std::string& name, const std::string& value) {
    HKEY key;
    if (RegCreateKeyExW(hiveHandle(hive), widen(subkey).c_str(), 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    std::wstring w = widen(value);
    LONG r = RegSetValueExW(key, widen(name).c_str(), 0, REG_SZ,
                            (const BYTE*)w.c_str(), (DWORD)((w.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool regWriteDword(Hive hive, const std::string& subkey, const std::string& name, uint32_t value) {
    HKEY key;
    if (RegCreateKeyExW(hiveHandle(hive), widen(subkey).c_str(), 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return false;
    DWORD d = value;
    LONG r = RegSetValueExW(key, widen(name).c_str(), 0, REG_DWORD, (const BYTE*)&d, sizeof(d));
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool regDeleteValue(Hive hive, const std::string& subkey, const std::string& name) {
    HKEY key;
    LONG r = RegOpenKeyExW(hiveHandle(hive), widen(subkey).c_str(), 0, KEY_SET_VALUE, &key);
    if (r == ERROR_FILE_NOT_FOUND) return true;   // no key -> nothing to delete
    if (r != ERROR_SUCCESS) return false;
    r = RegDeleteValueW(key, widen(name).c_str());
    RegCloseKey(key);
    return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
}

std::vector<std::string> regSubKeys(Hive hive, const std::string& subkey) {
    std::vector<std::string> out;
    HKEY key;
    // KEY_WOW64_64KEY so a 64-bit build still sees keys written under the same view;
    // GOG's own keys live under WOW6432Node, addressed explicitly by the caller.
    if (RegOpenKeyExW(hiveHandle(hive), widen(subkey).c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return out;
    wchar_t name[256];
    for (DWORD i = 0;; ++i) {
        DWORD len = 256;
        LONG r = RegEnumKeyExW(key, i, name, &len, nullptr, nullptr, nullptr, nullptr);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != ERROR_SUCCESS) break;
        out.push_back(narrow(std::wstring(name, len)));
    }
    RegCloseKey(key);
    return out;
}

bool processRunning(const std::string& imageName) {
    // Use the Toolhelp snapshot rather than parsing tasklist text.
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    std::wstring want = widen(imageName);
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, want.c_str()) == 0) { found = true; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

void forceKill(const std::string& imageName) {
    spawn({"taskkill", "/F", "/T", "/IM", imageName}, true);
}

void runWait(const std::vector<std::string>& argv) { spawn(argv, true); }

void spawnDetached(const std::vector<std::string>& argv) { spawn(argv, false); }

void spawnDetached(const std::vector<std::string>& argv, const std::string& workingDir) {
    spawn(argv, false, workingDir);
}

void openUri(const std::string& uri) {
    ShellExecuteW(nullptr, L"open", widen(uri).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

namespace {
void put16(std::string& s, uint16_t v) {
    s.push_back((char)(v & 0xFF)); s.push_back((char)((v >> 8) & 0xFF));
}
void put32(std::string& s, uint32_t v) {
    s.push_back((char)(v & 0xFF));        s.push_back((char)((v >> 8) & 0xFF));
    s.push_back((char)((v >> 16) & 0xFF)); s.push_back((char)((v >> 24) & 0xFF));
}

// Wrap a top-down BGRA pixel buffer in a 32-bpp BMP (BITMAPV4HEADER with an alpha
// mask, written bottom-up). Qt's image loader reads this incl. the alpha channel;
// even if a reader ignores alpha, transparent pixels read as black — which blends
// into ORBIT's near-black tiles, so it degrades gracefully.
std::string bmpFromBGRA(const std::vector<unsigned char>& px, int w, int h) {
    const uint32_t header = 14 + 108;
    const uint32_t imgSize = (uint32_t)w * (uint32_t)h * 4;
    std::string s;
    s.reserve(header + imgSize);
    s.push_back('B'); s.push_back('M');          // BITMAPFILEHEADER
    put32(s, header + imgSize);                  // bfSize
    put32(s, 0);                                 // bfReserved
    put32(s, header);                            // bfOffBits
    put32(s, 108);                               // BITMAPV4HEADER biSize
    put32(s, (uint32_t)w);
    put32(s, (uint32_t)h);                       // positive: bottom-up rows
    put16(s, 1);                                 // planes
    put16(s, 32);                                // bpp
    put32(s, 3);                                 // BI_BITFIELDS
    put32(s, imgSize);
    put32(s, 2835); put32(s, 2835);              // ~72 DPI
    put32(s, 0); put32(s, 0);                    // clrUsed / clrImportant
    put32(s, 0x00FF0000);                        // red mask
    put32(s, 0x0000FF00);                        // green mask
    put32(s, 0x000000FF);                        // blue mask
    put32(s, 0xFF000000);                        // alpha mask
    put32(s, 0x73524742);                        // 'sRGB' color space
    for (int i = 0; i < 12; ++i) put32(s, 0);    // endpoints (9) + gamma (3)
    for (int y = h - 1; y >= 0; --y)             // bottom-up
        s.append((const char*)&px[(size_t)y * w * 4], (size_t)w * 4);
    return s;
}
}  // namespace

std::optional<std::string> exeIcon(const std::string& exePath) {
    if (exePath.empty()) return std::nullopt;
    HICON hicon = nullptr;
    // Ask for 256px; PrivateExtractIcons returns the best resource up to that size.
    UINT got = PrivateExtractIconsW(widen(exePath).c_str(), 0, 256, 256, &hicon,
                                    nullptr, 1, LR_DEFAULTCOLOR);
    if (got == 0 || !hicon) return std::nullopt;

    std::optional<std::string> out;
    ICONINFO ii{};
    if (GetIconInfo(hicon, &ii)) {
        BITMAP bm{};
        if (GetObject(ii.hbmColor, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
            int w = bm.bmWidth, h = bm.bmHeight;
            BITMAPINFO bi{};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = w;
            bi.bmiHeader.biHeight = -h;          // top-down
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            std::vector<unsigned char> px((size_t)w * h * 4);
            HDC dc = GetDC(nullptr);
            if (GetDIBits(dc, ii.hbmColor, 0, h, px.data(), &bi, DIB_RGB_COLORS) == h) {
                // Older icons store no alpha in the color plane — derive it from the
                // AND mask (a set mask bit = transparent pixel).
                bool hasAlpha = false;
                for (size_t i = 3; i < px.size(); i += 4) if (px[i]) { hasAlpha = true; break; }
                if (!hasAlpha) {
                    std::vector<unsigned char> mask((size_t)w * h * 4);
                    if (GetDIBits(dc, ii.hbmMask, 0, h, mask.data(), &bi, DIB_RGB_COLORS) == h)
                        for (size_t i = 0; i < px.size(); i += 4) px[i + 3] = mask[i] ? 0 : 255;
                    else
                        for (size_t i = 0; i < px.size(); i += 4) px[i + 3] = 255;
                }
                out = bmpFromBGRA(px, w, h);
            }
            ReleaseDC(nullptr, dc);
        }
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
    }
    DestroyIcon(hicon);
    return out;
}

namespace {
struct EnumCtx { bool found = false; };
BOOL CALLBACK enumProc(HWND hwnd, LPARAM lparam) {
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t cls[256] = {0}, title[256] = {0};
    GetClassNameW(hwnd, cls, 256);
    GetWindowTextW(hwnd, title, 256);
    std::wstring c(cls), t(title);
    // Exact title "Steam"; lenient on class (Valve has changed it).
    if (t == L"Steam" && (c == L"SDL_app" || c.find(L"Steam") != std::wstring::npos)) {
        ((EnumCtx*)lparam)->found = true;
        return FALSE;
    }
    return TRUE;
}
}  // namespace

bool steamWindowPresent() {
    EnumCtx ctx;
    EnumWindows(enumProc, (LPARAM)&ctx);
    return ctx.found;
}

std::optional<std::string> getEnv(const std::string& name) {
    wchar_t buf[32767];
    DWORD n = GetEnvironmentVariableW(widen(name).c_str(), buf, 32767);
    if (n == 0 || n >= 32767) return std::nullopt;
    std::string v = narrow(std::wstring(buf, n));
    if (v.empty()) return std::nullopt;
    return v;
}

std::string homeDir() {
    if (auto h = getEnv("USERPROFILE")) return *h;
    return "";
}

}  // namespace ss::platform

#endif  // _WIN32
