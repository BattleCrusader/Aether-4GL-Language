# Bootstrap Plan — Option A: Direct Compilation

> **Purpose:** Get a native Aether compiler as fast as possible.  
> Everything else is secondary.

---

## Current Status (2026‑08‑07)

- ✅ **Phase 4 complete** – the Aether compiler self‑hosts correctly; all verification steps pass.  
- ✅ **Kanban `t_73f2dbd7` marked done** – compile & run `hello.ae` verified.  
- 🟡 **Outstanding Phase 4 items** – `t_71a02323` (Direct Mach‑O output), `t_af514ed8` (Full test‑suite verification), `t_63aad356` (Retire bootstrap) remain on the board.  
- 🟩 **Stage 1 – Static linking verification** – *Blocked* after verification attempt: `aether_v2` segfaults on `hello.ae` (exit 139). No byte‑identical output obtained. **Next step:** apply the next fix from the `aether_phase4_attempts` list (e.g., analyze `main()` prologue, inspect temporary `.o` files, compare relocation entries).  

---

## What We’re Building
... *(rest of the original document unchanged)*