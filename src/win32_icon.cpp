#include "win32_icon.hpp"

#if defined(_WIN32)

// Declaraciones nativas directas de Win32 para evitar la inclusion pesada de <windows.h>,
// previniendo colisiones de macros y advertencias de clangd con intrinsics de GCC (__rdtsc).
extern "C" {
    __declspec(dllimport) void* __stdcall LoadLibraryW(const wchar_t* lpLibFileName);
    __declspec(dllimport) void* __stdcall GetProcAddress(void* hModule, const char* lpProcName);
    __declspec(dllimport) void* __stdcall GetModuleHandleW(const wchar_t* lpModuleName);
    __declspec(dllimport) void* __stdcall LoadImageW(void* hInst, const wchar_t* name, unsigned int type, int cx, int cy, unsigned int fuLoad);
    __declspec(dllimport) long long __stdcall SendMessageW(void* hWnd, unsigned int Msg, unsigned long long wParam, long long lParam);
    __declspec(dllimport) int __stdcall SetForegroundWindow(void* hWnd);
    __declspec(dllimport) void* __stdcall SetFocus(void* hWnd);
    __declspec(dllimport) void* __stdcall SetActiveWindow(void* hWnd);

#if defined(_WIN64)
    __declspec(dllimport) unsigned long long __stdcall SetClassLongPtrW(void* hWnd, int nIndex, long long dwNewLong);
#else
    __declspec(dllimport) unsigned long __stdcall SetClassLongW(void* hWnd, int nIndex, long dwNewLong);
    #define SetClassLongPtrW SetClassLongW
#endif
}

constexpr unsigned int WIN32_IMAGE_ICON      = 1;
constexpr unsigned int WIN32_LR_DEFAULTSIZE = 0x00000040;
constexpr unsigned int WIN32_LR_SHARED      = 0x00008000;
constexpr unsigned int WIN32_LR_LOADFROMFILE = 0x00000010;
constexpr unsigned int WIN32_WM_SETICON     = 0x0080;
constexpr unsigned long long WIN32_ICON_SMALL = 0;
constexpr unsigned long long WIN32_ICON_BIG   = 1;
constexpr int WIN32_GCLP_HICON   = -14;
constexpr int WIN32_GCLP_HICONSM = -34;

void SetupPlatformAppId()
{
    // No establecer AppUserModelID explicito si la aplicacion se ejecuta de forma portable
    // sin acceso directo (.lnk) en el menu de inicio de Windows; de lo contrario, la barra
    // de tareas de Windows 10/11 desacopla el proceso de su ejecutable y muestra un icono generico en blanco.
}

void SetupPlatformTaskbarIcon(void* windowHandle, const wchar_t* fallbackIcoPath)
{
    void* hwnd = windowHandle;
    if (hwnd == nullptr) return;

    const wchar_t* resourceId1 = reinterpret_cast<const wchar_t*>(static_cast<unsigned long long>(1));

    // 1. Cargar desde el recurso embebido del ejecutable (ID 1)
    void* hIconBig = LoadImageW(GetModuleHandleW(nullptr), resourceId1, WIN32_IMAGE_ICON, 0, 0, WIN32_LR_DEFAULTSIZE | WIN32_LR_SHARED);
    void* hIconSmall = LoadImageW(GetModuleHandleW(nullptr), resourceId1, WIN32_IMAGE_ICON, 16, 16, WIN32_LR_SHARED);

    // 2. Fallback a archivo si el recurso no respondiera
    if (hIconBig == nullptr && fallbackIcoPath != nullptr)
    {
        hIconBig = LoadImageW(nullptr, fallbackIcoPath, WIN32_IMAGE_ICON, 0, 0, WIN32_LR_LOADFROMFILE | WIN32_LR_DEFAULTSIZE);
        hIconSmall = LoadImageW(nullptr, fallbackIcoPath, WIN32_IMAGE_ICON, 16, 16, WIN32_LR_LOADFROMFILE);
    }

    if (hIconBig != nullptr)
    {
        SendMessageW(hwnd, WIN32_WM_SETICON, WIN32_ICON_BIG, reinterpret_cast<long long>(hIconBig));
        SetClassLongPtrW(hwnd, WIN32_GCLP_HICON, reinterpret_cast<long long>(hIconBig));
    }
    if (hIconSmall != nullptr)
    {
        SendMessageW(hwnd, WIN32_WM_SETICON, WIN32_ICON_SMALL, reinterpret_cast<long long>(hIconSmall));
        SetClassLongPtrW(hwnd, WIN32_GCLP_HICONSM, reinterpret_cast<long long>(hIconSmall));
    }
}

void ForceWindowActive(void* windowHandle)
{
    void* hwnd = windowHandle;
    if (hwnd == nullptr) return;
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
}
#else
void SetupPlatformAppId() {}
void SetupPlatformTaskbarIcon(void*, const wchar_t*) {}
void ForceWindowActive(void*) {}
#endif
