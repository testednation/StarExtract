// StarExtract Native - component / plugin manager
//
// Downloads helper tools (7-Zip core, zstd codecs, UniExtract2 plugins) from
// their upstream URLs and unpacks them next to the app. Pure Win32: WinINet for
// HTTP, CryptoAPI for SHA-256, 7z.exe for unpacking - no .NET, no extra DLLs.
//
// Trust model (the reference app had none of this):
//   * a component may pin an expected SHA-256. If it does, a mismatch aborts
//     the install and the download is deleted.
//   * if it does not, the UI asks for confirmation first, showing the URL and
//     the destination, and the hash of what was actually fetched is pinned
//     afterwards so the next update is checked against it.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601        // Windows 7 - matches the installer's MinVersion
#endif

#include "se.h"
#include <wininet.h>
#include <atomic>
#include <thread>

// The Windows 11 SDK header shipped with this toolchain declares InternetReadFile
// without its trailing lpdwIndex argument, and the matching import library only
// exports the 4-argument stdcall symbol. The function WinINet really exports
// takes five, so resolve it by name instead of through the import library -
// otherwise the x86 build either fails to link or corrupts the stack.
typedef BOOL(WINAPI* PFN_InternetReadFile)(HINTERNET, LPVOID, DWORD, LPDWORD, LPDWORD);

static PFN_InternetReadFile InternetReadFileReal() {
    static HMODULE mod = LoadLibraryW(L"wininet.dll");
    return mod ? (PFN_InternetReadFile)GetProcAddress(mod, "InternetReadFile") : nullptr;
}

#pragma comment(lib, "wininet.lib")

// ---------------------------------------------------------------- catalog ---

enum class InstallMode {
    Core7z,       // self-extracting 7z installer: copy 7z.dll + 7z.exe out
    IntoDir,      // unpack straight into the destination folder
    VerifyParent, // unpack to a stage, copy the folder holding the verify file into the bin dir
    CopyContents, // unpack to a stage, copy the verify file's folder contents into
                  // the directory named by the verify path (bin/<tool>/...)
};

struct Component {
    std::wstring id, name, category, description, url, sha256;
    std::wstring verifyFile;   // relative to the app dir, backslashes
    std::wstring destFolder;   // relative to the app dir, "" = app dir
    std::wstring formats, installedVersion, latestVersion, status;
    InstallMode mode = InstallMode::VerifyParent;
    bool custom = false;
    bool urlEdited = false;
};

static std::vector<Component> g_components;
static std::vector<Component> g_defaults;

static const wchar_t* kUniExtractUrl =
    L"https://github.com/gvp9000/UniExtract2/releases/download/v3.0.4/UniExtract2.zip";

