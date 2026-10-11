// StarExtract Native - tools: engines, vault, hash, identify, context menu
#include "se.h"
#include <algorithm>
#include <map>
#include <functional>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <cwchar>

// ---------- paths ----------
std::wstring GetExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileName(nullptr, path, MAX_PATH);
    std::wstring p(path);
    size_t pos = p.find_last_of(L"\\/");
    return pos != std::wstring::npos ? p.substr(0, pos) : L"";
}
std::wstring Get7zPath() { return GetExeDir() + L"\\7z.exe"; }
std::wstring GetLessMsiPath() { return GetExeDir() + L"\\bin\\lessmsi\\lessmsi.exe"; }
std::wstring GetInnounpPath() { return GetExeDir() + L"\\bin\\innounp.exe"; }
std::wstring GetUnshieldPath() { return GetExeDir() + L"\\bin\\unshield.exe"; }
std::wstring GetPeaPath() { return GetExeDir() + L"\\bin\\peazip\\pea.exe"; }
std::wstring GetWisePath() { return GetExeDir() + L"\\bin\\E_WISE_W.EXE"; }
std::wstring GetTridPath() { return GetExeDir() + L"\\bin\\trid.exe"; }

bool FileExists(const std::wstring& path) {
    DWORD att = GetFileAttributes(path.c_str());
    return att != INVALID_FILE_ATTRIBUTES && !(att & FILE_ATTRIBUTE_DIRECTORY);
}
bool DirectoryExists(const std::wstring& path) {
    DWORD att = GetFileAttributes(path.c_str());
    return att != INVALID_FILE_ATTRIBUTES && (att & FILE_ATTRIBUTE_DIRECTORY);
}
std::wstring GetWindowTextString(HWND hwnd) {
    int len = GetWindowTextLength(hwnd);
    if (len <= 0) return L"";
    std::wstring buf((size_t)len + 1, L'\0');
    GetWindowText(hwnd, &buf[0], len + 1);
    buf.resize((size_t)len);
    return buf;
}
void SetWindowTextString(HWND hwnd, const std::wstring& text) { SetWindowText(hwnd, text.c_str()); }

void RunProcess(const std::wstring& exe, const std::wstring& args, const std::wstring& workDir) {
    SHELLEXECUTEINFO sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.hwnd = g_state.hwndMain;
    sei.lpVerb = L"open";
    sei.lpFile = exe.c_str();
    sei.lpParameters = args.c_str();
    sei.lpDirectory = workDir.empty() ? nullptr : workDir.c_str();
    sei.nShow = SW_HIDE;
    if (ShellExecuteEx(&sei) && sei.hProcess) {
        WaitForSingleObject(sei.hProcess, INFINITE);
        CloseHandle(sei.hProcess);
    }
}

// ---------- streaming worker (real progress + cancel) ----------
// A tool runs on its own thread with stdout+stderr merged into one pipe. Every
// complete line is posted to the UI as WM_APP_STREAM_LINE; the exit code is
// posted as WM_APP_STREAM_DONE. Nothing here touches the UI directly.
struct StreamJob {
    HANDLE proc = nullptr;
    std::atomic<bool> cancel{ false };
    HWND notify = nullptr;
    std::atomic<bool> busy{ false };
};
static StreamJob g_stream;

