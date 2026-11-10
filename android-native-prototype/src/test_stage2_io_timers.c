#include "test_stage2_io_timers.h"
#include "game_file_io.h"
#include "game_timers.h"
#include "game_lazy_views.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <android/log.h>

#define LOG_TAG "Stage2IOTests"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool test1_path_normalization_and_ci(void) {
    LOGI("[TEST 1] Testing Path Normalization and Case-Insensitive Matching...");

    char host_path[1024];
    // Test 1a: Windows absolute path mapping to g_game_root
    int exists = map_path("C:\\Cricket07\\data\\frontend\\menu.fsh", host_path, sizeof(host_path));
    LOGI("  1a. 'C:\\Cricket07\\data\\frontend\\menu.fsh' -> '%s' (exists: %d)", host_path, exists);
    if (!strstr(host_path, g_game_root) || !strstr(host_path, "data/frontend/menu.fsh")) {
        LOGE("  FAIL: Expected game root path containing 'data/frontend/menu.fsh'!");
        return false;
    }

    // Test 1b: Relative path mapping to g_game_root
    map_path("data\\audio\\speech.big", host_path, sizeof(host_path));
    LOGI("  1b. 'data\\audio\\speech.big' -> '%s'", host_path);
    if (!strstr(host_path, g_game_root) || !strstr(host_path, "data/audio/speech.big")) {
        LOGE("  FAIL: Relative path did not map to game root!");
        return false;
    }

    // Test 1c: Case-insensitive on-disk resolution
    // Create a real directory with mixed casing: ${g_game_root}/FeStReAm
    char test_dir[1024];
    snprintf(test_dir, sizeof(test_dir), "%s/FeStReAm", g_game_root);
    mkdir(test_dir, 0755);

    // Query with all-lowercase and all-uppercase
    map_path("c:\\cricket07\\festream\\cursor.fsh", host_path, sizeof(host_path));
    LOGI("  1c. Query 'festream' -> '%s'", host_path);
    if (!strstr(host_path, "FeStReAm")) {
        LOGE("  FAIL: find_ci failed to resolve on-disk casing 'FeStReAm'!");
        rmdir(test_dir);
        return false;
    }

    rmdir(test_dir);
    LOGI("  PASS: Test 1 Path Normalization and CI Resolution verified.");
    return true;
}

static bool test2_win32_file_creation(void) {
    LOGI("[TEST 2] Testing Win32 File Creation (CreateFileA)...");

    const char *test_filename = "C:\\Cricket07\\test_io_create.bin";
    uint32_t h = win32_create_file(test_filename, GENERIC_WRITE, CREATE_ALWAYS);
    if (h == INVALID_HANDLE_VALUE) {
        LOGE("  FAIL: win32_create_file CREATE_ALWAYS returned INVALID_HANDLE_VALUE!");
        return false;
    }

    WinHandle *wh = handle_get(h, H_FILE);
    if (!wh || wh->fd < 0) {
        LOGE("  FAIL: handle_get returned invalid WinHandle or negative fd!");
        win32_close_handle(h);
        return false;
    }
    LOGI("  File created successfully with handle 0x%08X (fd: %d, host path: %s)", h, wh->fd, wh->path);
    win32_close_handle(h);

    // Test CREATE_NEW on existing file: should fail with ERROR_ALREADY_EXISTS
    uint32_t h2 = win32_create_file(test_filename, GENERIC_WRITE, CREATE_NEW);
    if (h2 != INVALID_HANDLE_VALUE) {
        LOGE("  FAIL: CREATE_NEW on existing file succeeded unexpectedly!");
        win32_close_handle(h2);
        return false;
    }
    LOGI("  CREATE_NEW on existing file correctly failed with handle 0x%08X", h2);

    LOGI("  PASS: Test 2 Win32 File Creation verified.");
    return true;
}