static void BuildDefaults() {
    g_defaults.clear();
    auto add = [&](const wchar_t* id, const wchar_t* name, const wchar_t* cat,
                   const wchar_t* desc, const wchar_t* url, const wchar_t* verify,
                   const wchar_t* dest, const wchar_t* formats, const wchar_t* latest,
                   InstallMode mode) {
        Component c;
        c.id = id; c.name = name; c.category = cat; c.description = desc;
        c.url = url; c.verifyFile = verify; c.destFolder = dest;
        c.formats = formats; c.latestVersion = latest;
        c.mode = mode; c.status = L"";
        g_defaults.push_back(c);
    };

    // ---- core engine + codecs (arch-specific) --------------------------
    add(L"7z-core-x64", L"7-Zip Core Engine (x64)", L"Core",
        L"The compression engine everything else runs through. 64-bit.",
        L"https://www.7-zip.org/a/7z2408-x64.exe", L"7z.dll", L"",
        L"7z, zip, rar, tar, gz, xz, bz2, iso, cab, wim", L"24.08",
        InstallMode::Core7z);
    add(L"codecs-x64", L"Zstandard / Brotli codecs (x64)", L"Codecs",
        L"Modern codecs for 7-Zip: zstd, brotli, lz4, lz5, lizard. 64-bit.",
        L"https://github.com/mcmilk/7-Zip-zstd/releases/download/v26.02-v1.5.7-R1/Codecs-x64.7z",
        L"Codecs\\zstd.dll", L"Codecs", L"zst, tzst, br, lz4, lz5, flzma2", L"26.02",
        InstallMode::IntoDir);
    add(L"codecs-x86", L"Zstandard / Brotli codecs (x86)", L"Codecs",
        L"Modern codecs for the 32-bit engine: zstd, brotli, lz4, lz5, lizard.",
        L"https://github.com/mcmilk/7-Zip-zstd/releases/download/v26.02-v1.5.7-R1/Codecs-x86.7z",
        L"Codecs\\zstd.dll", L"Codecs", L"zst, tzst, br, lz4, lz5, flzma2", L"26.02",
        InstallMode::IntoDir);

    // ---- UniExtract2 bundle (ships innounp, unshield, unrpa, ewise, ...)
    add(L"uniextract2", L"UniExtract2 plugin bundle", L"Plugins",
        L"All UniExtract2 unpackers in one download (innounp, unshield, unrpa, wise, ...).",
        kUniExtractUrl, L"bin\\innounp.exe", L"bin",
        L"Inno Setup, InstallShield, RPA, Wise, ...", L"3.0.4",
        InstallMode::VerifyParent);

    // Individual plugins that fall back to the UniExtract2 bundle (no verified
    // standalone binary URL) - keep them so the grid can show install status.
    add(L"innounp", L"Inno Setup unpacker", L"Plugins",
        L"Unpacks Inno Setup installers without running them.",
        kUniExtractUrl, L"bin\\innounp.exe", L"bin", L"Inno Setup .exe", L"3.0.4",
        InstallMode::VerifyParent);
    add(L"unshield", L"InstallShield cabinet unpacker", L"Plugins",
        L"Unpacks InstallShield cabinet (.cab / .hdr) containers.",
        kUniExtractUrl, L"bin\\unshield.exe", L"bin", L"InstallShield .cab, .hdr", L"3.0.4",
        InstallMode::VerifyParent);
    add(L"unrpa", L"UnRPA engine", L"Plugins",
        L"Reads RPG Maker archives.",
        kUniExtractUrl, L"bin\\unrpa.exe", L"bin", L".rpa", L"3.0.4",
        InstallMode::VerifyParent);
    add(L"ewise", L"Wise setup unpacker", L"Plugins",
        L"Unpacks legacy Wise installer packages.",
        kUniExtractUrl, L"bin\\E_WISE_W.EXE", L"bin", L"Wise Setup .exe", L"3.0.4",
        InstallMode::VerifyParent);
    add(L"enigma", L"Enigma VirtualBox unpacker", L"Plugins",
        L"Extracts embedded files from Enigma VirtualBox packages.",
        kUniExtractUrl, L"bin\\EnigmaVBUnpacker.exe", L"bin",
        L"Enigma Virtual Box .exe", L"3.0.4", InstallMode::VerifyParent);

    // ---- standalone plugins with verified upstream download URLs -------
    add(L"lessmsi", L"LessMSI (.msi / .msp)", L"Plugins",
        L"Unpacks Windows Installer packages without installing them.",
        L"https://github.com/activescott/lessmsi/releases/download/v2.12.9/lessmsi-v2.12.9.zip",
        L"bin\\lessmsi\\lessmsi.exe", L"bin", L".msi, .msp", L"2.12.9",
        InstallMode::CopyContents);
    add(L"garbro", L"GARbro (game archives)", L"Plugins",
        L"Unpacks visual-novel and game data archives.",
        L"https://github.com/morkt/GARbro/releases/download/v1.5.44/GARbro-v1.5.44.2904.rar",
        L"bin\\GARbro\\ArcFormats.dll", L"bin", L"Game archives, visual novel assets", L"1.5.44",
        InstallMode::CopyContents);
    add(L"pea", L"PeaZip engine", L"Plugins",
        L"Reads PEA and ARC containers.",
        L"https://github.com/peazip/PeaZip/releases/download/11.3.0/peazip_portable-11.3.0.WINDOWS.zip",
        L"bin\\peazip\\pea.exe", L"bin", L".pea, .arc", L"11.3.0",
        InstallMode::CopyContents);
    add(L"exeinfope", L"ExeInfoPE (PE analyzer)", L"Plugins",
        L"Analyze PE files, compilers, packers, and languages.",
        L"https://github.com/ExeinfoASL/ASL/releases/download/V1.0.0/exeinfope.zip",
        L"bin\\ExeinfoPE\\exeinfope.exe", L"bin", L".exe, .dll, PE headers", L"1.0.0",
        InstallMode::CopyContents);
    add(L"trid", L"TrID (file identifier)", L"Plugins",
        L"Identifies file format by binary signature.",
        L"https://mark0.net/download/trid_w32.zip",
        L"bin\\trid.exe", L"bin", L"File identification", L"2.0.17",
        InstallMode::CopyContents);
    add(L"triddefs", L"TrID definitions", L"Plugins",
        L"Definition files for the TrID identifier.",
        L"https://mark0.net/download/triddefs.zip",
        L"bin\\triddefs.trd", L"bin", L"TrID definitions", L"22425 types",
        InstallMode::CopyContents);
    add(L"magika", L"Magika (ML file type)", L"Plugins",
        L"Machine-learning file format identification.",
        L"https://github.com/google/magika/releases/download/cli/v1.1.0/magika-cli-x86_64-pc-windows-msvc.zip",
        L"bin\\magika\\magika.exe", L"bin", L"File identification (ML)", L"1.1.0",
        InstallMode::CopyContents);
}

