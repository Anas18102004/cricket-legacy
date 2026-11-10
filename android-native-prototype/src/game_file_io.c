#include "game_file_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "CricketFileIO"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define MAX_HANDLES 1024
#define HANDLE_BASE 0x00010000u

static WinHandle g_handles[MAX_HANDLES];
static pthread_mutex_t g_handle_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_io_initialized = false;
static uint32_t g_last_error = ERROR_SUCCESS;

char g_game_root[1024] = "/data/data/org.cricketlegacy/files/game";
char g_user_root[1024] = "/data/data/org.cricketlegacy/files/userdata";
static char g_cwd[1024] = PROFILE_GUEST_ROOT;

static void set_last_error(uint32_t err) {
    g_last_error = err;
    if (t_self) {
        t_self->last_error = err;
    }
}

// ------------------------------------------------------------------ Win32 Handles

uint32_t handle_new(int type) {
    pthread_mutex_lock(&g_handle_lock);
    for (uint32_t i = 1; i < MAX_HANDLES; i++) {
        if (g_handles[i].type == H_FREE) {
            g_handles[i].type = type;
            g_handles[i].fd = -1;
            g_handles[i].ptr = NULL;
            g_handles[i].path[0] = '\0';
            pthread_mutex_unlock(&g_handle_lock);
            return HANDLE_BASE + i;
        }
    }
    pthread_mutex_unlock(&g_handle_lock);
    LOGE("CRITICAL: Win32 handle table exhausted (max %d)!", MAX_HANDLES);
    set_last_error(ERROR_ACCESS_DENIED);
    return INVALID_HANDLE_VALUE;
}

WinHandle *handle_get(uint32_t h, int type) {
    if (h < HANDLE_BASE || h >= HANDLE_BASE + MAX_HANDLES) {
        return NULL;
    }
    uint32_t idx = h - HANDLE_BASE;
    pthread_mutex_lock(&g_handle_lock);
    if (g_handles[idx].type != type && type != H_FREE) {
        pthread_mutex_unlock(&g_handle_lock);
        return NULL;
    }
    pthread_mutex_unlock(&g_handle_lock);
    return &g_handles[idx];
}

void handle_close(uint32_t h) {
    if (h < HANDLE_BASE || h >= HANDLE_BASE + MAX_HANDLES) {
        return;
    }
    uint32_t idx = h - HANDLE_BASE;
    pthread_mutex_lock(&g_handle_lock);
    if (g_handles[idx].type != H_FREE) {
        if (g_handles[idx].fd >= 0) {
            close(g_handles[idx].fd);
            g_handles[idx].fd = -1;
        }
        g_handles[idx].ptr = NULL;
        g_handles[idx].path[0] = '\0';
        g_handles[idx].type = H_FREE;
    }
    pthread_mutex_unlock(&g_handle_lock);
}

// ------------------------------------------------------------------ Path Resolution

const char *guest_cwd(void) {
    return g_cwd;
}

void guest_set_cwd(const char *p) {
    if (p) {
        snprintf(g_cwd, sizeof(g_cwd), "%s", p);
    }
}

// Case-insensitive component lookup on Android/Linux filesystem
static int find_ci(const char *dir, const char *comp, char *real, size_t real_size) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *de;
    int found = 0;
    while ((de = readdir(d)) != NULL) {
        if (strcasecmp(de->d_name, comp) == 0) {
            snprintf(real, real_size, "%s", de->d_name);
            found = 1;
            break;
        }
    }
    closedir(d);
    return found;
}

static void ensure_parent_dirs(const char *host_path) {
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", host_path);
    for (char *p = dir + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(dir, 0755);
            *p = '/';
        }
    }
}

