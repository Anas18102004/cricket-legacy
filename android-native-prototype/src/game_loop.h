#ifndef GAME_LOOP_H
#define GAME_LOOP_H

#include <stdint.h>
#include <stdbool.h>
#include "upstream_d3d8/win32.h"

#ifdef __cplusplus
extern "C" {
#endif

// Win32 Message Types
#ifndef WM_NULL
#define WM_NULL         0x0000
#define WM_CREATE       0x0001
#define WM_DESTROY      0x0002
#define WM_MOVE         0x0003
#define WM_SIZE         0x0005
#define WM_ACTIVATE     0x0006
#define WM_SETFOCUS     0x0007
#define WM_KILLFOCUS    0x0008
#define WM_PAINT        0x000F
#define WM_CLOSE        0x0010
#define WM_QUIT         0x0012
#define WM_ACTIVATEAPP  0x001C
#define WM_KEYDOWN      0x0100
#define WM_KEYUP        0x0101
#define WM_CHAR         0x0102
#define WM_MOUSEMOVE    0x0200
#define WM_LBUTTONDOWN  0x0201
#define WM_LBUTTONUP    0x0202
#define WM_RBUTTONDOWN  0x0204
#define WM_RBUTTONUP    0x0205
#define WM_USER         0x0400
#endif

#ifndef PM_NOREMOVE
#define PM_NOREMOVE 0
#define PM_REMOVE   1
#endif

// Win32 MSG structure layout
typedef struct {
    uint32_t hwnd;
    uint32_t msg;
    uint32_t wp;
    uint32_t lp;
    uint32_t time;
    int32_t pt_x;
    int32_t pt_y;
} Win32Msg;

// System Lifecycle
void game_loop_init(void);
void game_loop_shutdown(void);

// Win32 Message Queue API
void post_message(uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp);
bool pop_message(Win32Msg *m, bool remove);
uint32_t get_queue_message_count(void);

// Window Procedure & stdcall dispatch
uint32_t guest_stdcall(CPU *c, uint32_t fn, int nargs, const uint32_t *args);
void register_window_proc(uint32_t wndproc_fn);
uint32_t get_window_proc(void);
uint32_t send_to_wndproc(CPU *c, uint32_t hwnd, uint32_t msg, uint32_t wp, uint32_t lp);

// Android Lifecycle & Input Bridging
void game_loop_set_paused(bool paused);
bool game_loop_is_paused(void);
void game_loop_post_touch(int action, float x, float y);
void game_loop_post_key(int key_code, bool down);

// Frame Pacing (60 Hz target pacing)
void game_loop_pace_frame(uint64_t *last_frame_ns, uint32_t target_hz);

// Win32 Guest stdcall Handlers (stdcall: CPU *c)
void w_PeekMessageA(CPU *c);
void w_GetMessageA(CPU *c);
void w_TranslateMessage(CPU *c);
void w_DispatchMessageA(CPU *c);
void w_PostMessageA(CPU *c);
void w_SendMessageA(CPU *c);
void w_PostQuitMessage(CPU *c);
void w_DefWindowProcA(CPU *c);
void w_RegisterClassA(CPU *c);
void w_RegisterClassExA(CPU *c);

// Clearly Labeled Synthetic Game Operating Loop (Stage 4 Validation Only)
// NOTE: This is NOT authentic EA game code; it is a test loop validating
// message pumping, window procedure dispatch, frame pacing, and pause/resume.
void test_game_loop_program(CPU *c);
void test_custom_wndproc(CPU *c);

#ifdef __cplusplus
}
#endif

#endif // GAME_LOOP_H
