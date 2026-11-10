#ifndef GAME_FILE_IO_H
#define GAME_FILE_IO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "upstream_d3d8/win32.h"

#ifdef __cplusplus
extern "C" {
#endif

// Win32 File Constants
#ifndef GENERIC_READ
#define GENERIC_READ    (0x80000000U)
#define GENERIC_WRITE   (0x40000000U)
#define GENERIC_EXECUTE (0x20000000U)
#define GENERIC_ALL     (0x10000000U)
#endif

#ifndef CREATE_NEW
#define CREATE_NEW        1
#define CREATE_ALWAYS     2
#define OPEN_EXISTING     3
#define OPEN_ALWAYS       4
#define TRUNCATE_EXISTING 5
#endif

#ifndef FILE_BEGIN
#define FILE_BEGIN   0
#define FILE_CURRENT 1
#define FILE_END     2
#endif

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((uint32_t)0xFFFFFFFFU)
#endif

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS           0
#define ERROR_FILE_NOT_FOUND    2
#define ERROR_ACCESS_DENIED     5
#define ERROR_INVALID_HANDLE    6
#define ERROR_ALREADY_EXISTS    183
#endif

// System initialization
bool game_file_io_init(const char *game_root, const char *user_root);
void game_file_io_shutdown(void);

// Path mapping & resolution
const char *guest_cwd(void);
void guest_set_cwd(const char *p);
int map_path(const char *guest, char *host, size_t n);

// Win32 Standard Import Handlers (stdcall: CPU *c)
void w_CreateFileA(CPU *c);
void w_CreateFileW(CPU *c);
void w_ReadFile(CPU *c);
void w_WriteFile(CPU *c);
void w_SetFilePointer(CPU *c);
void w_GetFileSize(CPU *c);
void w_FlushFileBuffers(CPU *c);
void w_DeleteFileA(CPU *c);
void w_MoveFileA(CPU *c);
void w_CreateDirectoryA(CPU *c);
void w_CloseHandle(CPU *c);

// Direct C API Wrappers (for internal calls, tests, and harness verification)
uint32_t win32_create_file(const char *path, uint32_t access, uint32_t disp);
bool win32_read_file(uint32_t handle, void *buffer, uint32_t bytes_to_read, uint32_t *bytes_read);
bool win32_write_file(uint32_t handle, const void *buffer, uint32_t bytes_to_write, uint32_t *bytes_written);
int64_t win32_set_file_pointer(uint32_t handle, int64_t offset, uint32_t method);
uint32_t win32_get_file_size(uint32_t handle, uint32_t *high_size);
bool win32_close_handle(uint32_t handle);

#ifdef __cplusplus
}
#endif

#endif // GAME_FILE_IO_H