static DWORD WINAPI StreamThread(LPVOID param) {
    std::wstring* job = (std::wstring*)param; // "<exe>\n<args>"
    size_t sep = job->find(L'\n');
    std::wstring exe = job->substr(0, sep);
    std::wstring args = job->substr(sep + 1);
    delete job;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    LONG exitCode = -1;
    if (CreatePipe(&rd, &wr, &sa, 0)) {
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        si.hStdOutput = wr;
        si.hStdError = wr;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION pi = {};

        std::wstring cmd = L"\"" + exe + L"\" " + args;
        std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
        mutableCmd.push_back(L'\0');

        if (CreateProcessW(exe.c_str(), &mutableCmd[0], nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            g_stream.proc = pi.hProcess;
            std::string pending;
            char buf[4096];
            while (!g_stream.cancel.load()) {
                DWORD avail = 0;
                if (!PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr)) break;
                if (avail == 0) {
                    if (WaitForSingleObject(pi.hProcess, 30) == WAIT_OBJECT_0) break;
                    continue;
                }
                DWORD got = 0;
                if (!ReadFile(rd, buf, sizeof(buf), &got, nullptr) || got == 0) break;
                pending.append(buf, got);
                size_t nl;
                while ((nl = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, nl);
                    pending.erase(0, nl + 1);
                    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                    if (line.empty()) continue;
                    int wlen = MultiByteToWideChar(CP_UTF8, 0, line.data(), (int)line.size(), nullptr, 0);
                    std::wstring wline;
                    if (wlen > 0) {
                        wline.assign((size_t)wlen, L'\0');
                        MultiByteToWideChar(CP_UTF8, 0, line.data(), (int)line.size(), &wline[0], wlen);
                    }
                    PostMessage(g_stream.notify, WM_APP_STREAM_LINE, 0, (LPARAM)new std::wstring(wline));
                }
            }
            if (g_stream.cancel.load()) TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            DWORD code = 0;
            GetExitCodeProcess(pi.hProcess, &code);
            exitCode = g_stream.cancel.load() ? -2 : (LONG)code;            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
        g_stream.proc = nullptr;
        CloseHandle(wr);
        CloseHandle(rd);
    }
    PostMessage(g_stream.notify, WM_APP_STREAM_DONE, (WPARAM)(LONG)exitCode, 0);
    g_stream.busy.store(false);
    return 0;
}

bool RunProcessStream(const std::wstring& exe, const std::wstring& args, HWND notify) {
    if (g_stream.busy.load()) return false;
    if (!FileExists(exe)) return false;
    g_stream.cancel.store(false);
    g_stream.notify = notify;
    g_stream.busy.store(true);
    std::wstring* job = new std::wstring(exe + L"\n" + args);
    HANDLE th = CreateThread(nullptr, 0, StreamThread, job, 0, nullptr);
    if (!th) {
        delete job;
        g_stream.busy.store(false);
        return false;
    }
    CloseHandle(th);
    return true;
}

void CancelStreamedProcess() {
    if (g_stream.busy.load()) g_stream.cancel.store(true);
}

bool IsStreamedProcessRunning() { return g_stream.busy.load(); }

// ---------- formatting ----------
std::wstring FormatSize(unsigned long long bytes) {
    if (bytes == 0) return L"0 B";
    if (bytes < 1024ull) return std::to_wstring(bytes) + L" B";
    if (bytes < 1024ull * 1024) {
        wchar_t b[64]; swprintf(b, 64, L"%.1f KB", bytes / 1024.0); return b;
    }
    if (bytes < 1024ull * 1024 * 1024) {
        wchar_t b[64]; swprintf(b, 64, L"%.2f MB", bytes / (1024.0 * 1024.0)); return b;
    }
    wchar_t b[64]; swprintf(b, 64, L"%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0)); return b;
}

std::wstring RunProcessCapture(const std::wstring& exe, const std::wstring& args) {
    std::wstring result;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return result;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    PROCESS_INFORMATION pi = {};
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    std::wstring cmd = L"\"" + exe + L"\" " + args;
    if (CreateProcess(nullptr, &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWrite); hWrite = nullptr;
        char buf[4096]; DWORD read = 0;
        while (ReadFile(hRead, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
            buf[read] = '\0';
            int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, nullptr, 0);
            if (wlen > 1) {
                std::wstring wbuf((size_t)wlen - 1, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, buf, -1, &wbuf[0], wlen);
                result += wbuf;
            }
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    if (hWrite) CloseHandle(hWrite);
    CloseHandle(hRead);
    return result;
}

// ---------- engine detection ----------
static std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), ::towlower);
    return s;
}
static std::wstring ExtOf(const std::wstring& p) {
    size_t d = p.find_last_of(L"\\/");
    size_t dot = p.find_last_of(L'.');
    if (dot == std::wstring::npos || (d != std::wstring::npos && dot < d)) return L"";
    return ToLower(p.substr(dot));
}

// Scans the whole file for a byte pattern - installers keep their markers deep
// inside the binary, so a header-only peek is not enough.
static bool FileContainsBytes(const std::wstring& path, const char* needle) {
    size_t n = strlen(needle);
    if (n == 0) return false;
    HANDLE f = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    const DWORD CHUNK = 65536;
    std::vector<char> buf(CHUNK + n);
    DWORD read = 0;
    size_t carry = 0;
    bool found = false;
    while (!found && ReadFile(f, buf.data() + carry, CHUNK, &read, nullptr) && read > 0) {
        size_t total = carry + read;
        const char* p = buf.data();
        const char* end = p + total;
        found = (std::search(p, end, needle, needle + n) != end);
        if (!found && total >= n) {
            carry = n - 1;
            memmove(buf.data(), end - carry, carry);
        } else {
            carry = 0;
        }
    }
    CloseHandle(f);
    return found;
}

std::wstring DetectEngine(const std::wstring& filePath) {
    std::wstring ext = ExtOf(filePath);
    if (ext == L".msi" || ext == L".msp") return L"LessMSI";
    if (ext == L".cab" || ext == L".hdr") {
        // InstallShield cabinets start with "ISc("; any other CAB is plain CAB
        BYTE magic[4] = {};
        DWORD got = 0;
        HANDLE f = CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, 0, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            ReadFile(f, magic, 4, &got, nullptr);
            CloseHandle(f);
        }
        bool installShield = (got == 4 && magic[0] == 'I' && magic[1] == 'S' &&
            magic[2] == 'c' && magic[3] == '(');
        if (installShield && FileExists(GetUnshieldPath())) return L"Unshield";
        return L"7-Zip";
    }
    if (ext == L".pea") return L"PeaZip";
    if (ext == L".rpa") return L"UnRPA";
    if (ext == L".exe") {
        if (FileExists(GetInnounpPath()) && FileContainsBytes(filePath, "Inno Setup")) return L"InnoUnp";
        if (FileExists(GetWisePath()) &&
            (FileContainsBytes(filePath, "WiseMain") || FileContainsBytes(filePath, "WISE Installation")))
            return L"Wise";
        return L"7-Zip";
    }
    HANDLE h = CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        BYTE b[4] = {}; DWORD r = 0;
        ReadFile(h, b, 4, &r, nullptr);
        CloseHandle(h);
        if (r == 4 && b[0] == 0x50 && b[1] == 0x4B) return L"7-Zip (ZIP)";
    }
    return L"7-Zip";
}

static const wchar_t* kArchiveExts[] = {
    L".7z",L".zip",L".zipx",L".rar",L".tar",L".gz",L".tgz",L".bz2",L".xz",L".txz",
    L".zst",L".cab",L".wim",L".iso",L".jar",L".apk",L".docx",L".xlsx",L".pptx",
    L".odt",L".epub",L".cbz",L".pea",L".msi",L".msp",L".exe", nullptr
};
bool IsArchiveLike(const std::wstring& path) {
    std::wstring e = ExtOf(path);
    if (e.empty()) return true; // unknown -> try as archive
    for (int i = 0; kArchiveExts[i]; i++) if (e == kArchiveExts[i]) return true;
    return false;
}