int map_path(const char *guest, char *host, size_t n) {
    if (!guest || !host || n == 0) return 0;

    char full[1024];
    // Convert guest path to absolute Windows format
    if (isalpha((unsigned char)guest[0]) && guest[1] == ':') {
        snprintf(full, sizeof(full), "%s", guest);
    } else if (guest[0] == '\\' || guest[0] == '/') {
        snprintf(full, sizeof(full), "C:%s", guest);
    } else {
        snprintf(full, sizeof(full), "%s\\%s", g_cwd, guest);
    }

    // Normalize forward slashes to backslashes for guest parsing
    for (char *p = full; *p; p++) {
        if (*p == '/') *p = '\\';
    }

    const char *rest = NULL;
    const char *base = NULL;
    size_t guest_root_len = strlen(PROFILE_GUEST_ROOT);

    if (strncasecmp(full, PROFILE_GUEST_ROOT, guest_root_len) == 0 &&
        (full[guest_root_len] == '\\' || full[guest_root_len] == '\0')) {
        base = g_game_root;
        rest = full + guest_root_len;
    } else {
        base = g_user_root;
        rest = full + 2; // Skip drive letter e.g. "C:"
    }

    char out[1024];
    snprintf(out, sizeof(out), "%s", base);
    int exists = 1;

    while (*rest) {
        while (*rest == '\\') rest++;
        if (!*rest) break;
        size_t k = 0;
        while (rest[k] && rest[k] != '\\') k++;
        char comp[512];
        snprintf(comp, sizeof(comp), "%.*s", (int)k, rest);
        rest += k;

        if (strcmp(comp, ".") == 0) continue;
        if (strcmp(comp, "..") == 0) {
            char *sl = strrchr(out, '/');
            if (sl && sl > out + strlen(base) - 1) {
                *sl = '\0';
            }
            continue;
        }

        char real[512];
        if (exists && find_ci(out, comp, real, sizeof(real))) {
            size_t len = strlen(out);
            snprintf(out + len, sizeof(out) - len, "/%s", real);
        } else {
            exists = 0;
            size_t len = strlen(out);
            snprintf(out + len, sizeof(out) - len, "/%s", comp);
        }
    }

    snprintf(host, n, "%s", out);
    struct stat st;
    return (stat(host, &st) == 0);
}

// ------------------------------------------------------------------ System Lifecycle

bool game_file_io_init(const char *game_root, const char *user_root) {
    if (game_root && strlen(game_root) > 0) {
        snprintf(g_game_root, sizeof(g_game_root), "%s", game_root);
    }
    if (user_root && strlen(user_root) > 0) {
        snprintf(g_user_root, sizeof(g_user_root), "%s", user_root);
    }

    ensure_parent_dirs(g_game_root);
    ensure_parent_dirs(g_user_root);
    mkdir(g_game_root, 0755);
    mkdir(g_user_root, 0755);

    pthread_mutex_lock(&g_handle_lock);
    memset(g_handles, 0, sizeof(g_handles));
    pthread_mutex_unlock(&g_handle_lock);

    g_io_initialized = true;
    LOGI("File I/O Subsystem Initialized. GameRoot: %s, UserRoot: %s", g_game_root, g_user_root);
    return true;
}

void game_file_io_shutdown(void) {
    pthread_mutex_lock(&g_handle_lock);
    for (uint32_t i = 1; i < MAX_HANDLES; i++) {
        if (g_handles[i].type != H_FREE) {
            if (g_handles[i].fd >= 0) {
                close(g_handles[i].fd);
            }
            g_handles[i].type = H_FREE;
        }
    }
    pthread_mutex_unlock(&g_handle_lock);
    g_io_initialized = false;
    LOGI("File I/O Subsystem shut down cleanly.");
}

// ------------------------------------------------------------------ Direct C Wrappers

