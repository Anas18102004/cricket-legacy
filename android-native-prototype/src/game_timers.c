#include "game_timers.h"
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>

#define LOG_TAG "CricketTimers"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#define QPC_FREQUENCY 1000000000ULL // 1 GHz (nanosecond resolution)

static double g_timer_start_s = -1.0;
static pthread_mutex_t g_timer_lock = PTHREAD_MUTEX_INITIALIZER;

static double get_monotonic_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static double get_realtime_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec * 1e-6;
}

void game_timers_init(void) {
    pthread_mutex_lock(&g_timer_lock);
    if (g_timer_start_s < 0.0) {
        g_timer_start_s = get_monotonic_s();
    }
    pthread_mutex_unlock(&g_timer_lock);
    LOGI("High-Resolution Timers Initialized. QPC Frequency: %llu Hz", (unsigned long long)QPC_FREQUENCY);
}

double timer_host_seconds(void) {
    return host_seconds();
}

bool timer_query_performance_frequency(uint64_t *freq) {
    if (!freq) return false;
    *freq = QPC_FREQUENCY;
    return true;
}

bool timer_query_performance_counter(uint64_t *counter) {
    if (!counter) return false;
    double s = timer_host_seconds();
    *counter = (uint64_t)(s * 1e9);
    return true;
}

uint32_t timer_time_get_time(void) {
    return (uint32_t)(timer_host_seconds() * 1000.0);
}

uint32_t timer_get_tick_count(void) {
    return (uint32_t)(timer_host_seconds() * 1000.0);
}

uint64_t timer_host_time_100ns(void) {
    double ms = get_realtime_ms();
    // 11644473600ULL is seconds between 1601-01-01 and 1970-01-01 (Windows epoch offset)
    return (uint64_t)(ms * 10000.0) + 11644473600ULL * 10000000ULL;
}

void timer_sleep_ms(uint32_t ms) {
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&req, NULL);
}

// ------------------------------------------------------------------ Win32 Guest stdcall Handlers

void w_QueryPerformanceFrequency(CPU *c) {
    uint32_t out_ptr = ARG(0);
    if (out_ptr) {
        W64(out_ptr, QPC_FREQUENCY);
    }
    RET(1, 1);
}

void w_QueryPerformanceCounter(CPU *c) {
    uint32_t out_ptr = ARG(0);
    if (out_ptr) {
        uint64_t ticks = 0;
        timer_query_performance_counter(&ticks);
        W64(out_ptr, ticks);
    }
    RET(1, 1);
}

void w_timeGetTime(CPU *c) {
    uint32_t ms = timer_time_get_time();
    RET(0, ms);
}

void w_timeBeginPeriod(CPU *c) {
    // Android Linux timer granularity is automatically high-resolution
    RET(1, TIMERR_NOERROR);
}

void w_timeEndPeriod(CPU *c) {
    RET(1, TIMERR_NOERROR);
}

void w_GetTickCount(CPU *c) {
    uint32_t ms = timer_get_tick_count();
    RET(0, ms);
}

void w_GetSystemTimeAsFileTime(CPU *c) {
    uint32_t out_ptr = ARG(0);
    if (out_ptr) {
        uint64_t ft = timer_host_time_100ns();
        W64(out_ptr, ft);
    }
    RET(1, 0);
}