// Parses `7z l -slt`. The first block describes the archive itself (it carries
// a "Type =" line); it is kept so callers can report encryption on encrypted
// headers, but it is never shown as a row.
std::vector<ArchiveEntry> ListArchiveEntries(const std::wstring& archive, const std::wstring& password) {
    std::vector<ArchiveEntry> entries;
    std::wstring args = L"l -slt \"" + archive + L"\"";
    if (!password.empty()) args += L" -p\"" + password + L"\"";
    std::wstring out = RunProcessCapture(Get7zPath(), args);

    ArchiveEntry cur;
    bool have = false;
    auto flush = [&]() {
        if (have && !cur.path.empty()) entries.push_back(cur);
        cur = ArchiveEntry();
        have = false;
    };
    size_t pos = 0;
    while (pos <= out.size()) {
        size_t nl = out.find(L'\n', pos);
        std::wstring line = out.substr(pos, (nl == std::wstring::npos ? out.size() : nl) - pos);
        pos = (nl == std::wstring::npos) ? out.size() + 1 : nl + 1;
        while (!line.empty() && (line.back() == L'\r' || line.back() == L' ')) line.pop_back();
        if (line.empty()) { flush(); continue; }
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos || eq == 0) continue;
        std::wstring key = line.substr(0, eq);
        std::wstring val = line.substr(eq + 1);
        if (key == L"Path") {
            flush();
            have = true;
            cur.path = val;
            size_t slash = val.find_last_of(L"\\/");
            cur.name = (slash == std::wstring::npos) ? val : val.substr(slash + 1);
        } else if (key == L"Folder") {
            cur.isDir = (val == L"+");
        } else if (key == L"Size") {
            cur.size = _wcstoui64(val.c_str(), nullptr, 10);
        } else if (key == L"Packed Size") {
            cur.packed = _wcstoui64(val.c_str(), nullptr, 10);
        } else if (key == L"Modified") {
            cur.modified = val;
        } else if (key == L"Attributes") {
            cur.attrs = val;
            if (!cur.isDir && val.find(L'D') != std::wstring::npos) cur.isDir = true;
        } else if (key == L"Encrypted") {
            cur.encrypted = (val == L"+");
        } else if (key == L"Type") {
            cur.isArchiveSelf = true;
        }
    }
    flush();
    return entries;
}

unsigned long long TotalPackedSize(const std::vector<ArchiveEntry>& entries) {
    unsigned long long total = 0;
    for (const auto& e : entries) if (!e.isDir) total += e.size;
    return total;
}

bool EntriesEncrypted(const std::vector<ArchiveEntry>& entries) {
    for (const auto& e : entries) if (e.encrypted) return true;
    return false;
}

std::vector<EngineInfo> GetEngineStatus() {
    std::vector<EngineInfo> v;
    auto add = [&](const wchar_t* n, const wchar_t* h, const std::wstring& p) {
        v.push_back({ n, h, p, FileExists(p) });
    };
    add(L"7-Zip core", L"7z, zip, rar, tar, iso + more", Get7zPath());
    add(L"7-Zip library", L"format codecs", GetExeDir() + L"\\7z.dll");
    add(L"Zstd / Brotli codecs", L"zst, br, lz4", GetExeDir() + L"\\Codecs\\zstd.dll");
    add(L"LessMSI", L"msi, msp packages", GetLessMsiPath());
    add(L"InnoUnp", L"Inno Setup installers", GetInnounpPath());
    add(L"Unshield", L"InstallShield cabinets", GetUnshieldPath());
    add(L"Pea", L"pea, arc", GetPeaPath());
    add(L"Wise unpacker", L"Wise installers", GetWisePath());
    add(L"TrID scanner", L"file identification", GetTridPath());
    return v;
}

// ---------- password vault (DPAPI, current user) ----------
// Portable installs keep the vault next to the exe; when that folder is not
// writable (Program Files) it falls back to %APPDATA%\StarExtract.
static std::wstring VaultPathPortable() { return GetExeDir() + L"\\vault.dat"; }
static std::wstring VaultPathRoaming() {
    wchar_t appdata[MAX_PATH];
    if (!SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) return L"";
    std::wstring dir = std::wstring(appdata) + L"\\StarExtract";
    SHCreateDirectoryEx(nullptr, dir.c_str(), nullptr);
    return dir + L"\\vault.dat";
}
static bool DirIsWritable(const std::wstring& dir) {
    wchar_t probe[MAX_PATH];
    swprintf(probe, MAX_PATH, L"%s\\.se-write-test", dir.c_str());
    HANDLE h = CreateFile(probe, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}
static std::wstring VaultPathForWrite() {
    std::wstring portable = VaultPathPortable();
    if (DirIsWritable(GetExeDir())) return portable;
    std::wstring roaming = VaultPathRoaming();
    return roaming.empty() ? portable : roaming;
}
static std::wstring VaultPathForRead() {
    std::wstring portable = VaultPathPortable();
    if (FileExists(portable)) return portable;
    std::wstring roaming = VaultPathRoaming();
    if (!roaming.empty() && FileExists(roaming)) return roaming;
    return portable;
}

void LoadPasswords() {
    g_state.passwords.clear();
    std::wstring vp = VaultPathForRead();
    if (!FileExists(vp)) return;
    HANDLE h = CreateFile(vp.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD sz = GetFileSize(h, nullptr);
    std::vector<BYTE> data;
    if (sz > 0 && sz < 1024 * 1024) {
        data.resize(sz); DWORD r = 0;
        ReadFile(h, data.data(), sz, &r, nullptr);
        data.resize(r);
    }
    CloseHandle(h);
    if (data.empty()) return;
    DATA_BLOB inB = { (DWORD)data.size(), data.data() };
    DATA_BLOB outB = {};
    if (CryptUnprotectData(&inB, nullptr, nullptr, nullptr, nullptr, 0, &outB)) {
        std::wstring all((wchar_t*)outB.pbData, outB.cbData / sizeof(wchar_t));
        size_t s = 0;
        while (s < all.size()) {
            size_t e = all.find(L'\n', s);
            if (e == std::wstring::npos) e = all.size();
            std::wstring p = all.substr(s, e - s);
            if (!p.empty() && p.back() == L'\r') p.pop_back();
            if (!p.empty()) g_state.passwords.push_back(p);
            s = e + 1;
        }
        LocalFree(outB.pbData);
    }
}
void SavePasswords() {
    std::wstring all;
    for (auto& p : g_state.passwords) all += p + L"\n";
    DATA_BLOB inB = { (DWORD)(all.size() * sizeof(wchar_t)), (BYTE*)all.c_str() };
    DATA_BLOB outB = {};
    if (CryptProtectData(&inB, L"StarExtract", nullptr, nullptr, nullptr, 0, &outB)) {
        HANDLE h = CreateFile(VaultPathForWrite().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD w = 0;
            WriteFile(h, outB.pbData, outB.cbData, &w, nullptr);
            CloseHandle(h);
        }
        LocalFree(outB.pbData);
    }
}
void AddPassword(const std::wstring& pwd) {
    if (pwd.empty()) return;
    if (std::find(g_state.passwords.begin(), g_state.passwords.end(), pwd) == g_state.passwords.end()) {
        g_state.passwords.push_back(pwd);
        SavePasswords();
    }
}
void RemovePassword(int i) {
    if (i >= 0 && i < (int)g_state.passwords.size()) {
        g_state.passwords.erase(g_state.passwords.begin() + i);
        SavePasswords();
    }
}
void ClearPasswords() { g_state.passwords.clear(); SavePasswords(); }

// ---------- UI settings (HKCU, current user) ----------
static const wchar_t* kUiKey = L"Software\\StarExtract\\Native";

void LoadUiSettings() {
    HKEY h = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kUiKey, 0, KEY_QUERY_VALUE, &h) != ERROR_SUCCESS) return;
    DWORD v = 0, type = 0, cb = sizeof(v);
    if (RegQueryValueExW(h, L"Theme", nullptr, &type, (LPBYTE)&v, &cb) == ERROR_SUCCESS &&
        type == REG_DWORD && v < (DWORD)Theme::Count)
        g_state.currentTheme = (Theme)v;
    cb = sizeof(v); type = 0;
    if (RegQueryValueExW(h, L"Accent", nullptr, &type, (LPBYTE)&v, &cb) == ERROR_SUCCESS && type == REG_DWORD)
        g_customAccent = (COLORREF)v;
    cb = sizeof(v); type = 0;
    if (RegQueryValueExW(h, L"Zoom", nullptr, &type, (LPBYTE)&v, &cb) == ERROR_SUCCESS &&
        type == REG_DWORD && v >= 50 && v <= 200)
        g_state.ui.zoom = v / 100.0;
    cb = sizeof(v); type = 0;
    if (RegQueryValueExW(h, L"Tips", nullptr, &type, (LPBYTE)&v, &cb) == ERROR_SUCCESS && type == REG_DWORD)
        g_state.showTips = (v != 0);
    cb = sizeof(v); type = 0;
    if (RegQueryValueExW(h, L"DblClick", nullptr, &type, (LPBYTE)&v, &cb) == ERROR_SUCCESS && type == REG_DWORD)
        g_state.doubleExtract = (v != 0);
    RegCloseKey(h);
}

