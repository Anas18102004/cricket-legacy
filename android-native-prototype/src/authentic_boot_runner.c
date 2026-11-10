#include "win32.h"
#include "runtime.h"
#include "recomp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <android/log.h>

#define TAG "CricketAuthentic"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__); printf(__VA_ARGS__); printf("\n")
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n")

static LoadedImage g_img;
static CPU g_cpu;
static jmp_buf g_local_trap_buf;

int main(int argc, char **argv) {
    LOGI("==================================================================");
    LOGI("CRICKET LEGACY: Authentic Game Engine Boot Runner (Android NDK)");
    LOGI("==================================================================");

    const char *game_path = (argc > 1) ? argv[1] : "/data/local/tmp/cricket07";
    snprintf(g_game_root, sizeof(g_game_root), "%s", game_path);
    snprintf(g_user_root, sizeof(g_user_root), "%s/userdata", game_path);
    mkdir(g_user_root, 0755);

    LOGI("[Phase E.1] Game root: %s", g_game_root);
    LOGI("[Phase E.1] User root: %s", g_user_root);

    // 1. Wire all host Windows API import tables
    LOGI("[Phase E.2] Registering host import tables...");
    static ImportHandler all_imports[2048];
    uint32_t total_imports = 0;
    const ImportTable *tables[] = {
        &kernel32_imports,
        &user32_imports,
        &gdi32_imports,
        &advapi32_imports,
        &winmm_imports,
        &directx_imports
    };
    for (size_t t = 0; t < sizeof(tables) / sizeof(tables[0]); t++) {
        for (uint32_t i = 0; i < tables[t]->count; i++) {
            if (total_imports < sizeof(all_imports) / sizeof(all_imports[0])) {
                all_imports[total_imports++] = tables[t]->list[i];
            }
        }
    }
    set_import_handlers(all_imports, total_imports);
    LOGI("[Phase E.2] Registered %u host API handlers.", total_imports);

    // 2. Initialize runtime and CPU
    LOGI("[Phase E.3] Initializing guest 4GiB virtual memory & CPU...");
    runtime_init(&g_cpu);
    vclock_init();
    LOGI("[Phase E.3] Guest memory initialized at host pointer %p (ESP=0x%08x)", (void *)g_mem, g_cpu.esp);

    // 3. Register all 31,499 recompiled game functions
    LOGI("[Phase E.4] Registering 31,499 recompiled static functions...");
    register_all_functions();
    LOGI("[Phase E.4] All recompiled functions registered in hash table.");

    // 4. Load PE image into guest memory
    char exe_file[1024];
    snprintf(exe_file, sizeof(exe_file), "%s/Cricket07.exe", g_game_root);
    LOGI("[Phase E.5] Loading authentic PE image from: %s", exe_file);

    if (access(exe_file, R_OK) != 0) {
        LOGE("[Phase E.5] ERROR: Executable not accessible at %s", exe_file);
        return 1;
    }

    if (load_image(exe_file, &g_img) != 0) {
        LOGE("[Phase E.5] ERROR: Failed to load PE image into guest address space!");
        return 2;
    }
    LOGI("[Phase E.5] PE Image Loaded: Base=0x%08x, Entry=0x%08x, ImportsBound=%u",
         g_img.image_base, g_img.entry, g_img.num_imports);

    // 5. Verify Entry Point
    GuestFn entry_fn = lookup_function(g_img.entry);
    if (!entry_fn) {
        LOGE("[Phase E.6] FATAL: Entry point 0x%08x not found in recompiled functions!", g_img.entry);
        return 3;
    }
    LOGI("[Phase E.6] Entry point verified -> function pointer %p", (void *)entry_fn);

    // 6. Set up guest trap handler for graceful recovery
    g_trap_jmp = &g_local_trap_buf;
    if (setjmp(g_local_trap_buf) != 0) {
        LOGE("[Phase E] GUEST TRAP INTERCEPTED: %s", g_trap_msg);
        LOGI("[Phase E] CPU State at Trap: EAX=0x%08x, ECX=0x%08x, EDX=0x%08x, EBX=0x%08x, ESP=0x%08x, EBP=0x%08x, ESI=0x%08x, EDI=0x%08x",
             g_cpu.eax, g_cpu.ecx, g_cpu.edx, g_cpu.ebx, g_cpu.esp, g_cpu.ebp, g_cpu.esi, g_cpu.edi);
        return 42;
    }

    // 7. Initialize thread and FPU rounding mode
    thread_main_init(&g_cpu);
    sched_main_init();
    if (t_self) {
        t_self->cpu = g_cpu;
    }
    fpu_sync_rounding(&g_cpu);

    // 8. Boot authentic game!
    LOGI("[Phase E.7] Booting authentic CRT startup trampoline at 0x%08x...", g_img.entry);
    PUSH32(&g_cpu, GUEST_RETURN_SENTINEL);

    entry_fn(&g_cpu);

    LOGI("[Phase E.8] Game WinMain returned gracefully with exit code: %d", (int)g_cpu.eax);
    LOGI("==================================================================");
    LOGI("PHASE E BOOT TEST: SUCCESSFUL AUTHENTIC ENGINE INITIALIZATION");
    LOGI("==================================================================");

    return (int)g_cpu.eax;
}
