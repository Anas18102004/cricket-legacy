#ifndef GAME_TIMERS_H
#define GAME_TIMERS_H

#include <stdint.h>
#include <stdbool.h>
#include "upstream_d3d8/win32.h"

#ifdef __cplusplus
extern "C" {
#endif

// WinMM Timer Error Codes
#ifndef TIMERR_NOERROR
#define TIMERR_NOERROR 0
#endif

// System initialization
void game_timers_init(void);

// High-Resolution Performance Counter & Frequency
bool timer_query_performance_frequency(uint64_t *freq);
bool timer_query_performance_counter(uint64_t *counter);

// WinMM Multimedia & Win32 System Clocks
uint32_t timer_time_get_time(void);
uint32_t timer_get_tick_count(void);
double timer_host_seconds(void);
uint64_t timer_host_time_100ns(void);
void timer_sleep_ms(uint32_t ms);

// Win32 Guest stdcall Handlers (stdcall: CPU *c)
void w_QueryPerformanceFrequency(CPU *c);
void w_QueryPerformanceCounter(CPU *c);
void w_timeGetTime(CPU *c);
void w_timeBeginPeriod(CPU *c);
void w_timeEndPeriod(CPU *c);
void w_GetTickCount(CPU *c);
void w_GetSystemTimeAsFileTime(CPU *c);

#ifdef __cplusplus
}
#endif

#endif // GAME_TIMERS_H