void SaveUiSettings() {
    HKEY h = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kUiKey, 0, nullptr, 0, KEY_WRITE, nullptr, &h, nullptr) != ERROR_SUCCESS)
        return;
    DWORD v = 0;
    v = (DWORD)g_state.currentTheme;
    RegSetValueExW(h, L"Theme", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    v = (DWORD)g_customAccent;
    RegSetValueExW(h, L"Accent", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    v = (DWORD)(g_state.ui.zoom * 100.0 + 0.5);
    RegSetValueExW(h, L"Zoom", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    v = g_state.showTips ? 1 : 0;
    RegSetValueExW(h, L"Tips", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    v = g_state.doubleExtract ? 1 : 0;
    RegSetValueExW(h, L"DblClick", 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    RegCloseKey(h);
}

// Try saved passwords with `7z t -p"..."`; returns true + working password.
bool TryVaultPasswords(const std::wstring& archive, std::wstring& working) {
    for (auto& pwd : g_state.passwords) {
        std::wstring cmd = L"/c \"\"" + Get7zPath() + L"\" t -p\"" + pwd + L"\" \"" + archive + L"\" >nul 2>nul\"";
        // Use cmd to capture exit code quickly
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        std::wstring cl = L"cmd.exe " + cmd;
        DWORD flags = CREATE_NO_WINDOW;
        if (CreateProcess(nullptr, &cl[0], nullptr, nullptr, FALSE, flags, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 8000);
            DWORD code = 1;
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            if (code == 0) { working = pwd; return true; }
        }
    }
    return false;
}
bool TryVaultPasswords(const std::wstring& archive) { std::wstring w; return TryVaultPasswords(archive, w); }

// ---------- context menu ----------
void RegisterContextMenu() {
    wchar_t exe[MAX_PATH];
    GetModuleFileName(nullptr, exe, MAX_PATH);
    struct Verb { const wchar_t* key; const wchar_t* label; const wchar_t* args; bool multi; };
    Verb verbs[] = {
        { L"StarExtract_Open", L"Star Extract: Open", L"\"%1\"", false },
        { L"StarExtract_ExtractHere", L"Star Extract: Extract Here", L"/extracthere \"%1\"", false },
        { L"StarExtract_ExtractTo", L"Star Extract: Extract to Folder", L"/extract \"%1\"", false },
        { L"StarExtract_Add7z", L"Star Extract: separate .7z (one per file)", L"/add7z \"%1\"", false },
        { L"StarExtract_AddZip", L"Star Extract: separate .zip (one per file)", L"/addzip \"%1\"", false },
        { L"StarExtract_Combine7z", L"Star Extract: combine into one .7z", L"/combine7z \"%1\"", true },
        { L"StarExtract_CombineZip", L"Star Extract: combine into one .zip", L"/combinezip \"%1\"", true },
        { L"StarExtract_Test", L"Star Extract: Test archive", L"/test \"%1\"", false },
        { L"StarExtract_Hash", L"Star Extract: Hash (SHA-256)", L"/hash \"%1\"", false },
        { L"StarExtract_MSI", L"Star Extract: Extract MSI (LessMSI)", L"/extract \"%1\"", false },
    };
    const wchar_t* targets[] = { L"*", L"Directory", L"Folder", L"AllFilesystemObjects" };
    for (auto t : targets) {
        for (auto& v : verbs) {
            std::wstring base = std::wstring(L"Software\\Classes\\") + t + L"\\shell\\" + v.key;
            HKEY h = nullptr;
            if (RegCreateKeyEx(HKEY_CURRENT_USER, base.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(h, L"MUIVerb", 0, REG_SZ, (const BYTE*)v.label, (DWORD)((wcslen(v.label) + 1) * sizeof(wchar_t)));
                std::wstring icon = std::wstring(L"\"") + exe + L"\",0";
                RegSetValueExW(h, L"Icon", 0, REG_SZ, (const BYTE*)icon.c_str(), (DWORD)((icon.size() + 1) * sizeof(wchar_t)));
                if (v.multi) {
                    const wchar_t* player = L"Player";
                    RegSetValueExW(h, L"MultiSelectModel", 0, REG_SZ, (const BYTE*)player, (DWORD)((wcslen(player) + 1) * sizeof(wchar_t)));
                }
                HKEY hc = nullptr;
                if (RegCreateKeyEx(h, L"command", 0, nullptr, 0, KEY_WRITE, nullptr, &hc, nullptr) == ERROR_SUCCESS) {
                    std::wstring cmd = std::wstring(L"\"") + exe + L"\" " + v.args;
                    RegSetValueExW(hc, nullptr, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
                    RegCloseKey(hc);
                }
                RegCloseKey(h);
            }
        }
    }
    // Dedicated MSI verbs so packages always route to LessMSI
    for (auto assoc : { L"SystemFileAssociations\\.msi", L"SystemFileAssociations\\.msp" }) {
        std::wstring base = std::wstring(L"Software\\Classes\\") + assoc + L"\\shell\\StarExtract_LessMsiExtract";
        HKEY h = nullptr;
        if (RegCreateKeyEx(HKEY_CURRENT_USER, base.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &h, nullptr) == ERROR_SUCCESS) {
            const wchar_t* label = L"Star Extract: Extract MSI with LessMSI";
            RegSetValueExW(h, L"MUIVerb", 0, REG_SZ, (const BYTE*)label, (DWORD)((wcslen(label) + 1) * sizeof(wchar_t)));
            std::wstring icon = std::wstring(L"\"") + exe + L"\",0";
            RegSetValueExW(h, L"Icon", 0, REG_SZ, (const BYTE*)icon.c_str(), (DWORD)((icon.size() + 1) * sizeof(wchar_t)));
            HKEY hc = nullptr;
            if (RegCreateKeyEx(h, L"command", 0, nullptr, 0, KEY_WRITE, nullptr, &hc, nullptr) == ERROR_SUCCESS) {
                std::wstring cmd = std::wstring(L"\"") + exe + L"\" /extract \"%1\"";
                RegSetValueExW(hc, nullptr, 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
                RegCloseKey(hc);
            }
            RegCloseKey(h);
        }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
void UnregisterContextMenu() {
    const wchar_t* keys[] = {
        L"StarExtract_Open", L"StarExtract_ExtractHere", L"StarExtract_ExtractTo",
        L"StarExtract_Add7z", L"StarExtract_AddZip", L"StarExtract_Combine7z",
        L"StarExtract_CombineZip", L"StarExtract_Test", L"StarExtract_Hash", L"StarExtract_MSI",
    };
    const wchar_t* targets[] = { L"*", L"Directory", L"Folder", L"AllFilesystemObjects" };
    for (auto t : targets)
        for (auto k : keys)
            SHDeleteKey(HKEY_CURRENT_USER, (std::wstring(L"Software\\Classes\\") + t + L"\\shell\\" + k).c_str());
    SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\SystemFileAssociations\\.msi\\shell\\StarExtract_LessMsiExtract");
    SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\SystemFileAssociations\\.msp\\shell\\StarExtract_LessMsiExtract");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

// ---------- Explorer double-click association (quick extract / open in app) ----------
static const wchar_t* kAssocExts[] = {
    L".7z",L".zip",L".zipx",L".rar",L".tar",L".gz",L".tgz",L".bz2",L".xz",L".txz",
    L".zst",L".cab",L".wim",L".iso",L".jar",L".apk",L".docx",L".xlsx",L".pptx",
    L".odt",L".epub",L".cbz",L".pea", nullptr
};

static void SetRegStr(HKEY h, const wchar_t* name, const std::wstring& val) {
    RegSetValueExW(h, name, 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));
}

bool IsShellAssociationRegistered() {
    HKEY h = nullptr;
    bool ok = (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\StarExtract.Archive", 0, KEY_READ, &h) == ERROR_SUCCESS);
    if (ok) RegCloseKey(h);
    return ok;
}

bool RegisterShellAssociation(bool quickExtract) {
    wchar_t exe[MAX_PATH];
    GetModuleFileName(nullptr, exe, MAX_PATH);
    const wchar_t* progId = L"StarExtract.Archive";
    HKEY h = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, (std::wstring(L"Software\\Classes\\") + progId).c_str(),
        0, nullptr, 0, KEY_WRITE, nullptr, &h, nullptr) != ERROR_SUCCESS) return false;
    SetRegStr(h, nullptr, L"Archive File (Star Extract)");
    {
        HKEY hi = nullptr;
        if (RegCreateKeyExW(h, L"DefaultIcon", 0, nullptr, 0, KEY_WRITE, nullptr, &hi, nullptr) == ERROR_SUCCESS) {
            SetRegStr(hi, nullptr, std::wstring(L"\"") + exe + L"\",0");
            RegCloseKey(hi);
        }
    }
    HKEY hshell = nullptr;
    bool ok = (RegCreateKeyExW(h, L"shell", 0, nullptr, 0, KEY_WRITE, nullptr, &hshell, nullptr) == ERROR_SUCCESS);
    if (ok) {
        // default verb decides what a double-click does
        SetRegStr(hshell, nullptr, quickExtract ? L"quickextract" : L"open");
        HKEY hq = nullptr;
        if (RegCreateKeyExW(hshell, L"quickextract", 0, nullptr, 0, KEY_WRITE, nullptr, &hq, nullptr) == ERROR_SUCCESS) {
            SetRegStr(hq, nullptr, L"Quick Extract Here");
            HKEY hc = nullptr;
            if (RegCreateKeyExW(hq, L"command", 0, nullptr, 0, KEY_WRITE, nullptr, &hc, nullptr) == ERROR_SUCCESS) {
                SetRegStr(hc, nullptr, std::wstring(L"\"") + exe + L"\" /extract \"%1\"");
                RegCloseKey(hc);
            }
            RegCloseKey(hq);
        }
        HKEY ho = nullptr;
        if (RegCreateKeyExW(hshell, L"open", 0, nullptr, 0, KEY_WRITE, nullptr, &ho, nullptr) == ERROR_SUCCESS) {
            SetRegStr(ho, nullptr, L"Open in Star Extract");
            HKEY hc = nullptr;
            if (RegCreateKeyExW(ho, L"command", 0, nullptr, 0, KEY_WRITE, nullptr, &hc, nullptr) == ERROR_SUCCESS) {
                SetRegStr(hc, nullptr, std::wstring(L"\"") + exe + L"\" \"%1\"");
                RegCloseKey(hc);
            }
            RegCloseKey(ho);
        }
        RegCloseKey(hshell);
    }
    RegCloseKey(h);
    if (!ok) return false;
    for (int i = 0; kAssocExts[i]; i++) {
        HKEY he = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, (std::wstring(L"Software\\Classes\\") + kAssocExts[i]).c_str(),
            0, nullptr, 0, KEY_WRITE, nullptr, &he, nullptr) == ERROR_SUCCESS) {
            SetRegStr(he, nullptr, progId);
            RegCloseKey(he);
        }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

void UnregisterShellAssociation() {
    for (int i = 0; kAssocExts[i]; i++) {
        HKEY he = nullptr;
        std::wstring base = std::wstring(L"Software\\Classes\\") + kAssocExts[i];
        if (RegOpenKeyExW(HKEY_CURRENT_USER, base.c_str(), 0, KEY_SET_VALUE, &he) == ERROR_SUCCESS) {
            wchar_t cur[128] = {}; DWORD cb = sizeof(cur);
            if (RegQueryValueExW(he, nullptr, nullptr, nullptr, (LPBYTE)cur, &cb) == ERROR_SUCCESS &&
                lstrcmpiW(cur, L"StarExtract.Archive") == 0)
                RegDeleteValueW(he, nullptr);
            RegCloseKey(he);
        }
    }
    SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Classes\\StarExtract.Archive");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

// ---------- file identification (magic bytes) ----------
std::wstring IdentifyFile(const std::wstring& path) {
    HANDLE h = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"Cannot open file.";
    BYTE b[16] = {}; DWORD r = 0;
    ReadFile(h, b, 16, &r, nullptr);
    CloseHandle(h);
    if (r < 4) return L"Too small to identify.";
    std::wstring magic = L"First bytes: ";
    wchar_t tmp[8];
    for (DWORD i = 0; i < (r < 8 ? r : 8); i++) { swprintf(tmp, 8, L"%02X ", b[i]); magic += tmp; }
    magic += L"\n";
    if (b[0] == 0x50 && b[1] == 0x4B) return magic + L"ZIP container (zip, docx, apk, jar, ...).";
    if (b[0] == 0x37 && b[1] == 0x7A && b[2] == 0xBC) return magic + L"7-Zip archive.";
    if (b[0] == 0x52 && b[1] == 0x61 && b[2] == 0x72) return magic + L"RAR archive.";
    if (b[0] == 0x1F && b[1] == 0x8B) return magic + L"GZIP archive.";
    if (b[0] == 0x42 && b[1] == 0x5A && b[2] == 0x68) return magic + L"BZip2 archive.";
    if (b[0] == 0xFD && b[1] == 0x37 && b[2] == 0x7A) return magic + L"XZ archive.";
    if (b[0] == 0x28 && b[1] == 0xB5 && b[2] == 0x2F) return magic + L"Zstandard (.zst) archive.";
    if (b[0] == 0x4D && b[1] == 0x5A) return magic + L"Windows executable (MZ). May be an installer.";
    if (b[0] == 0xD0 && b[1] == 0xCF) return magic + L"OLE compound file (MSI installer, old Office doc).";
    if (b[0] == 0x4D && b[1] == 0x53 && b[2] == 0x43 && b[3] == 0x46) return magic + L"MS cabinet (.cab).";
    if (r >= 6 && b[0] == 0x43 && b[1] == 0x44 && b[2] == 0x30 && b[3] == 0x30 && b[4] == 0x31) return magic + L"ISO disc image.";
    // Prefer TrID for a second opinion when bundled
    if (FileExists(GetTridPath())) {
        std::wstring out = RunProcessCapture(GetTridPath(), L"\"" + path + L"\"");
        size_t p = out.find(L"\n");
        if (p != std::wstring::npos) out = out.substr(0, 800);
        return magic + L"TrID says:\n" + out;
    }
    return magic + L"No exact match. Try opening it as an archive.";
}

// ---------- hashing via 7z ----------
std::wstring HashFile(const std::wstring& path, const std::wstring& algo) {
    return RunProcessCapture(Get7zPath(), L"h -scrc" + algo + L" \"" + path + L"\"");
}

// ---------- browser navigation helpers ----------
struct TreeNode {
    std::wstring name;
    std::wstring fullPath;
    bool isExpanded;
    std::vector<TreeNode> children;
};

static std::vector<TreeNode> g_browserTree;

void BuildBrowserTree() {
    g_browserTree.clear();
    TreeNode root;
    root.name = g_state.currentArchive;
    size_t s = root.name.find_last_of(L"\\/");
    if (s != std::wstring::npos) root.name = root.name.substr(s + 1);
    root.fullPath = L"";
    root.isExpanded = true;

    std::map<std::wstring, TreeNode*, std::less<>> dirMap;
    dirMap[L""] = &root;

    for (const auto& entry : g_state.archiveEntries) {
        if (entry.isArchiveSelf) continue;
        std::wstring p = entry.path;
        for (auto& ch : p) if (ch == L'/') ch = L'\\';
        if (p.empty()) continue;

        std::wstring acc;
        std::wstring parentAcc;
        for (size_t i = 0; i < p.size(); ) {
            size_t bs = p.find(L'\\', i);
            if (bs == std::wstring::npos) bs = p.size();
            std::wstring part = p.substr(i, bs - i);
            parentAcc = acc;
            acc = acc.empty() ? part : acc + L"\\" + part;
            bool isLast = (bs == p.size());

            if (dirMap.find(acc) == dirMap.end()) {
                TreeNode node;
                node.name = part;
                node.fullPath = acc;
                node.isExpanded = g_state.browserTreeExpanded;
                dirMap[parentAcc]->children.emplace_back(std::move(node));
                dirMap[acc] = &dirMap[parentAcc]->children.back();
            }
            i = bs + 1;
        }
    }

    // Sort children: folders first, then alphabetically
    std::function<void(TreeNode&)> sortRec = [&](TreeNode& node) {
        std::sort(node.children.begin(), node.children.end(),
            [](const TreeNode& a, const TreeNode& b) {
                if (a.children.empty() != b.children.empty())
                    return !a.children.empty(); // folders first
                return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
            });
        for (auto& c : node.children) sortRec(c);
    };
    sortRec(root);
    g_browserTree.push_back(std::move(root));
}

void RefreshBrowserTree() {
    HWND h = g_state.hwndTreeList;
    if (!h) return;
    BuildBrowserTree();
    ListView_DeleteAllItems(h);

    auto addNode = [&](auto&& self, const TreeNode& node, int indent, int parentIdx) -> void {
        std::wstring label(indent * 3, L' ');
        label += node.name.empty() ? L"(archive)" : node.name;
        LVITEM it = { 0 };
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = ListView_GetNextItem(h, parentIdx, LVNI_ALL);   // insert after parent
        if (parentIdx == -1 || it.iItem == -1) it.iItem = ListView_GetItemCount(h);
        it.pszText = (LPWSTR)label.c_str();
        it.lParam = (LPARAM)new std::wstring(node.fullPath);
        ListView_InsertItem(h, &it);
        for (const auto& child : node.children) {
            self(self, child, indent + 1, it.iItem);
        }
    };

    // drop the paths owned by the previous build before re-populating
    for (int i = 0; i < ListView_GetItemCount(h); i++) {
        LVITEM it = { 0 };
        it.mask = LVIF_PARAM;
        it.iItem = i;
        if (ListView_GetItem(h, &it) && it.lParam) delete (std::wstring*)it.lParam;
    }
    ListView_DeleteAllItems(h);

    for (const auto& root : g_browserTree) {
        addNode(addNode, root, 0, -1);
    }
    ListView_SetColumnWidth(h, 0, LVSCW_AUTOSIZE_USEHEADER);
}

void RefreshBreadcrumbs() {
    HWND h = g_state.hwndBreadcrumbBar;
    if (!h) return;
    // Breadcrumbs as a static text with clickable segments
    std::wstring text = g_state.currentArchive;
    size_t s = text.find_last_of(L"\\/");
    if (s != std::wstring::npos) text = text.substr(s + 1);
    if (!g_state.currentFolder.empty()) {
        text += L"  >  " + g_state.currentFolder;
    }
    SetWindowTextString(h, text);
}

void PushHistory(const std::wstring& folder) {
    if (g_state.historyPos + 1 < g_state.folderHistory.size()) {
        g_state.folderHistory.resize(g_state.historyPos + 1);
    }
    g_state.folderHistory.push_back(folder);
    g_state.historyPos = g_state.folderHistory.size() - 1;
    EnableWindow(g_state.hwndBtnBack, g_state.historyPos > 0);
    EnableWindow(g_state.hwndBtnFwd, g_state.historyPos + 1 < g_state.folderHistory.size());
}

void NavigateBrowser(const std::wstring& folder, bool pushHistory) {
    if (pushHistory) {
        PushHistory(g_state.currentFolder);
    }
    if (!g_state.filterText.empty()) {
        SetWindowTextString(g_state.hwndFilter, L"");
        g_state.filterText.clear();
    }
    g_state.currentFolder = folder;
    RefreshBrowserList();
    RefreshBrowserTree();
    RefreshBreadcrumbs();
}

// Rebuilds the file list for the current folder. With a filter typed in the
// search box the whole archive is searched instead and matching rows show
// their path inside the archive, the way the classic browser behaved.
void RefreshBrowserList() {
    HWND h = g_state.hwndFileList;
    if (!h) return;
    ListView_DeleteAllItems(h);
    ListView_SetBkColor(h, g_state.colors.bgSurface);
    ListView_SetTextBkColor(h, g_state.colors.bgSurface);
    ListView_SetTextColor(h, g_state.colors.textPrimary);

    std::wstring filter = g_state.filterText;
    while (!filter.empty() && filter.front() == L' ') filter.erase(filter.begin());
    while (!filter.empty() && filter.back() == L' ') filter.pop_back();

    int idx = 0;
    if (g_state.currentArchive.empty()) {
        LVITEM it = { 0 }; it.mask = LVIF_TEXT; it.iItem = 0;
        it.pszText = (LPWSTR)L"No archive open yet - click Open above, or drag & drop a file anywhere.";
        ListView_InsertItem(h, &it);
        UpdateBrowserStatus();
        return;
    }

    struct Row {
        std::wstring display, type, size, packed, modified;
        std::wstring path;      // real path inside the archive (may be a folder prefix)
        bool dir;
    };
    std::vector<Row> rows;

    auto addRow = [&](const std::wstring& display, const std::wstring& type,
                      const std::wstring& size, const std::wstring& packed,
                      const std::wstring& modified, const std::wstring& path, bool dir) {
        LVITEM it = { 0 };
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = idx;
        it.pszText = (LPWSTR)display.c_str();
        it.lParam = (LPARAM)new std::wstring(path);
        if (ListView_InsertItem(h, &it) != -1) {
            ListView_SetItemText(h, idx, 1, (LPWSTR)type.c_str());
            ListView_SetItemText(h, idx, 2, (LPWSTR)size.c_str());
            ListView_SetItemText(h, idx, 3, (LPWSTR)packed.c_str());
            ListView_SetItemText(h, idx, 4, (LPWSTR)modified.c_str());
        } else {
            delete (std::wstring*)it.lParam;
        }
        idx++;
    };

    auto typeOf = [](const std::wstring& name, bool dir) -> std::wstring {
        if (dir) return L"Folder";
        size_t dot = name.find_last_of(L'.');
        if (dot == std::wstring::npos || dot + 1 >= name.size()) return L"File";
        std::wstring ext = name.substr(dot + 1);
        for (auto& c : ext) c = towupper(c);
        return ext + L" File";
    };

    if (!filter.empty()) {
        // recursive search across the whole archive
        std::vector<Row> hits;
        for (const auto& e : g_state.archiveEntries) {
            if (e.isArchiveSelf || e.isDir) continue;
            if (StrStrIW(e.path.c_str(), filter.c_str()) || StrStrIW(e.name.c_str(), filter.c_str())) {
                hits.push_back({ e.path, typeOf(e.name, false), FormatSize(e.size),
                                 FormatSize(e.packed), e.modified, e.path, false });
            }
        }
        std::sort(hits.begin(), hits.end(), [](const Row& a, const Row& b) {
            return _wcsicmp(a.display.c_str(), b.display.c_str()) < 0;
        });
        for (const auto& r : hits)
            addRow(r.display, r.type, r.size, r.packed, r.modified, r.path, r.dir);

        std::wstring label = g_state.currentArchive;
        size_t s2 = label.find_last_of(L"\\/");
        if (s2 != std::wstring::npos) label = label.substr(s2 + 1);
        label += L"   search: " + filter + L"   (" + std::to_wstring(hits.size()) + L" match" +
            (hits.size() == 1 ? L"" : L"es") + L")";
        SetWindowTextString(g_state.hwndPathLabel ? g_state.hwndPathLabel : g_state.hwndEngineBadge, label);
        UpdateBrowserStatus();
        return;
    }

    if (!g_state.currentFolder.empty()) {
        addRow(L"..", L"Folder", L"", L"", L"", L"..", true);
    }

    std::wstring prefix = g_state.currentFolder.empty() ? L"" : g_state.currentFolder + L"\\";
    std::vector<std::wstring> seenDirs;
    for (const auto& e : g_state.archiveEntries) {
        if (e.isArchiveSelf) continue;
        std::wstring p = e.path;
        for (auto& ch : p) if (ch == L'/') ch = L'\\';
        if (prefix.size() > p.size() || _wcsnicmp(p.c_str(), prefix.c_str(), prefix.size()) != 0) continue;
        std::wstring rest = p.substr(prefix.size());
        if (rest.empty()) continue;
        size_t bs = rest.find(L'\\');
        if (bs == std::wstring::npos) {
            rows.push_back({ e.name, typeOf(e.name, false), FormatSize(e.size),
                             FormatSize(e.packed), e.modified, p, false });
        } else if (!e.isDir) {
            std::wstring dn = rest.substr(0, bs);
            bool dup = false;
            for (auto& s : seenDirs) if (!_wcsicmp(s.c_str(), dn.c_str())) { dup = true; break; }
            if (!dup) {
                seenDirs.push_back(dn);
                std::wstring full = prefix + dn;
                rows.push_back({ dn, L"Folder", L"", L"", L"", full, true });
            }
        }
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.dir != b.dir) return a.dir > b.dir;
        return _wcsicmp(a.display.c_str(), b.display.c_str()) < 0;
    });
    for (const auto& r : rows)
        addRow(r.display, r.type, r.size, r.packed, r.modified, r.path, r.dir);

    std::wstring label = g_state.currentArchive;
    size_t s2 = label.find_last_of(L"\\/");
    if (s2 != std::wstring::npos) label = label.substr(s2 + 1);
    if (!g_state.currentFolder.empty()) label += L"  >  " + g_state.currentFolder;
    else label += L"  (top level)";
    SetWindowTextString(g_state.hwndPathLabel ? g_state.hwndPathLabel : g_state.hwndEngineBadge, label);
    UpdateBrowserStatus();
}

// ---------- plugin grid ----------
// Rows come from the component catalog in se_components.cpp; row index == the
// component index, which is what every action here operates on.
void RefreshPluginGrid() {
    HWND h = g_state.hwndPluginGrid;
    if (!h) return;
    RefreshComponentStatus();
    ListView_DeleteAllItems(h);
    ListView_SetBkColor(h, g_state.colors.bgSurface);
    ListView_SetTextBkColor(h, g_state.colors.bgSurface);
    ListView_SetTextColor(h, g_state.colors.textPrimary);

    if (ListView_GetHeader(h) && Header_GetItemCount(ListView_GetHeader(h)) == 0) {
        LVCOLUMN c = { 0 };
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        c.pszText = (LPWSTR)L"Component"; c.cx = 200; ListView_InsertColumn(h, 0, &c);
        c.pszText = (LPWSTR)L"Status"; c.cx = 100; ListView_InsertColumn(h, 1, &c);
        c.pszText = (LPWSTR)L"Latest"; c.cx = 70; ListView_InsertColumn(h, 2, &c);
        c.pszText = (LPWSTR)L"Verified"; c.cx = 80; ListView_InsertColumn(h, 3, &c);
        c.pszText = (LPWSTR)L"Source URL"; c.cx = 360; ListView_InsertColumn(h, 4, &c);
        c.pszText = (LPWSTR)L"Verify file"; c.cx = 190; ListView_InsertColumn(h, 5, &c);
    }

    int n = ComponentCount();
    for (int i = 0; i < n; i++) {
        LVITEM it = { 0 };
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = i;
        std::wstring name = ComponentField(i, COMP_FIELD_NAME);
        if (ComponentField(i, COMP_FIELD_CUSTOM) == L"1") name += L"  *";
        it.pszText = (LPWSTR)name.c_str();
        it.lParam = (LPARAM)i;
        if (ListView_InsertItem(h, &it) == -1) continue;
        auto set = [&](int col, int field) {
            std::wstring v = ComponentField(i, field);
            ListView_SetItemText(h, i, col, (LPWSTR)v.c_str());
        };
        set(1, COMP_FIELD_STATUS);
        set(2, COMP_FIELD_LATEST);
        ListView_SetItemText(h, i, 3,
            (LPWSTR)(ComponentField(i, COMP_FIELD_SHA256).empty() ? L"no" : L"yes"));
        set(4, COMP_FIELD_URL);
        set(5, COMP_FIELD_VERIFY);
    }
}