// ------------------------------------------------------------ persistence ---
// Plain INI: one section per component. Kept next to the app for portable
// installs, with the same %APPDATA% fallback the password vault uses.

static std::wstring ComponentsPath() {
    std::wstring portable = GetExeDir() + L"\\components.ini";
    wchar_t probe[MAX_PATH];
    swprintf(probe, MAX_PATH, L"%s\\.se-write-test", GetExeDir().c_str());
    HANDLE h = CreateFile(probe, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); return portable; }
    wchar_t appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
        std::wstring dir = std::wstring(appdata) + L"\\StarExtract";
        SHCreateDirectoryEx(nullptr, dir.c_str(), nullptr);
        return dir + L"\\components.ini";
    }
    return portable;
}

static void ReadComponentIni() {
    std::wstring path = ComponentsPath();
    wchar_t section[128];
    for (size_t i = 0; i < g_components.size(); i++) {
        _snwprintf(section, 128, L"%s", g_components[i].id.c_str());
        auto get = [&](const wchar_t* key, std::wstring def) -> std::wstring {
            wchar_t buf[2048] = L"";
            GetPrivateProfileStringW(section, key, def.c_str(), buf, 2048, path.c_str());
            return buf;
        };
        Component& c = g_components[i];
        std::wstring url = get(L"URL", c.url);
        if (!url.empty()) c.url = url;
        std::wstring sha = get(L"SHA256", L"");
        if (!sha.empty()) c.sha256 = sha;
        c.custom = get(L"Custom", L"0") == L"1";
        c.urlEdited = get(L"UrlEdited", L"0") == L"1";
    }
}

static void WriteComponentIni() {
    std::wstring path = ComponentsPath();
    for (size_t i = 0; i < g_components.size(); i++) {
        Component& c = g_components[i];
        WritePrivateProfileStringW(c.id.c_str(), L"URL", c.url.c_str(), path.c_str());
        WritePrivateProfileStringW(c.id.c_str(), L"SHA256",
            c.sha256.empty() ? nullptr : c.sha256.c_str(), path.c_str());
        WritePrivateProfileStringW(c.id.c_str(), L"Custom", c.custom ? L"1" : L"0", path.c_str());
        WritePrivateProfileStringW(c.id.c_str(), L"UrlEdited",
            c.urlEdited ? L"1" : L"0", path.c_str());
    }
}