static bool test3_file_write_and_read(void) {
    LOGI("[TEST 3] Testing File Write and Read Byte Parity...");

    const char *test_filename = "C:\\Cricket07\\test_io_rw.bin";
    uint32_t h = win32_create_file(test_filename, GENERIC_WRITE, CREATE_ALWAYS);
    if (h == INVALID_HANDLE_VALUE) {
        LOGE("  FAIL: Failed to create write test file!");
        return false;
    }

    // Generate 1024-byte test payload
    uint8_t write_buf[1024];
    for (int i = 0; i < 1024; i++) {
        write_buf[i] = (uint8_t)((i * 37 + 101) & 0xFF);
    }

    uint32_t bytes_written = 0;
    if (!win32_write_file(h, write_buf, sizeof(write_buf), &bytes_written) || bytes_written != sizeof(write_buf)) {
        LOGE("  FAIL: win32_write_file wrote %u bytes, expected %zu!", bytes_written, sizeof(write_buf));
        win32_close_handle(h);
        return false;
    }
    win32_close_handle(h);

    // Read back and compare
    uint32_t hr = win32_create_file(test_filename, GENERIC_READ, OPEN_EXISTING);
    if (hr == INVALID_HANDLE_VALUE) {
        LOGE("  FAIL: Failed to open file for read!");
        return false;
    }

    uint8_t read_buf[1024];
    memset(read_buf, 0, sizeof(read_buf));
    uint32_t bytes_read = 0;
    if (!win32_read_file(hr, read_buf, sizeof(read_buf), &bytes_read) || bytes_read != sizeof(read_buf)) {
        LOGE("  FAIL: win32_read_file read %u bytes, expected %zu!", bytes_read, sizeof(read_buf));
        win32_close_handle(hr);
        return false;
    }
    win32_close_handle(hr);

    if (memcmp(write_buf, read_buf, sizeof(write_buf)) != 0) {
        LOGE("  FAIL: Data read back does not match written buffer bit-for-bit!");
        return false;
    }

    LOGI("  Successfully wrote and read back 1024 bytes with 100%% byte parity.");
    LOGI("  PASS: Test 3 File Write & Read verified.");
    return true;
}

static bool test4_seeking_and_partial_reads(void) {
    LOGI("[TEST 4] Testing SetFilePointer Random Seeking...");

    const char *test_filename = "C:\\Cricket07\\test_io_rw.bin";
    uint32_t h = win32_create_file(test_filename, GENERIC_READ, OPEN_EXISTING);
    if (h == INVALID_HANDLE_VALUE) {
        LOGE("  FAIL: Failed to open file for seek testing!");
        return false;
    }

    // Seek to offset 256 from FILE_BEGIN
    int64_t pos = win32_set_file_pointer(h, 256, FILE_BEGIN);
    if (pos != 256) {
        LOGE("  FAIL: Seek FILE_BEGIN to 256 returned %lld!", (long long)pos);
        win32_close_handle(h);
        return false;
    }

    uint8_t sample[128];
    uint32_t got = 0;
    win32_read_file(h, sample, sizeof(sample), &got);
    for (int i = 0; i < 128; i++) {
        uint8_t expected = (uint8_t)(((256 + i) * 37 + 101) & 0xFF);
        if (sample[i] != expected) {
            LOGE("  FAIL: Byte mismatch at offset %d: expected 0x%02X, got 0x%02X", 256 + i, expected, sample[i]);
            win32_close_handle(h);
            return false;
        }
    }

    // Seek forward 64 bytes using FILE_CURRENT (current pos was 256 + 128 = 384, + 64 = 448)
    pos = win32_set_file_pointer(h, 64, FILE_CURRENT);
    if (pos != 448) {
        LOGE("  FAIL: Seek FILE_CURRENT to 448 returned %lld!", (long long)pos);
        win32_close_handle(h);
        return false;
    }

    // Seek backwards 128 bytes from FILE_END (1024 - 128 = 896)
    pos = win32_set_file_pointer(h, -128, FILE_END);
    if (pos != 896) {
        LOGE("  FAIL: Seek FILE_END -128 returned %lld, expected 896!", (long long)pos);
        win32_close_handle(h);
        return false;
    }

    win32_close_handle(h);
    LOGI("  PASS: Test 4 SetFilePointer Seeking verified.");
    return true;
}

static bool test5_file_size_and_handle_lifecycle(void) {
    LOGI("[TEST 5] Testing File Size and Handle Lifecycle...");

    const char *test_filename = "C:\\Cricket07\\test_io_rw.bin";
    uint32_t h = win32_create_file(test_filename, GENERIC_READ, OPEN_EXISTING);
    if (h == INVALID_HANDLE_VALUE) {
        LOGE("  FAIL: Failed to open file for size test!");
        return false;
    }

    uint32_t hi_size = 0;
    uint32_t lo_size = win32_get_file_size(h, &hi_size);
    if (lo_size != 1024 || hi_size != 0) {
        LOGE("  FAIL: win32_get_file_size reported %u:%u, expected 1024:0!", hi_size, lo_size);
        win32_close_handle(h);
        return false;
    }
    LOGI("  File size accurately reported: %u bytes (high: %u)", lo_size, hi_size);

    // Close handle and verify it cannot be used again
    win32_close_handle(h);
    uint8_t dummy[16];
    uint32_t dummy_read = 0;
    if (win32_read_file(h, dummy, sizeof(dummy), &dummy_read)) {
        LOGE("  FAIL: win32_read_file on closed handle succeeded unexpectedly!");
        return false;
    }

    LOGI("  Closed handle correctly rejected subsequent read operations.");
    LOGI("  PASS: Test 5 File Size & Handle Lifecycle verified.");
    return true;
}

