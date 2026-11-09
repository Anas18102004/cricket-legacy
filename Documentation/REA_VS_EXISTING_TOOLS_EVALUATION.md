# CRICKET LEGACY — REA Integration vs. Existing Tools Evaluation
**Document ID:** `CL-REA-EVAL-v1.0`  
**Status:** Deferred / Evaluation Completed  
**Objective:** Compare `morluto/rea` against existing developer tools (Ghidra, translated C source, differential tests, NDK debuggers) to establish whether an MCP integration provides clear technical advantages.

---

## 1. Existing Tooling Baseline

Our development environment and the `cricket07-browser` / `retro-port` toolchain already possess high-powered analysis capabilities:

1. **The Translated C Source Code:**
   - 31,548 fully translated functions (~1.3M lines of C).
   - Function signatures, register allocations, logic branches, and call graphs are already expressed in plain C text.
   - Text search tools (`grep`, `ripgrep`, IDE AST navigation) can immediately locate any variable, string literal, or function reference without decompiling binary machine code.
2. **Ghidra (NSA Reverse Engineering Suite):**
   - Disassembles the original 32-bit Windows PE executable (`Cricket07.exe`).
   - Generates control flow graphs (CFGs), type recovery, structure definitions, and cross-references.
   - Operates headless for batch analysis or via GUI for deep manual disassembly.
3. **Existing Differential Test Suite (`tests/` in `cricket07-browser`):**
   - Compares translated C function execution against original x86 instruction execution.
   - Validates mathematical determinism, CPU flags, and register states.
4. **Android NDK Toolchain & Profiling Tools:**
   - Clang compiler warnings and static analyzer.
   - AddressSanitizer (`ASan`), UndefinedBehaviorSanitizer (`UBSan`) for memory corruption detection.
   - `lldb` for native interactive debugging and Android GPU Inspector (AGI) / Simpleperf for device profiling.

---

## 2. What REA (`morluto/rea`) Would Provide

| Analysis Category | Existing Tooling Capability | REA Capability | Meaningful Technical Advantage? |
| :--- | :--- | :--- | :---: |
| **C Source Inspection** | Direct text search / clang AST | AST inspection via MCP | **No.** Plain C source is already directly accessible. |
| **x86 Binary Disassembly** | Ghidra GUI & Headless | Wraps Ghidra / Hopper via MCP | **Marginal.** Only automates feeding decompiled snippets to LLM context. |
| **Wasm Bytecode Audit** | `wasm-tools` / Chrome DevTools | MCP Wasm disassembler | **Low.** Native C++ path bypasses WebAssembly entirely. |
| **Android APK Inspection** | `apkanalyzer`, `unzip`, `aapt2` | APK manifest & DEX inspection via MCP | **Low.** Standard Android SDK tools already installed locally. |
| **Shader Verification** | `wgpu` compiler / Tint validator | Static WGSL parsing | **Low.** Compile-time shader errors are caught by `wgpu-native`. |

---

## 3. Engineering Recommendation

**Decision: DEFER REA MCP INTEGRATION.**  
REA is an impressive automation layer for reverse engineering unknown black-box binaries from scratch. However, in our project:
- We already have the primary prize: the **31,548 translated C functions**.
- Adding an MCP server introduces unnecessary operational overhead and third-party dependencies at this stage.
- Existing tools (Clang, NDK, AddressSanitizer, and direct C inspection) provide 100% of the fidelity required for the Android native port.

We will revisit REA only if we encounter an encrypted data format or obfuscated subsystem in the original PC assets that requires automated symbol graph generation.