void LoadComponents() {
    BuildDefaults();
    g_components = g_defaults;
    ReadComponentIni();
}

void SaveComponents() { WriteComponentIni(); }

std::vector<Component>& Components() { return g_components; }

int ComponentCount() { return (int)g_components.size(); }

std::wstring ComponentField(int index, int field) {
    if (index < 0 || index >= (int)g_components.size()) return L"";
    const Component& c = g_components[index];
    switch (field) {
    case COMP_FIELD_NAME: return c.name;
    case COMP_FIELD_STATUS: return c.status;
    case COMP_FIELD_INSTALLED: return c.installedVersion;
    case COMP_FIELD_LATEST: return c.latestVersion;
    case COMP_FIELD_URL: return c.url;
    case COMP_FIELD_VERIFY: return c.verifyFile;
    case COMP_FIELD_CATEGORY: return c.category;
    case COMP_FIELD_FORMATS: return c.formats;
    case COMP_FIELD_SHA256: return c.sha256;
    case COMP_FIELD_CUSTOM: return c.custom ? L"1" : L"0";
    default: return L"";
    }
}

void SetComponentField(int index, int field, const std::wstring& value) {
    if (index < 0 || index >= (int)g_components.size()) return;
    Component& c = g_components[index];
    switch (field) {
    case COMP_FIELD_URL: c.url = value; c.urlEdited = true; break;
    case COMP_FIELD_VERIFY: c.verifyFile = value; break;
    default: break;
    }
    WriteComponentIni();
}

void ResetComponentsToDefaults() {
    DeleteFile(ComponentsPath().c_str());
    LoadComponents();
}

// --------------------------------------------------------------- helpers ---

static std::wstring ToLower(std::wstring s) {
    for (auto& c : s) c = towlower(c);
    return s;
}

static std::wstring JoinPath(const std::wstring& a, const std::wstring& b) {
    if (b.empty()) return a;
    if (a.empty()) return b;
    if (a.back() == L'\\') return a + b;
    return a + L"\\" + b;
}

static bool CopyTree(const std::wstring& from, const std::wstring& to) {
    SHCreateDirectoryEx(nullptr, to.c_str(), nullptr);
    WIN32_FIND_DATAW fd;
    std::wstring pattern = JoinPath(from, L"*");
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        std::wstring src = JoinPath(from, fd.cFileName);
        std::wstring dst = JoinPath(to, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!CopyTree(src, dst)) ok = false;
        } else {
            if (!CopyFileW(src.c_str(), dst.c_str(), FALSE)) ok = false;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return ok;
}

static bool FindFileRecursive(const std::wstring& root, const std::wstring& leaf,
    std::wstring& found) {
    WIN32_FIND_DATAW fd;
    std::wstring pattern = JoinPath(root, L"*");
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    bool hit = false;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        std::wstring full = JoinPath(root, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (FindFileRecursive(full, leaf, found)) { hit = true; break; }
        } else if (!_wcsicmp(fd.cFileName, leaf.c_str())) {
            found = full;
            hit = true;
            break;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return hit;
}

static void RemoveTree(const std::wstring& dir) {
    WIN32_FIND_DATAW fd;
    std::wstring pattern = JoinPath(dir, L"*");
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            std::wstring full = JoinPath(dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveTree(full);
            else DeleteFileW(full.c_str());
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir.c_str());
}

std::wstring Sha256OfFile(const std::wstring& path) {
    HCRYPTPROV prov = 0;
    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return L"";
    HCRYPTHASH hash = 0;
    if (!CryptCreateHash(prov, CALG_SHA_256, 0, 0, &hash)) {
        CryptReleaseContext(prov, 0);
        return L"";
    }
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        CryptDestroyHash(hash);
        CryptReleaseContext(prov, 0);
        return L"";
    }
    std::vector<BYTE> buf(65536);
    DWORD r = 0;
    while (ReadFile(f, buf.data(), (DWORD)buf.size(), &r, nullptr) && r > 0)
        CryptHashData(hash, buf.data(), r, 0);
    CloseHandle(f);

    DWORD len = 0;
    std::vector<BYTE> digest;
    if (CryptGetHashParam(hash, HP_HASHVAL, nullptr, &len, 0)) {
        digest.resize(len);
        if (CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &len, 0)) {
            CryptDestroyHash(hash);
            CryptReleaseContext(prov, 0);
            wchar_t hex[65] = L"";
            for (size_t i = 0; i < len && i < 32; i++)
                swprintf(hex + i * 2, 3, L"%02X", digest[i]);
            return hex;
        }
    }
    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);
    return L"";
}

