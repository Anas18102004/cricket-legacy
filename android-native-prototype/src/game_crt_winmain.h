#ifndef GAME_CRT_WINMAIN_H
#define GAME_CRT_WINMAIN_H

#include <stdint.h>
#include <stdbool.h>
#include "upstream_d3d8/win32.h"

#ifdef __cplusplus
extern "C" {
#endif

// Win32 ShowWindow constants
#ifndef SW_HIDE
#define SW_HIDE         0
#define SW_SHOWNORMAL   1
#define SW_SHOWMINIMIZED 2
#define SW_SHOWMAXIMIZED 3
#define SW_SHOWDEFAULT  10
#endif

#ifndef VER_PLATFORM_WIN32_NT
#define VER_PLATFORM_WIN32_NT 2
#endif

#ifndef GUEST_RETURN_SENTINEL
#define GUEST_RETURN_SENTINEL 0xFFFFFFFEu
#endif

#ifndef HEAP_HANDLE
#define HEAP_HANDLE 0x00010000u
#endif

// Win32 STARTUPINFOA structure definition
typedef struct {
    uint32_t cb;
    uint32_t lpReserved;
    uint32_t lpDesktop;
    uint32_t lpTitle;
    uint32_t dwX;
    uint32_t dwY;
    uint32_t dwXSize;
    uint32_t dwYSize;
    uint32_t dwXCountChars;
    uint32_t dwYCountChars;
    uint32_t dwFillAttribute;
    uint32_t dwFlags;
    uint16_t wShowWindow;
    uint16_t cbReserved2;
    uint32_t lpReserved2;
    uint32_t hStdInput;
    uint32_t hStdOutput;
    uint32_t hStdError;
} Win32StartupInfoA;

// Win32 OSVERSIONINFOA structure definition
typedef struct {
    uint32_t dwOSVersionInfoSize;
    uint32_t dwMajorVersion;
    uint32_t dwMinorVersion;
    uint32_t dwBuildNumber;
    uint32_t dwPlatformId;
    char szCSDVersion[128];
} Win32OSVersionInfoA;

// System Lifecycle & String Helpers
bool crt_startup_init(const char *cmd_line, const char *exe_name);
void crt_startup_shutdown(void);
uint32_t guest_strdup(const char *s);
uint32_t module_for(const char *name);

// Win32 Guest stdcall Handlers (stdcall: CPU *c)
void w_GetModuleHandleA(CPU *c);
void w_GetCommandLineA(CPU *c);
void w_GetCommandLineW(CPU *c);
void w_GetStartupInfoA(CPU *c);
void w_GetEnvironmentStrings(CPU *c);
void w_FreeEnvironmentStringsA(CPU *c);
void w_GetVersionExA(CPU *c);
void w_GetProcessHeap(CPU *c);
void w_HeapAlloc(CPU *c);
void w_HeapFree(CPU *c);
void w_HeapReAlloc(CPU *c);
void w_GetStdHandle(CPU *c);
void w_ExitProcess(CPU *c);

// Dedicated WinMain Launch Trampoline
bool crt_launch_winmain(GuestFn winmain_fn, uint32_t image_base, int n_cmd_show, int *out_exit_code);

// Clearly Labeled Synthetic Test Program (For Stage 3 Validation Only)
// NOTE: This is NOT authentic EA game code; it is a validation harness that
// verifies the 32-bit WinMain ABI, stack parameters, and CRT imports.
void test_crt_winmain_program(CPU *c);

#ifdef __cplusplus
}
#endif

#endif // GAME_CRT_WINMAIN_H
