# CRICKET LEGACY — Phase B.6: Memory Correctness & Physical Verification Report
**Document ID:** `CL-B6-MEM-v1.0`  
**Classification:** Memory Architecture, Bounds Verification & Live Device Telemetry  
**Target:** Android 14.0 (API 34) Kernel Environment (`TrueCost_Test_AVD_API34`)  

---

## 1. Executive Summary & Audit Corrections

The user's Phase B.5 critique identified a critical mathematical flaw in Strategy 1:
```c
// Flawed Initial Code:
void* requested_low_addr = (void*)0x00010000;
size_t low_size = arena_size - 0x00010000;
void* ptr = mmap(requested_low_addr, low_size, ..., -1, 0);
arena->base_address = (void*)0x0;
```
If `base_address` is set to `(void*)0x0`, any dereference in the range `[0x0, 0x00010000)` hits the unmapped NULL guard page. This is proper for NULL-pointer protection.  
However, if an unconstrained fallback (Strategy 2) is used, simply treating addresses as raw pointers causes `SIGSEGV` because `base_address` is in high 64-bit space.

### The Corrected Dual-Strategy Implementation:
```c
// Corrected Phase B.6 Implementation:
void* requested_low_addr = (void*)0x00010000;
size_t low_size = (arena_size > 0x00010000) ? (arena_size - 0x00010000) : arena_size;

void* ptr = mmap(requested_low_addr, low_size, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);

if (ptr != MAP_FAILED && ptr == requested_low_addr) {
    arena->base_address = (void*)0x0; // Base is 0; raw 32-bit absolute addresses are directly valid!
    arena->reserved_size = arena_size;
    arena->mode = ARENA_MODE_FIXED_LOWER_4GB;
    arena->is_initialized = true;
    return true;
}
```
If `MAP_FIXED_NOREPLACE` fails (e.g. collision with Android dynamic linker or translation tables):
```c
ptr = mmap(NULL, arena_size, PROT_READ | PROT_WRITE,
           MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, -1, 0);
arena->base_address = ptr;
arena->mode = ARENA_MODE_BASE_OFFSET;
```
All dereferences then pass through the inlined translation macro:
```c
static inline void* cricket_translate_addr(const CricketVirtualArena* arena, uint32_t x86_addr) {
    uintptr_t offset = (uintptr_t)(x86_addr & 0xFFFFFFFFU);
    return (void*)((uint8_t*)arena->base_address + offset);
}
```

---

## 2. Live Android Device Verification Results

The automated test runner (`cricket_test_runner`) was compiled for both `x86_64` and `arm64-v8a`, pushed to `/data/local/tmp` on the running Android 14 AVD emulator (`TrueCost_Test_AVD_API34`), and executed live via `adb shell`.

### 2.1 Execution Run 1: x86_64 Native ABI
```
====================================================
CRICKET LEGACY — PHASE B.6 NATIVE TEST RUNNER
Architectural & Memory Correctness Verification Suite
====================================================

[TEST 1] Initializing 4GB Virtual Memory Arena...
  Arena Base Address: 0x0
  Arena Reserved Size: 4294967296 (0x100000000) bytes
  Active Mode: ARENA_MODE_FIXED_LOWER_4GB (Low 32-bit direct pointers valid)
  PASS: Arena initialization successful.
[TEST 2] Testing Boundary Address Accesses...
  Verified addr 0x00010000 -> 0xA1B3C3D4 (OK)
  Verified addr 0x00400000 -> 0xA1F2C3D4 (OK)
  Verified addr 0x00800000 -> 0xA132C3D4 (OK)
  Verified addr 0x10000000 -> 0xB1B2C3D4 (OK)
  Verified addr 0x7FFFF000 -> 0xDE4D33D4 (OK)
  Verified addr 0xFFFFFFF0 -> 0x5E4D3C24 (OK)
  PASS: Boundary address verification successful.
[TEST 3] Testing Stored 32-bit Pointers inside Game Memory...
  Object (0x01500000) -> VTable (0x00850000) -> Fn (0x00420000)
  PASS: Stored 32-bit pointer simulation successful.
[TEST 4] Testing Unaligned Memory Accesses...
  Unaligned addr 0x00500001 -> 0xFEEDFACE (OK)
  Unaligned addr 0x00500002 -> 0xFEEDFACF (OK)
  Unaligned addr 0x00500003 -> 0xFEEDFACC (OK)
  Unaligned addr 0x00500005 -> 0xFEEDFACD (OK)
  PASS: Unaligned memory accesses verified.
[TEST 5] Testing 32-Bit Address Wraparound Semantics...
  0xFFFFFFFC + 8 wraps to 0x00000004 -> Native Ptr: 0x4
  PASS: Address wraparound verified.
[TEST 6] Testing Resident Memory (RSS) Monitoring...
  Current Process Resident RAM: 33544 KB (32.76 MB)
  PASS: RSS profiling operational.
[TEST 7] Running ARM64 Ballistic Physics Validation Test...
  PASS: Physics calculation determinism verified.

====================================================
PHASE B.6 TEST RESULT: ALL 7 TESTS PASSED SUCCESSFULLY!
====================================================
```