// ------------------------------------------------------------- status ------

void RefreshComponentStatus() {
    std::wstring app = GetExeDir();
    for (auto& c : g_components) {
        bool present = FileExists(JoinPath(app, c.verifyFile));
        c.installedVersion = present ? c.latestVersion : L"-";
        c.status = present ? L"Installed" : L"Not installed";
    }
}

static int ComponentIndexByVerifyName(const std::wstring& id) {
    for (size_t i = 0; i < g_components.size(); i++)
        if (g_components[i].id == id) return (int)i;
    return -1;
}

// ---------------------------------------------------------- the job --------

struct CompJob {
    std::atomic<bool> cancel{ false };
    std::atomic<bool> busy{ false };
    HWND notify = nullptr;
};
static CompJob g_job;

static void Post(HWND h, UINT msg, WPARAM wp, std::wstring text) {
    PostMessage(h, msg, wp, (LPARAM)new std::wstring(text));
}

static void PostResult(HWND h, int code, const std::wstring& msg, const std::wstring& sha) {
    CompResult* r = new CompResult{ code, msg, sha };
    PostMessage(h, WM_APP_COMP_DONE, 0, (LPARAM)r);
}

static bool DownloadTo(const std::wstring& url, const std::wstring& dest, HWND notify,
    unsigned long long& received) {
    received = 0;
    PFN_InternetReadFile readFile = InternetReadFileReal();
    if (!readFile) return false;
    HINTERNET net = InternetOpenW(L"StarExtract/1.6", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!net) return false;
    HINTERNET conn = InternetOpenUrlW(net, url.c_str(), nullptr, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_UI, 0);
    if (!conn) { InternetCloseHandle(net); return false; }

    HANDLE out = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(conn);
        InternetCloseHandle(net);
        return false;
    }

    std::vector<BYTE> buf(65536);
    bool ok = true;
    unsigned long long lastReport = 0;
    for (;;) {
        if (g_job.cancel.load()) { ok = false; break; }
        DWORD avail = 0;
        if (!InternetQueryDataAvailable(conn, &avail, 0, 0)) { ok = false; break; }
        if (avail == 0) break;
        DWORD got = 0, index = 0;
        DWORD want = (avail > (DWORD)buf.size()) ? (DWORD)buf.size() : avail;
        if (!readFile(conn, buf.data(), want, &got, &index) || got == 0) { ok = false; break; }
        DWORD written = 0;
        if (!WriteFile(out, buf.data(), got, &written, nullptr)) { ok = false; break; }
        received += got;
        // no reliable content length across every server, so report bytes moved
        if (received - lastReport >= 256 * 1024) {
            lastReport = received;
            Post(notify, WM_APP_COMP_PROGRESS, (WPARAM)0,
                L"downloaded " + FormatSize(received) + L"...");
        }
    }
    CloseHandle(out);
    InternetCloseHandle(conn);
    InternetCloseHandle(net);
    return ok;
}