static bool test6_high_res_qpc(void) {
    LOGI("[TEST 6] Testing High-Resolution Performance Counter (QPC)...");

    uint64_t freq = 0;
    if (!timer_query_performance_frequency(&freq) || freq == 0) {
        LOGE("  FAIL: timer_query_performance_frequency returned invalid freq %llu!", (unsigned long long)freq);
        return false;
    }
    LOGI("  Performance Frequency: %llu Hz", (unsigned long long)freq);

    uint64_t t0 = 0, t1 = 0;
    timer_query_performance_counter(&t0);
    // Sleep for 25 ms
    timer_sleep_ms(25);
    timer_query_performance_counter(&t1);

    if (t1 <= t0) {
        LOGE("  FAIL: Performance counter did not monotonically increase! t0=%llu, t1=%llu",
             (unsigned long long)t0, (unsigned long long)t1);
        return false;
    }

    uint64_t delta_ticks = t1 - t0;
    double elapsed_ms = ((double)delta_ticks / (double)freq) * 1000.0;
    LOGI("  Elapsed Time across 25ms sleep: %.2f ms (Delta ticks: %llu)", elapsed_ms, (unsigned long long)delta_ticks);

    if (elapsed_ms < 20.0 || elapsed_ms > 60.0) {
        LOGE("  FAIL: Measured elapsed time %.2f ms outside acceptable range [20ms, 60ms]!", elapsed_ms);
        return false;
    }

    LOGI("  PASS: Test 6 High-Res QPC verified.");
    return true;
}

static bool test7_winmm_multimedia_timers(void) {
    LOGI("[TEST 7] Testing WinMM Multimedia Timers (timeGetTime, GetTickCount)...");

    uint32_t t0 = timer_time_get_time();
    timer_sleep_ms(20);
    uint32_t t1 = timer_time_get_time();

    if (t1 < t0) {
        LOGE("  FAIL: timeGetTime did not advance monotonically! t0=%u, t1=%u", t0, t1);
        return false;
    }

    uint32_t dt = t1 - t0;
    LOGI("  timeGetTime delta: %u ms across 20ms sleep", dt);
    if (dt < 15 || dt > 55) {
        LOGE("  FAIL: timeGetTime delta %u ms outside expected range [15ms, 55ms]!", dt);
        return false;
    }

    uint64_t ft = timer_host_time_100ns();
    if (ft < 11644473600ULL * 10000000ULL) {
        LOGE("  FAIL: Windows file time %llu is before 1970!", (unsigned long long)ft);
        return false;
    }
    LOGI("  Windows FILETIME timestamp: %llu (100ns units since 1601)", (unsigned long long)ft);

    LOGI("  PASS: Test 7 WinMM Timers verified.");
    return true;
}

