/* Shared by NX2navi.exe (nx2navi.c) and NX2navi-Setup.exe (setup.c): paths, small files, shortcuts and the
   uninstall entry. Wide-character Win32 throughout, so folder names in any language work. */
#pragma once
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define PATHN 1024
#define APP_NAME L"NX2navi"
#define UNINSTALL_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NX2navi"
/* The launcher window's taskbar identity (launcher/winsys.py app_id): a shortcut that carries it is the one the
   taskbar groups and pins the window under, with the shortcut's icon. */
#define APP_USER_MODEL_ID L"NX2navi.Launcher"
static const PROPERTYKEY NX_PKEY_AppUserModel_ID = {
    {0x9F4C2855, 0x9F79, 0x4B39, {0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3}}, 5};
static const IID NX_IID_IPropertyStore = {0x886d8eeb, 0x8cf2, 0x4446, {0x8d, 0x02, 0xcd, 0xba, 0x1d, 0xbd, 0xcf, 0x99}};

static inline void exe_dir(wchar_t *out, size_t n)
{
    DWORD len = GetModuleFileNameW(NULL, out, (DWORD)n);
    if (len == 0 || len >= n) { out[0] = 0; return; }
    while (len > 0 && out[len - 1] != L'\\' && out[len - 1] != L'/') len--;
    out[len > 0 ? len - 1 : 0] = 0;
}

static inline int path_exists(const wchar_t *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }

static inline int is_dir(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

/* The first line of a small UTF-8 text file, trimmed, as wide text; 0 when absent or empty. */
static inline int read_line(const wchar_t *path, wchar_t *out, size_t n)
{
    char buf[256];
    DWORD got = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    if (!ReadFile(f, buf, sizeof buf - 1, &got, NULL)) got = 0;
    CloseHandle(f);
    buf[got] = 0;
    char *s = buf;
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strcspn(s, "\r\n");
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) len--;
    s[len] = 0;
    if (!len) return 0;
    return MultiByteToWideChar(CP_UTF8, 0, s, -1, out, (int)n) > 1;
}

static inline int write_line(const wchar_t *path, const wchar_t *text)
{
    char buf[512];
    int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, buf, sizeof buf - 2, NULL, NULL);
    if (len <= 0) return 0;
    buf[len - 1] = '\n';
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD put = 0;
    BOOL ok = WriteFile(f, buf, (DWORD)len, &put, NULL);
    CloseHandle(f);
    return ok && put == (DWORD)len;
}

/* Compare dotted versions ("0.3.11" > "0.3.9"); build metadata after '+' is ignored. */
static inline int version_cmp(const wchar_t *a, const wchar_t *b)
{
    for (;;) {
        long x = 0, y = 0;
        while (*a >= L'0' && *a <= L'9') x = x * 10 + (*a++ - L'0');
        while (*b >= L'0' && *b <= L'9') y = y * 10 + (*b++ - L'0');
        if (x != y) return x < y ? -1 : 1;
        int ea = *a != L'.', eb = *b != L'.';
        if (ea && eb) return 0;
        if (!ea) a++;
        if (!eb) b++;
        if (ea && !eb) { while (*b == L'0' || *b == L'.') b++; return (*b >= L'1' && *b <= L'9') ? -1 : 0; }
        if (eb && !ea) { while (*a == L'0' || *a == L'.') a++; return (*a >= L'1' && *a <= L'9') ? 1 : 0; }
    }
}

static inline int known_folder(REFKNOWNFOLDERID id, wchar_t *out, size_t n)
{
    PWSTR p = NULL;
    if (FAILED(SHGetKnownFolderPath(id, 0, NULL, &p)) || !p) return 0;
    int ok = wcslen(p) < n;
    if (ok) wcscpy(out, p);
    CoTaskMemFree(p);
    return ok;
}

/* A .lnk at lnk that starts target (with its own icon). */
static inline int make_shortcut(const wchar_t *lnk, const wchar_t *target, const wchar_t *workdir, const wchar_t *desc)
{
    IShellLinkW *sl = NULL;
    IPersistFile *pf = NULL;
    int ok = 0;
    HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl))) {
        IShellLinkW_SetPath(sl, target);
        IShellLinkW_SetWorkingDirectory(sl, workdir);
        IShellLinkW_SetIconLocation(sl, target, 0);
        IShellLinkW_SetDescription(sl, desc);
        IPropertyStore *ps = NULL;
        if (SUCCEEDED(IShellLinkW_QueryInterface(sl, &NX_IID_IPropertyStore, (void **)&ps))) {
            PROPVARIANT pv;
            memset(&pv, 0, sizeof pv);
            pv.vt = VT_LPWSTR;
            pv.pwszVal = (LPWSTR)APP_USER_MODEL_ID;
            IPropertyStore_SetValue(ps, &NX_PKEY_AppUserModel_ID, &pv);
            IPropertyStore_Commit(ps);
            IPropertyStore_Release(ps);
        }
        if (SUCCEEDED(IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) {
            ok = SUCCEEDED(IPersistFile_Save(pf, lnk, TRUE));
            IPersistFile_Release(pf);
        }
        IShellLinkW_Release(sl);
    }
    if (SUCCEEDED(init)) CoUninitialize();
    return ok;
}

/* The Desktop or Start menu shortcut's path. */
static inline int shortcut_path(int desktop, wchar_t *out, size_t n)
{
    wchar_t dir[PATHN];
    if (!known_folder(desktop ? &FOLDERID_Desktop : &FOLDERID_Programs, dir, PATHN)) return 0;
    return swprintf(out, n, L"%ls\\%ls.lnk", dir, APP_NAME) > 0;
}

/* Create (on) or remove (off) the Desktop and/or Start menu shortcut to root\NX2navi.exe. */
static inline int set_shortcut(const wchar_t *root, int desktop, int on)
{
    wchar_t lnk[PATHN], target[PATHN];
    if (!shortcut_path(desktop, lnk, PATHN)) return 0;
    if (!on) return DeleteFileW(lnk) || GetLastError() == ERROR_FILE_NOT_FOUND;
    swprintf(target, PATHN, L"%ls\\NX2navi.exe", root);
    return make_shortcut(lnk, target, root, L"Play and update NX2navi");
}

static inline void reg_str(HKEY k, const wchar_t *name, const wchar_t *value)
{
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
}

static inline void reg_dword(HKEY k, const wchar_t *name, DWORD value)
{
    RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE *)&value, sizeof value);
}

/* The entry in Settings > Apps that runs "NX2navi.exe --uninstall". */
static inline int write_uninstall_entry(const wchar_t *root, const wchar_t *version, DWORD size_kb)
{
    HKEY k;
    wchar_t buf[PATHN + 64];
    if (RegCreateKeyExW(HKEY_CURRENT_USER, UNINSTALL_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL) != ERROR_SUCCESS)
        return 0;
    reg_str(k, L"DisplayName", APP_NAME);
    reg_str(k, L"DisplayVersion", version);
    reg_str(k, L"Publisher", APP_NAME);
    reg_str(k, L"InstallLocation", root);
    swprintf(buf, PATHN + 64, L"\"%ls\\NX2navi.exe\",0", root);
    reg_str(k, L"DisplayIcon", buf);
    swprintf(buf, PATHN + 64, L"\"%ls\\NX2navi.exe\" --uninstall", root);
    reg_str(k, L"UninstallString", buf);
    reg_dword(k, L"NoModify", 1);
    reg_dword(k, L"NoRepair", 1);
    reg_dword(k, L"EstimatedSize", size_kb);
    RegCloseKey(k);
    return 1;
}