static DWORD WINAPI ComponentThread(LPVOID param) {
    Component* c = (Component*)param;
    HWND notify = g_job.notify;
    std::wstring id = c->id, url = c->url, verify = c->verifyFile;
    std::wstring dest = c->destFolder;
    InstallMode mode = c->mode;
    std::wstring pinned = ToLower(c->sha256);
    delete c;

    wchar_t tmpRoot[MAX_PATH];
    GetTempPath(MAX_PATH, tmpRoot);
    std::wstring work = std::wstring(tmpRoot) + L"StarExtract\\dl_" + id;
    RemoveTree(work);
    SHCreateDirectoryEx(nullptr, work.c_str(), nullptr);
    std::wstring tmpFile = work + L"\\download.bin";

    Post(notify, WM_APP_COMP_PROGRESS, 0, L"contacting the server...");
    unsigned long long received = 0;
    if (!DownloadTo(url, tmpFile, notify, received)) {
        RemoveTree(work);
        PostResult(notify, g_job.cancel.load() ? 2 : 1,
            g_job.cancel.load() ? L"Cancelled." : L"Download failed. Check the URL and your connection.", L"");
        g_job.busy.store(false);
        return 0;
    }

    Post(notify, WM_APP_COMP_PROGRESS, 100, L"verifying...");
    std::wstring sha = Sha256OfFile(tmpFile);
    if (sha.empty()) {
        RemoveTree(work);
        PostResult(notify, 1, L"Could not hash the download.", L"");
        g_job.busy.store(false);
        return 0;
    }
    if (!pinned.empty() && pinned != ToLower(sha)) {
        RemoveTree(work);
        PostResult(notify, 1,
            L"SHA-256 mismatch - the download was deleted and nothing was installed.\nExpected: " + pinned +
            L"\nActual:   " + sha, sha);
        g_job.busy.store(false);
        return 0;
    }

    Post(notify, WM_APP_COMP_PROGRESS, 100, L"unpacking...");
    std::wstring stage = work + L"\\stage";
    SHCreateDirectoryEx(nullptr, stage.c_str(), nullptr);
    std::wstring out;
    if (mode == InstallMode::IntoDir) {
        std::wstring app = GetExeDir();
        std::wstring target = dest.empty() ? app : JoinPath(app, dest);
        SHCreateDirectoryEx(nullptr, target.c_str(), nullptr);
        out = RunProcessCapture(Get7zPath(), L"x -y -o\"" + target + L"\" \"" + tmpFile + L"\"");
    } else {
        out = RunProcessCapture(Get7zPath(), L"x -y -o\"" + stage + L"\" \"" + tmpFile + L"\"");
    }
    if (out.find(L"Everything is Ok") == std::wstring::npos &&
        out.find(L"Everything is ok") == std::wstring::npos) {
        // 7z can still succeed without that exact line; fall back to the file check
        std::wstring app = GetExeDir();
        if (!FileExists(JoinPath(app, verify))) {
            RemoveTree(work);
            PostResult(notify, 1, L"Unpacking failed - the expected file did not appear.", sha);
            g_job.busy.store(false);
            return 0;
        }
    }

    std::wstring app = GetExeDir();
    if (mode == InstallMode::Core7z) {
        std::wstring found;
        if (FindFileRecursive(stage, L"7z.dll", found)) {
            std::wstring srcDir = found.substr(0, found.find_last_of(L"\\"));
            for (const wchar_t* f : { L"7z.dll", L"7z.exe" }) {
                std::wstring from = JoinPath(srcDir, f);
                if (FileExists(from)) {
                    if (!CopyFileW(from.c_str(), JoinPath(app, f).c_str(), FALSE))
                        Post(notify, WM_APP_COMP_PROGRESS, 100,
                            std::wstring(L"could not replace ") + f + L" (close other programs using it)");
                }
            }
        }
    } else if (mode == InstallMode::VerifyParent) {
        std::wstring leaf = verify.substr(verify.find_last_of(L"\\") + 1);
        std::wstring found;
        if (FindFileRecursive(stage, leaf, found)) {
            std::wstring srcDir = found.substr(0, found.find_last_of(L"\\"));
            std::wstring target = dest.empty() ? app : JoinPath(app, dest);
            if (!CopyTree(srcDir, target)) {
                Post(notify, WM_APP_COMP_PROGRESS, 100,
                    L"copied, but some files could not be written (read-only install folder?)");
            }
        }
    } else if (mode == InstallMode::CopyContents) {
        // copy the folder that HOLDS the verify file into the directory named
        // by the verify path. Example: staged bin/peazip/pea.exe with verify
        // bin/peazip/pea.exe copies the peazip folder into <app>/bin/peazip
        std::wstring leaf = verify.substr(verify.find_last_of(L"\\") + 1);
        std::wstring found;
        if (FindFileRecursive(stage, leaf, found)) {
            std::wstring srcDir = found.substr(0, found.find_last_of(L"\\"));
            std::wstring parentInApp = verify.substr(0, verify.find_last_of(L"\\"));
            std::wstring target = parentInApp.empty() ? app : JoinPath(app, parentInApp);
            if (!CopyTree(srcDir, target)) {
                Post(notify, WM_APP_COMP_PROGRESS, 100,
                    L"copied, but some files could not be written (read-only install folder?)");
            }
        }
    }

    std::wstring finalCheck = JoinPath(app, verify);
    bool present = FileExists(finalCheck);
    RemoveTree(work);
    g_job.busy.store(false);

    if (!present) {
        PostResult(notify, 1,
            L"Downloaded and unpacked, but " + verify + L" is still missing. "
            L"If the app is in Program Files, run it as administrator.", sha);
        return 0;
    }
    PostResult(notify, 0, L"Installed. " + verify + L" is in place.", sha);
    return 0;
}