static bool test8_lazy_archive_views(void) {
    LOGI("[TEST 8] Testing EA Sports .big Archive Lazy Paging (64KB Chunk Paging)...");

    // Create a 192 KB test archive file (3 chunks of 64KB: Chunk 0, Chunk 1, Chunk 2)
    char archive_path[1024];
    snprintf(archive_path, sizeof(archive_path), "%s/test_archive.big", g_game_root);
    int fd = open(archive_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LOGE("  FAIL: Failed to create test archive file at %s!", archive_path);
        return false;
    }

    uint8_t chunk_data[VIEW_CHUNK];
    // Chunk 0: 0x11
    memset(chunk_data, 0x11, sizeof(chunk_data));
    write(fd, chunk_data, sizeof(chunk_data));
    // Chunk 1: 0x22
    memset(chunk_data, 0x22, sizeof(chunk_data));
    write(fd, chunk_data, sizeof(chunk_data));
    // Chunk 2: 0x33
    memset(chunk_data, 0x33, sizeof(chunk_data));
    write(fd, chunk_data, sizeof(chunk_data));
    fsync(fd);

    // Map view without eager reading (eager = 0)
    uint32_t total_size = 3 * VIEW_CHUNK;
    uint32_t guest_va = guest_view_map(fd, total_size, 0);
    if (!guest_va) {
        LOGE("  FAIL: guest_view_map returned NULL/0!");
        close(fd);
        unlink(archive_path);
        return false;
    }
    LOGI("  Mapped 192 KB archive view at guest VA 0x%08X", guest_va);

    // Verify all chunks are initially UNFILLED
    if (is_chunk_filled(0, 0) || is_chunk_filled(0, 1) || is_chunk_filled(0, 2)) {
        LOGE("  FAIL: Chunks were marked filled before any read request!");
        guest_view_unmap(guest_va);
        close(fd);
        unlink(archive_path);
        return false;
    }

    // Simulate memcpy read into Chunk 1 (offset 65536)
    LOGI("  Simulating game reading from Chunk 1 (Offset 0x%08X)...", guest_va + VIEW_CHUNK);
    view_fill(guest_va + VIEW_CHUNK, 128);

    // Verify Chunk 1 is now FILLED, but Chunk 0 and Chunk 2 remain UNFILLED
    if (!is_chunk_filled(0, 1)) {
        LOGE("  FAIL: Chunk 1 was not filled after view_fill!");
        guest_view_unmap(guest_va);
        close(fd);
        unlink(archive_path);
        return false;
    }
    if (is_chunk_filled(0, 0) || is_chunk_filled(0, 2)) {
        LOGE("  FAIL: Unrelated chunks were filled unexpectedly!");
        guest_view_unmap(guest_va);
        close(fd);
        unlink(archive_path);
        return false;
    }

    // Verify Chunk 1 payload in guest memory
    const uint8_t *mapped_ptr = (const uint8_t *)HP(guest_va + VIEW_CHUNK);
    if (mapped_ptr[0] != 0x22 || mapped_ptr[127] != 0x22) {
        LOGE("  FAIL: Chunk 1 payload mismatch: expected 0x22, got 0x%02X!", mapped_ptr[0]);
        guest_view_unmap(guest_va);
        close(fd);
        unlink(archive_path);
        return false;
    }
    LOGI("  Chunk 1 paged in on demand successfully (Value 0x22 verified).");

    // Now trigger Chunk 0
    view_fill(guest_va + 500, 64);
    if (!is_chunk_filled(0, 0)) {
        LOGE("  FAIL: Chunk 0 was not filled after view_fill!");
        guest_view_unmap(guest_va);
        close(fd);
        unlink(archive_path);
        return false;
    }
    const uint8_t *c0_ptr = (const uint8_t *)HP(guest_va);
    if (c0_ptr[500] != 0x11) {
        LOGE("  FAIL: Chunk 0 payload mismatch: expected 0x11, got 0x%02X!", c0_ptr[500]);
        guest_view_unmap(guest_va);
        close(fd);
        unlink(archive_path);
        return false;
    }
    LOGI("  Chunk 0 paged in on demand successfully (Value 0x11 verified).");

    // Clean up
    guest_view_unmap(guest_va);
    close(fd);
    unlink(archive_path);

    LOGI("  PASS: Test 8 Lazy Archive Views verified.");
    return true;
}

bool run_stage2_io_timer_tests(void) {
    LOGI("==================================================================");
    LOGI("PHASE C.2 — STAGE 2: FILE I/O AND TIMERS EMPIRICAL VERIFICATION");
    LOGI("Win32 File Emulation, Case-Insensitive Mapping & Demand Paging");
    LOGI("==================================================================");

    // Initialize subsystems
    game_file_io_init("/data/data/org.cricketlegacy/files/game", 
                      "/data/data/org.cricketlegacy/files/userdata");
    game_timers_init();
    game_lazy_views_init();

    int passed = 0;
    int total = 8;

    if (test1_path_normalization_and_ci()) passed++;
    if (test2_win32_file_creation()) passed++;
    if (test3_file_write_and_read()) passed++;
    if (test4_seeking_and_partial_reads()) passed++;
    if (test5_file_size_and_handle_lifecycle()) passed++;
    if (test6_high_res_qpc()) passed++;
    if (test7_winmm_multimedia_timers()) passed++;
    if (test8_lazy_archive_views()) passed++;

    LOGI("==================================================================");
    LOGI("STAGE 2 TEST SUMMARY: %d / %d TESTS PASSED (Failed: %d)", passed, total, total - passed);
    LOGI("==================================================================");

    return (passed == total);
}