### 2.2 Execution Run 2: ARM64 ABI via Android Translation Bridge
```
====================================================
CRICKET LEGACY — PHASE B.6 NATIVE TEST RUNNER
Architectural & Memory Correctness Verification Suite
====================================================

[TEST 1] Initializing 4GB Virtual Memory Arena...
  Arena Base Address: 0x7096d4b84000
  Arena Reserved Size: 4294967296 (0x100000000) bytes
  Active Mode: ARENA_MODE_BASE_OFFSET (Base-relative translation active)
  PASS: Arena initialization successful.
[TEST 2] Testing Boundary Address Accesses...
  Verified addr 0x00010000 -> 0xA1B3C3D4 (OK)
  Verified addr 0x00400000 -> 0xA1F2C3D4 (OK)
  Verified addr 0x00800000 -> 0xA132C3D4 (OK)
  Verified addr 0x10000000 -> 0xB1B2C3D4 (OK)
  Verified addr 0x7FFFF000 -> 0xDE4D33D4 (OK)
  Verified addr 0xFFFFFFF0 -> 0x5E4D3C24 (OK)
  PASS: Boundary address verification successful.
[TEST 3] Testing Stored 32-bit Pointers inside Game Memory...
  Object (0x01500000) -> VTable (0x00850000) -> Fn (0x00420000)
  PASS: Stored 32-bit pointer simulation successful.
[TEST 4] Testing Unaligned Memory Accesses...
  Unaligned addr 0x00500001 -> 0xFEEDFACE (OK)
  Unaligned addr 0x00500002 -> 0xFEEDFACF (OK)
  Unaligned addr 0x00500003 -> 0xFEEDFACC (OK)
  Unaligned addr 0x00500005 -> 0xFEEDFACD (OK)
  PASS: Unaligned memory accesses verified.
[TEST 5] Testing 32-Bit Address Wraparound Semantics...
  0xFFFFFFFC + 8 wraps to 0x00000004 -> Native Ptr: 0x7096d4b84004
  PASS: Address wraparound verified.
[TEST 6] Testing Resident Memory (RSS) Monitoring...
  Current Process Resident RAM: 51372 KB (50.17 MB)
  PASS: RSS profiling operational.
[TEST 7] Running ARM64 Ballistic Physics Validation Test...
  PASS: Physics calculation determinism verified.

====================================================
PHASE B.6 TEST RESULT: ALL 7 TESTS PASSED SUCCESSFULLY!
====================================================
```

---

## 3. Physical RAM vs. Virtual Allocation (`MAP_NORESERVE`)

The user correctly warned: *"`MAP_NORESERVE` defers physical backing; it does not guarantee the device can satisfy later page faults."*

### Empirical Verification:
- **Virtual Allocation:** The process requested $4,294,967,296\text{ bytes}$ ($4.0\text{ GB}$) of virtual address space.
- **Physical RSS Consumed:**
  - x86_64 process: **$32.76\text{ MB}$**
  - ARM64 process: **$50.17\text{ MB}$**
- **Conclusion:** The kernel did NOT allocate 4GB of physical DRAM. Only the pages written during Test 2 and Test 4 ($< 64\text{ KB}$) were backed by physical pages.
- **The Risk:** If the game attempts to write to $> 1.0\text{ GB}$ of pages during a match on a 4GB RAM phone, the Linux Out-Of-Memory (`oom_killer`) will terminate the process.
- **The Mandatory Mitigation:** We must preserve the **archive lazy loading** pattern from `host/core.c`, paging in 64KB asset chunks on-demand and reusing texture/audio buffers rather than decompressing entire match archives into the arena simultaneously.
