# Bootstrap Plan — Option A: Direct Compilation

> **Purpose:** Get a native Aether compiler as fast as possible.  
> Everything else is secondary.

---

## Current Status (2026‑08‑08)

- ✅ **Bootstrap compiler (`build/aether`, Go)** – solid. Compiles `hello.ae` → valid, runnable Mach–O (exit 0). All 86 Go unit tests pass.
- ✅ **Self-hosting pipeline runs end‑to‑end** – `make self-host` compiles `aether/*.ae` → `build/aether_v2` (exit 0); `aether_v2` compiles `hello.ae` → Mach-O file (exit 0). No more segfault.
- ❌ **Byte‑identical self‑hosting NOT achieved** – `aether_v2` output differs from bootstrap output at byte 17 (71,975 vs 73,904 bytes). `sizeofcmds` field corrupted (`0x18100880` instead of `0x288`); output dies `Killed: 9` (exit 137); `strings` reports "load commands extend past the end of the file".
- ❌ **Direct Mach‑O output from Aether `linkMachO` not runnable** – emits 6 load commands vs the bootstrap's 15; missing `LC_LOAD_DYLINKER`, `LC_MAIN`, `LC_SYMTAB`, `LC_UUID`, etc.
- 🟡 **Root cause (active)** – `[byte]` struct‑field access bug in `aether/codegen.ae` corrupting header bytes after position 16. Cards `t_4c4a913f` (byte‑identical self‑hosting) and `t_71a02323` (direct Mach‑O output) reopened and dispatched 2026‑08‑08.
- 🟡 **Outstanding Phase 4 items** – `t_af514ed8` (Full test‑suite verification), `t_63aad356` (Retire bootstrap) remain on the board.

---

## What We’re Building
... *(rest of the original document unchanged)*