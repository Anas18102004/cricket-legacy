# CRICKET LEGACY — Phase D Step D.3: Asset Availability and Resource Audit
**Document ID:** `CL-D3-AUDIT-v1.0`  
**Classification:** Resource Call-Path Mapping, Archive Architecture & File Host Readiness  
**Target Platform:** Android 14+ (ARM64-v8a & x86_64)  
**Status:** BLOCKED — GAME ASSETS ABSENT (Scaffolding: VERIFIED; Assets: MISSING)  

---

## 1. Executive Summary

Step D.3 audits the game resources required to boot EA SPORTS Cricket 07 and verifies whether the existing Android host file subsystem (`game_file_io.c` and `game_lazy_views.c` implemented in Phase C.2 Stage 2) meets the resource access contract expected by the engine.

### Key Audit Finding
While **zero authentic EA assets exist locally in the repository**, the reverse-engineered asset access protocols and archive architectures documented in [`tools/ea_assets.py`](file:///d:/cricket07-browser-src/tools/ea_assets.py) and [`host/core.c`](file:///d:/cricket07-browser-src/host/core.c) confirm that the **Android host lazy-view demand-paging engine developed in Stage 2 is architecturally aligned** with the game's actual I/O pipeline.

---

## 2. Authentic Asset Archive Architecture

Inspection of upstream tools reveals that EA SPORTS Cricket 07 uses proprietary mid-2000s EA Sports packaging formats:

```
                            EA SPORTS Cricket 07 Asset Ecosystem
┌─────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                       data.gob (~325 MB)                                        │
│  ┌───────────────────────────────────────────────────────────────────────────────────────────┐  │
│  │ BIG Archive Header ('BIGF' or 'BIG4' magic, 16-byte header, Big-Endian Entry Index)       │  │
│  ├───────────────────────────────────────────────────────────────────────────────────────────┤  │
│  │ RefPack Compressed Entries (Magic: byte0 & 0x3E == 0x10, byte1 == 0xFB)                  │  │
│  │ • fe.big       -> Front-end UI, menus, fonts, button textures                             │  │
│  │ • audio.big    -> Ball impacts, umpire calls, crowd reaction samples                      │  │
│  │ • stadiums.big -> 3D pitch meshes, stadium models, turf textures                          │  │
│  │ • teams.big    -> Rosters, player attributes, tournament definitions                      │  │
│  │ • motion.big   -> 64 MB animation archive (demand-mapped at match start)                  │  │
│  │ • *.fsh        -> EA SHPI texture containers (DXT1, DXT3, 16-bit 565, 32-bit ARGB)        │  │
│  └───────────────────────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.1 File Name Hashing Convention
In EA BIG/GOB archives, internal file entries are identified not by raw directory paths, but by deterministic MD5 hashes computed by the game engine:
$$\text{EntryName} = \text{MD5}(\text{lowercase\_stem}) + \text{'.' + ext}$$
For example, a path `assets\textures\pitch.fsh` becomes `assets/textures/pitch` $\to$ `md5("assets/textures/pitch") + ".fsh"`.

### 2.2 RefPack Decompression Protocol
- Individual files inside BIG archives are compressed with RefPack (LZ77 variant).
- Header byte 0: flags (`flags & 0x01` indicates explicit compressed length; `flags & 0x80` denotes 4-byte uncompressed size).
- Header byte 1: magic byte `0xFB`.
- Decompression emits literal bytes followed by back-references with variable copy lengths and distance offsets.

### 2.3 FSH (SHPI) Texture Format
- FSH containers begin with magic `SHPI`.
- Each texture entry specifies width, height, format code (DXT1 = `0x60`, DXT3 = `0x61`, 16-bit ARGB1555 = `0x7C`, 32-bit ARGB = `0x7D`), and mipmap levels.

---

## 3. Host Android Subsystem Readiness Analysis

### 3.1 What Was Built in Phase C.2 Stage 2
1. **Case-Insensitive Path Resolution (`map_path`):**
   - Android's native Linux filesystem (`ext4`/`f2fs`) is strictly case-sensitive.
   - `game_file_io.c` implements `map_path()` with recursive directory scanning, matching Windows file resolution behavior regardless of case (`DATA.GOB` vs `data.gob`).
2. **Virtual Memory Handle Manager (`w_CreateFileA`, `w_ReadFile`, etc.):**
   - 1,024-slot file handle table supporting 64-bit seek offsets and asynchronous read/write bridging.
3. **Lazy Demand-Paging Subsystem (`game_lazy_views.c`):**
   - Implements 64KB chunk demand-paging (`VIEW_CHUNK = 0x10000`).
   - Hooks EA's custom memcpy routine at `0x00652A00` (`hook_copy_source`).
   - Transparently populates 64KB page clusters from disk into the guest virtual arena only when touched by the engine.

### 3.2 Compatibility Gap Analysis

| Subsystem Component | Host Status | Real Asset Gap |
| :--- | :---: | :--- |
| **Path Mapping (`map_path`)** | **READY** | Verified with synthetic filenames; ready for real paths. |
| **Handle Management** | **READY** | 1,024 handles verified; ready for concurrent archives. |
| **Lazy Demand-Paging** | **READY** | 64KB chunk bitmap verified; hook hooked at `0x00652A00`. |
| **RefPack Decompression** | **UNVERIFIED** | Decompressor lives inside translated x86 code; host has not exercised real RefPack blocks. |
| **FSH Texture Decoding** | **UNVERIFIED** | Texture parser lives inside translated x86 code; host D3D8 adapter expects uploaded textures. |
| **Audio Stream Decoding** | **UNVERIFIED** | DirectSound bridge active, but EA audio streaming buffers not connected to real audio streams. |

---

## 4. Audit Verdict & Gate Status

```text
Asset Inventory Check:
├── data.gob       -> MISSING [CRITICAL BLOCKER]
├── fe.big         -> MISSING [CRITICAL BLOCKER]
├── audio.big      -> MISSING [HIGH BLOCKER]
├── motion.big     -> MISSING [HIGH BLOCKER]
└── userdata/      -> NOT CREATED (Created automatically upon first run)
```

### Status: BLOCKED
The host Android file I/O and lazy view scaffolding is verified and ready. However, authentic execution cannot proceed without the legitimate EA game asset archives. Under Non-Negotiable Rule 7, no EA assets will be downloaded or fabricated.
