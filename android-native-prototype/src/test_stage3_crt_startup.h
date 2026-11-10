#ifndef TEST_STAGE3_CRT_STARTUP_H
#define TEST_STAGE3_CRT_STARTUP_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Runs the comprehensive Phase C.2 Stage 3 CRT Startup and WinMain Trampoline verification suite
bool run_stage3_crt_startup_tests(void);

#ifdef __cplusplus
}
#endif

#endif // TEST_STAGE3_CRT_STARTUP_H