bool IsComponentJobRunning() { return g_job.busy.load(); }
void CancelComponentJob() { if (g_job.busy.load()) g_job.cancel.store(true); }

bool StartComponentInstall(int index, HWND notify) {
    if (index < 0 || index >= (int)g_components.size()) return false;
    if (g_job.busy.load()) return false;
    if (g_components[index].url.empty()) return false;
    g_job.cancel.store(false);
    g_job.notify = notify;
    g_job.busy.store(true);
    Component* copy = new Component(g_components[index]);
    HANDLE th = CreateThread(nullptr, 0, ComponentThread, copy, 0, nullptr);
    if (!th) {
        delete copy;
        g_job.busy.store(false);
        return false;
    }
    CloseHandle(th);
    return true;
}

// ------------------------------------------------------------- editing -----

int ComponentIndexByVerify(const wchar_t* verify) {
    int i = ComponentIndexByVerifyName(verify);
    return i;
}

int AddCustomComponent(const std::wstring& name, const std::wstring& url,
    const std::wstring& verify) {
    Component c;
    wchar_t idbuf[32];
    swprintf(idbuf, 32, L"custom-%lu", (unsigned long)GetTickCount());
    c.id = idbuf;
    c.name = name;
    c.category = L"Custom";
    c.description = L"Added by you. Files are unpacked into bin\\.";
    c.url = url;
    c.verifyFile = verify;
    c.destFolder = L"bin";
    c.formats = L"Custom";
    c.latestVersion = L"1.0";
    c.mode = InstallMode::VerifyParent;
    c.custom = true;
    c.urlEdited = true;
    g_components.insert(g_components.begin(), c);
    WriteComponentIni();
    RefreshComponentStatus();
    return 0;
}

bool RemoveComponentAt(int index) {
    if (index < 0 || index >= (int)g_components.size()) return false;
    if (!g_components[index].custom) return false;
    g_components.erase(g_components.begin() + index);
    WriteComponentIni();
    return true;
}

void SetComponentUrl(int index, const std::wstring& url) {
    if (index < 0 || index >= (int)g_components.size()) return;
    g_components[index].url = url;
    g_components[index].urlEdited = true;
    WriteComponentIni();
}

void PinComponentHash(int index, const std::wstring& sha) {
    if (index < 0 || index >= (int)g_components.size()) return;
    if (sha.empty()) return;
    g_components[index].sha256 = sha;
    WriteComponentIni();
}