uint32_t win32_create_file(const char *path, uint32_t access, uint32_t disp) {
    char host[1024];
    int exists = map_path(path, host, sizeof(host));

    int fl = 0;
    if ((access & (GENERIC_READ | GENERIC_WRITE)) == (GENERIC_READ | GENERIC_WRITE)) {
        fl = O_RDWR;
    } else if (access & GENERIC_WRITE) {
        fl = O_WRONLY;
    } else {
        fl = O_RDONLY;
    }

    switch (disp) {
        case CREATE_NEW:
            fl |= O_CREAT | O_EXCL;
            break;
        case CREATE_ALWAYS:
            fl |= O_CREAT | O_TRUNC;
            break;
        case OPEN_EXISTING:
            break;
        case OPEN_ALWAYS:
            fl |= O_CREAT;
            break;
        case TRUNCATE_EXISTING:
            fl |= O_TRUNC;
            break;
        default:
            break;
    }

    if (fl & O_CREAT) {
        ensure_parent_dirs(host);
    }

    int fd = open(host, fl, 0644);
    if (fd < 0) {
        set_last_error(exists ? ERROR_ACCESS_DENIED : ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }

    uint32_t h = handle_new(H_FILE);
    if (h == INVALID_HANDLE_VALUE) {
        close(fd);
        return INVALID_HANDLE_VALUE;
    }

    WinHandle *wh = handle_get(h, H_FILE);
    wh->fd = fd;
    snprintf(wh->path, sizeof(wh->path), "%s", host);

    set_last_error(((disp == OPEN_ALWAYS || disp == CREATE_ALWAYS) && exists) ? ERROR_ALREADY_EXISTS : ERROR_SUCCESS);
    return h;
}

bool win32_read_file(uint32_t handle, void *buffer, uint32_t bytes_to_read, uint32_t *bytes_read) {
    WinHandle *wh = handle_get(handle, H_FILE);
    if (!wh || wh->fd < 0) {
        set_last_error(ERROR_INVALID_HANDLE);
        if (bytes_read) *bytes_read = 0;
        return false;
    }

    uint32_t got = 0;
    ssize_t n = 0;
    uint8_t *p = (uint8_t *)buffer;

    while (got < bytes_to_read && (n = read(wh->fd, p + got, bytes_to_read - got)) > 0) {
        got += (uint32_t)n;
    }

    if (bytes_read) *bytes_read = got;
    if (n < 0) {
        set_last_error(ERROR_ACCESS_DENIED);
        return false;
    }

    set_last_error(ERROR_SUCCESS);
    return true;
}

bool win32_write_file(uint32_t handle, const void *buffer, uint32_t bytes_to_write, uint32_t *bytes_written) {
    WinHandle *wh = handle_get(handle, H_FILE);
    if (!wh || wh->fd < 0) {
        set_last_error(ERROR_INVALID_HANDLE);
        if (bytes_written) *bytes_written = 0;
        return false;
    }

    ssize_t n = write(wh->fd, buffer, bytes_to_write);
    if (n < 0) {
        set_last_error(ERROR_ACCESS_DENIED);
        if (bytes_written) *bytes_written = 0;
        return false;
    }

    if (bytes_written) *bytes_written = (uint32_t)n;
    set_last_error(ERROR_SUCCESS);
    return true;
}

int64_t win32_set_file_pointer(uint32_t handle, int64_t offset, uint32_t method) {
    WinHandle *wh = handle_get(handle, H_FILE);
    if (!wh || wh->fd < 0) {
        set_last_error(ERROR_INVALID_HANDLE);
        return -1;
    }

    int whence = SEEK_SET;
    if (method == FILE_CURRENT) whence = SEEK_CUR;
    else if (method == FILE_END) whence = SEEK_END;

    off_t res = lseek(wh->fd, (off_t)offset, whence);
    if (res == (off_t)-1) {
        set_last_error(ERROR_INVALID_HANDLE);
        return -1;
    }

    set_last_error(ERROR_SUCCESS);
    return (int64_t)res;
}

uint32_t win32_get_file_size(uint32_t handle, uint32_t *high_size) {
    WinHandle *wh = handle_get(handle, H_FILE);
    if (!wh || wh->fd < 0) {
        set_last_error(ERROR_INVALID_HANDLE);
        if (high_size) *high_size = 0;
        return (uint32_t)-1;
    }

    struct stat st;
    if (fstat(wh->fd, &st) != 0) {
        set_last_error(ERROR_INVALID_HANDLE);
        return (uint32_t)-1;
    }

    if (high_size) {
        *high_size = (uint32_t)((uint64_t)st.st_size >> 32);
    }
    set_last_error(ERROR_SUCCESS);
    return (uint32_t)(st.st_size & 0xFFFFFFFFU);
}

bool win32_close_handle(uint32_t handle) {
    WinHandle *wh = handle_get(handle, H_FILE);
    if (!wh) {
        set_last_error(ERROR_INVALID_HANDLE);
        return false;
    }
    handle_close(handle);
    set_last_error(ERROR_SUCCESS);
    return true;
}

// ------------------------------------------------------------------ Win32 Guest stdcall Handlers

void w_CreateFileA(CPU *c) {
    const char *name = GSTR(ARG(0));
    uint32_t access = ARG(1);
    uint32_t disp = ARG(4);
    uint32_t h = win32_create_file(name, access, disp);
    RET(7, h);
}

void w_CreateFileW(CPU *c) {
    char name[512];
    uint32_t p = ARG(0);
    int i = 0;
    for (; i < 511 && R16(p + 2 * i); i++) {
        name[i] = (char)R16(p + 2 * i);
    }
    name[i] = '\0';
    uint32_t access = ARG(1);
    uint32_t disp = ARG(4);
    uint32_t h = win32_create_file(name, access, disp);
    RET(7, h);
}

void w_ReadFile(CPU *c) {
    uint32_t h = ARG(0);
    void *buf = HP(ARG(1));
    uint32_t want = ARG(2);
    uint32_t got = 0;
    bool ok = win32_read_file(h, buf, want, &got);
    if (ARG(3)) {
        W32(ARG(3), got);
    }
    RET(5, ok ? 1 : 0);
}

void w_WriteFile(CPU *c) {
    uint32_t h = ARG(0);
    const void *buf = HP(ARG(1));
    uint32_t want = ARG(2);
    uint32_t written = 0;
    bool ok = win32_write_file(h, buf, want, &written);
    if (ARG(3)) {
        W32(ARG(3), written);
    }
    RET(5, ok ? 1 : 0);
}

void w_SetFilePointer(CPU *c) {
    uint32_t h = ARG(0);
    int64_t off = (int32_t)ARG(1);
    if (ARG(2)) {
        off = (int64_t)(((uint64_t)R32(ARG(2)) << 32) | ARG(1));
    }
    uint32_t method = ARG(3);
    int64_t res = win32_set_file_pointer(h, off, method);
    if (res < 0) {
        RET(4, 0xFFFFFFFFu);
    }
    if (ARG(2)) {
        W32(ARG(2), (uint32_t)((uint64_t)res >> 32));
    }
    RET(4, (uint32_t)(res & 0xFFFFFFFFu));
}

void w_GetFileSize(CPU *c) {
    uint32_t h = ARG(0);
    uint32_t hi = 0;
    uint32_t lo = win32_get_file_size(h, &hi);
    if (ARG(1)) {
        W32(ARG(1), hi);
    }
    RET(2, lo);
}

void w_FlushFileBuffers(CPU *c) {
    uint32_t h = ARG(0);
    WinHandle *wh = handle_get(h, H_FILE);
    if (wh && wh->fd >= 0) {
        fsync(wh->fd);
    }
    RET(1, 1);
}

void w_DeleteFileA(CPU *c) {
    char host[1024];
    map_path(GSTR(ARG(0)), host, sizeof(host));
    int r = unlink(host);
    RET(1, (r == 0) ? 1 : 0);
}

void w_MoveFileA(CPU *c) {
    char a[1024], b[1024];
    map_path(GSTR(ARG(0)), a, sizeof(a));
    map_path(GSTR(ARG(1)), b, sizeof(b));
    int r = rename(a, b);
    RET(2, (r == 0) ? 1 : 0);
}

void w_CreateDirectoryA(CPU *c) {
    char host[1024];
    map_path(GSTR(ARG(0)), host, sizeof(host));
    int r = mkdir(host, 0755);
    RET(2, (r == 0) ? 1 : 0);
}

void w_CloseHandle(CPU *c) {
    uint32_t h = ARG(0);
    handle_close(h);
    RET(1, 1);
}
