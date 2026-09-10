// Title-specific native overrides.
//
// TEST (reversible, see docs/re/status.md `Stall mechanism`): bank-finalize
// teardown (`822F33B8` -> `82CA9638` destructor table) destroys the streaming
// lists at `0x834A5C88`/`0x834A601C`/`0x8331AFA4` while the Texture Streamer and 3D Engine
// workers poll them unparked (park path `822EA928` never runs in this boot),
// deadlocking boot. These no-ops keep the lists valid so workers poll live
// state. Node release via `8221BE68` is skipped too (leak: a few dozen bytes
// per boot, no reuse). REVERT if boot does not advance past the stall.
#include <rex/ppc/func.h>

extern "C" void sub_823FB0E8(PPCContext& ctx, uint8_t* base) {
  (void)ctx;
  (void)base;
}

extern "C" void sub_82B6BEB0(PPCContext& ctx, uint8_t* base) {
  (void)ctx;
}

extern "C" void sub_832AF210(PPCContext& ctx, uint8_t* base) {
  (void)ctx;
  (void)base;
}


// TEST (reversible): presence probes. Each logs first call, then passes
// through to the original body untouched (zero-arg import preserves all
#include <execinfo.h>
#include <unistd.h>
// guest registers). Determines which boot phases execute at all.
#include <atomic>
#include <cstdio>
#include <mutex>
#include <rex/hook.h>
#include "fable_ii_pch.h"
// PERF (2026-09-10): silence every diagnostic probe in this TU. All
// fprintf here is logging; SDK uses spdlog. Keeps: resume-jump logic,
// thunk chains (no logging in them). One line, fully reversible.
#define PROBE_LOG(...) ((void)0)
// 2026-09-10: plain mutex self-deadlocks on 82BCA340 -> 82BC9E10 reentrancy
// (Permanent Bank stuck, GDB-proven) — BUT any reentrant-capable form
// (none, recursive) lets Permanent Bank complete a path after which the 3D
// thread fault-storms 16-27k times on [0x14] at 821E27C8 (ReXGlue resumes
// unhandled faults at the same PC: infinite signal loop, log-destroying).
// Plain mutex = known 0-fault baseline every A/B datum was gathered on.
// The reentrant bank path + 3D [0x14] read is open follow-up work; do NOT
// "fix" by removing this mutex without solving that first.
static std::recursive_mutex bank_table_mutex;
// Phase gate: set when GameThread's drain returns; 3D's takes wait on it.
std::atomic<bool> gt_drain_done{false};
// Set when DRAINVIRT fires (defined near `822F27C0`); `829FF648` logs
// caller LRs for populate-phase calls.
extern bool g_pop_active;
REX_IMPORT(__imp__sub_8236C940, probe_orig_8236C940, void());
extern "C" void sub_8236C940(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 8236C940 populate-caller\n");
  }
  probe_orig_8236C940(ctx, base);
}
REX_IMPORT(__imp__sub_82A47D48, probe_orig_82A47D48, void());
extern "C" void sub_82A47D48(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 82A47D48 G4-populate\n");
  }
  probe_orig_82A47D48(ctx, base);
  static bool done = false;
  if (!done) {
    done = true;
    PROBE_LOG(stderr, "PROBE-HIT 82A47D48-EXIT returned\n");
  }
}
REX_IMPORT(__imp__sub_82AA8AD0, probe_o_82AA8AD0, void());
extern "C" void sub_82AA8AD0(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 82AA8AD0 G4-sub\n");
  }
  probe_o_82AA8AD0(ctx, base);
}
REX_IMPORT(__imp__sub_82CBB620, probe_o_82CBB620, void());
extern "C" void sub_82CBB620(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 82CBB620 G4-sub2\n");
  }
  probe_o_82CBB620(ctx, base);
}
REX_IMPORT(__imp__sub_822EA928, probe_orig_822EA928, void());
extern "C" void sub_822EA928(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 822EA928 park-sequencer\n");
  }
  probe_orig_822EA928(ctx, base);
}
REX_IMPORT(__imp__sub_822F4690, probe_orig_822F4690, void());
extern "C" void sub_822F4690(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 822F4690 quit-setter\n");
  }
  probe_orig_822F4690(ctx, base);
}
#include <rex/system/xthread.h>
REX_IMPORT(__imp__sub_82200688, probe_orig_82200688, void());
extern "C" void sub_82200688(PPCContext& ctx, uint8_t* base) {
  static unsigned npair = 0;
  if (npair < 40) {
    uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
    static uint32_t seen[40] = {0};
    bool known = false;
    for (unsigned i = 0; i < npair; ++i) {
      if (seen[i] == gid) {
        known = true;
        break;
      }
    }
    if (!known) {
      seen[npair++] = gid;
      PROBE_LOG(stderr, "THREADMAP gid=%08X lwp=%d\n", gid, gettid());
    }
  }
  // REMOVED 2026-09-09: the 3D gate (blocked lr==0x8236C588 until
  // drain done) deadlocked populate — GameThread's `8236CC90` needs a
  // lock 3D holds while 3D sat in GATE-WAIT; every run burned the
  // 120 s timeout (GATE-TIMEOUT observed). Hypothesis falsified.
  // GameThread takes the lock normally (no skip); 3D ungated.
  uint32_t lr = (uint32_t)ctx.lr;
  {
    uint32_t lk = ctx.r4.u32;
    int32_t lc = -1;
    uint32_t ow = 0;
    if (lk >= 0x10000) {
      std::memcpy(&lc, base + lk + 0x10, 4);
      uint32_t beow = 0;
      std::memcpy(&beow, base + lk + 0x18, 4);
      ow = __builtin_bswap32(beow);
    }
    if (lc != -1) {
      static uint32_t klr = 0, klk = 0;
      static int32_t klc = 0;
      static uint32_t kow = 0;
      if (lr != klr || lk != klk || lc != klc || ow != kow) {
        klr = lr;
        klk = lk;
        klc = lc;
        kow = ow;
        PROBE_LOG(stderr, "LOCKW lr=%08X lock=%08X count=%d owner=%08X\n",
                     lr, lk, lc, ow);
      }
    }
  }
  if (lr == 0x822F3664 || lr == 0x82378834) {
    static unsigned ngt2 = 0;
    if (ngt2 < 8) {
      ++ngt2;
      uint32_t lk = ctx.r4.u32;
      int32_t lc = 0;
      uint32_t ow = 0;
      if (lk >= 0x10000) {
        std::memcpy(&lc, base + lk + 0x10, 4);
        uint32_t beow = 0;
        std::memcpy(&beow, base + lk + 0x18, 4);
        ow = __builtin_bswap32(beow);
      }
      PROBE_LOG(stderr, "GT2-ENTER lr=%08X lock=%08X count=%d owner=%08X\n",
                   lr, lk, lc, ow);
    }
  }
  probe_orig_82200688(ctx, base);
  if (lr == 0x822F3664 || lr == 0x82378834) {
    static unsigned ngt2x = 0;
    if (ngt2x < 8) {
      ++ngt2x;
      PROBE_LOG(stderr, "GT2-EXIT lr=%08X\n", lr);
    }
  }
}
// thread after the heap works). The real producer (`82A47D48`) never runs
// before first use in this boot, and the 3D worker faults writing through
// the null pointer, holding its lock and wedging GameThread's park sequence.
// A zeroed object takes the consumer's own null-tolerant branch
// (`[r3+4]==0` -> `82BE4D50` path); the real producer overwrites the slot
// unconditionally once unblocked, so this self-heals. Full guest context
// snapshot/restore: zero behavioral delta besides heap + the slot.
#include <cstring>
REX_IMPORT(__imp__sub_8221F388, probe_alloc32, uint32_t(uint32_t));
REX_IMPORT(__imp__sub_82CC1990, probe_orig_82CC1990, void());
extern "C" void sub_82CC1990(PPCContext& ctx, uint8_t* base) {
  probe_orig_82CC1990(ctx, base);
  PPCContext saved = ctx;
  uint32_t node = probe_alloc32(ctx, base, 32);
  ctx = saved;
  if (node != 0) {
    std::memset(base + node, 0, 32);
    uint32_t be = __builtin_bswap32(node);
    std::memcpy(base + 0x8349F554, &be, 4);
    PROBE_LOG(stderr, "PRESEED-G4 node=%08X\n", node);
  }
  PPCContext saved2 = ctx;
  uint32_t node2 = probe_alloc32(ctx, base, 32);
  ctx = saved2;
  if (node2 != 0) {
    // Self-linked like the game's own inits (`[n+0]=[n+4]=[n+8]=n`): the
    // 3D consumer (`82191FD8`) exits cleanly when cursor meets head through
    // a self link (`82192010` -> `821920A8` return). Real producer
    // overwrites unconditionally once unblocked.
    uint32_t be2 = __builtin_bswap32(node2);
    std::memcpy(base + node2 + 0, &be2, 4);
    std::memcpy(base + node2 + 4, &be2, 4);
    std::memcpy(base + node2 + 8, &be2, 4);
    std::memcpy(base + 0x8349F7CC, &be2, 4);
    PROBE_LOG(stderr, "PRESEED-7CC node=%08X\n", node2);
  }

  PPCContext saved3 = ctx;
  uint32_t node3 = probe_alloc32(ctx, base, 32);
  ctx = saved3;
  if (node3 != 0) {
    // Self-linked like the game's own inits: the 3D walker (`822C5898`)
    // exits when cursor meets start (`r31==r26`). Real producer overwrites.
    uint32_t be3 = __builtin_bswap32(node3);
    std::memcpy(base + node3 + 0, &be3, 4);
    std::memcpy(base + node3 + 4, &be3, 4);
    std::memcpy(base + node3 + 8, &be3, 4);
    std::memcpy(base + 0x8349F7B4, &be3, 4);
    PROBE_LOG(stderr, "PRESEED-7B4 node=%08X\n", node3);
  }
}

// TEST (reversible): no-op the whole destructor-table runner. Per-entry
// whack-a-mole does not converge (`823FB0E8`, `82B6BEB0`, `832AF210`,
// `832AC2F8`, `82A46DF8`, now `832AC260`, ... all via `82CA9638`); every
// observed entry destroys live streaming state while workers poll unparked.
// First invocation runs empty anyway, so the runner tolerates no-ops.
// Revert if constructive entries prove necessary.
extern "C" void sub_82CA9638(PPCContext& ctx, uint8_t* base) {
  (void)ctx;
  (void)base;
}

// TEST (reversible): same bank-finalize teardown family via the `82CA9638`
// table keeps nulling preseeded slots: `832AC2F8` zeroes `[0x8349F7CC]`,
// `82A46DF8` zeroes `[0x8349F554]` (watchpoint-attributed). No-op both.
extern "C" void sub_832AC2F8(PPCContext& ctx, uint8_t* base) {
  (void)ctx;
  (void)base;
}

extern "C" void sub_82A46DF8(PPCContext& ctx, uint8_t* base) {
  (void)ctx;
  (void)base;
}

// TEST (reversible): `8227BB58` indirect-calls `[r31+12]` with
// `r31 = [r28+16]`; when the input object itself is null the chain faults
// (or FATALs via zero page). Run the original whenever `r28` is present;
// otherwise return a private zeroed scratch block so the caller's fill loop
// has mapped memory. Revisit when the producer is understood.
REX_IMPORT(__imp__sub_8227BB58, probe_orig_8227BB58, void());
static uint32_t hook_scratch = 0;
extern "C" void sub_8227BB58(PPCContext& ctx, uint8_t* base) {
  static int n = 0;
  uint32_t r28 = ctx.r3.u32;
  uint32_t r31 = 0;
  uint32_t tgt = 0;
  if (r28 >= 0x10000) {
    uint32_t o16;
    std::memcpy(&o16, base + r28 + 16, 4);
    r31 = __builtin_bswap32(o16);
  }
  if (r31 >= 0x10000) {
    uint32_t o12;
    std::memcpy(&o12, base + r31 + 12, 4);
    tgt = __builtin_bswap32(o12);
  }
  uint32_t lr = (uint32_t)ctx.lr;
  static uint32_t last_lr = 0;
  if (lr != last_lr) {
    last_lr = lr;
    PROBE_LOG(stderr, "PROBE-7BB58-CALLER lr=%08X r28=%08X tgt=%08X\n", lr,
                 r28, tgt);
  }
  if (n < 12) {
    PROBE_LOG(stderr, "PROBE-7BB58 r28=%08X r31=%08X tgt=%08X\n", r28, r31,
                 tgt);
    ++n;
  }
  if (r28 != 0) {
    probe_orig_8227BB58(ctx, base);
    if (ctx.r3.u32 == 0) {
      // NULL item returned (pool empty). Pass it through untouched and
      // observe: callers with null checks take their empty paths; the
      // scratch substitution is removed (it fed infinite fake-pending
      // items into the drain). Faults, if any, implicate the mutex.
      static bool logged = false;
      if (!logged) {
        logged = true;
        PROBE_LOG(stderr, "PASS-NULL 8227BB58\n");
      }
    }
    return;
  }
  if (hook_scratch == 0) {
    PPCContext saved = ctx;
    hook_scratch = probe_alloc32(ctx, base, 64);
    ctx = saved;
  }
  if (hook_scratch != 0) {
    std::memset(base + hook_scratch, 0, 64);
  }
  ctx.r3.u32 = hook_scratch;
}

// TEST (reversible): liveness probes (first-hit log + passthrough).
REX_IMPORT(__imp__sub_822F2608, probe_o_822F2608, void());
extern "C" void sub_822F2608(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  PROBE_LOG(stderr, "PROBE-HIT 822F2608 populate-dispatch #%u lr=%08X\n",
               ++n, (uint32_t)ctx.lr);
  probe_o_822F2608(ctx, base);
}
// TEST: trace the sequencer branch containing `822EAA8C` (populate
// caller): entry LR identifies who drives populate; exit shows the
REX_IMPORT(__imp__sub_822EA8C0, probe_o_822EA8C0, void());
extern "C" void sub_822EA8C0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  if (++n <= 4) {
    PROBE_LOG(stderr, "PROBE-HIT 822EA8C0-ENTER #%u lr=%08X r3=%08X\n",
                 n, (uint32_t)ctx.lr, ctx.r3.u32);
  }
  probe_o_822EA8C0(ctx, base);
  static unsigned nx = 0;
  if (++nx <= 4) {
    PROBE_LOG(stderr, "PROBE-HIT 822EA8C0-EXIT #%u\n", nx);
  }
}
REX_IMPORT(__imp__sub_82378BB8, probe_o_82378BB8, void());
extern "C" void sub_82378BB8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 82378BB8 populate-branch\n");
  }
  probe_o_82378BB8(ctx, base);
}
static uint32_t drain_flag_addr = 0;
static uint32_t drain_item_addr = 0;
uint32_t gate_byte_addr = 0;
REX_IMPORT(__imp__sub_82B67950, probe_o_82B67950, void());
extern "C" void sub_82B67950(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 82B67950 streamer-poll\n");
  }
  if (drain_flag_addr >= 0x10000) {
    static uint8_t last = 0xFF;
    static unsigned long n = 0;
    static long long t0 = 0;
    uint8_t cur = *(base + drain_flag_addr);
    if (t0 == 0) {
      t0 = (long long)time(nullptr);
    }
    if (++n == 1 || cur != last) {
      last = cur;
      PROBE_LOG(stderr, "FLAGWATCH %02X\n", cur);
    }
    // Drain-wait exit gate byte ([gateobj+44], zero-tested by 82185418):
    // change-logged; the drain exits only when this reaches 0.
    if (gate_byte_addr >= 0x10000) {
      static uint8_t glast = 0xFF;
      static unsigned long gn = 0;
      uint8_t gcur = *(base + gate_byte_addr);
      if (++gn == 1 || gcur != glast) {
        glast = gcur;
        PROBE_LOG(stderr, "GATEWATCH %02X\n", gcur);
      }
    }
    // REPEATING SYNTH OFF 2026-09-10: trace-only mode. Forcing completion
    // manufactured execution orders; synths stay available only as explicit
    // timeout-triggered probes. Watchers above remain active.
#if 0
    // TEST (reversible): synthesize missing drain completions, REPEATING.
    // The drain processes items sequentially; each can park on (a) the
    // 01-flag, (b) the gate byte, (c) the spin byte, with no producer
    // owning them in our boot. One-shots covered only the first item, so
    // each synth re-fires on cooldown while its condition persists.
    static long long lastflag = 0;
    if (cur != 0 && (long long)time(nullptr) - t0 > 30 &&
        (long long)time(nullptr) - lastflag > 30) {
      lastflag = (long long)time(nullptr);
      *(base + drain_flag_addr) = 0;
      if (drain_item_addr >= 0x10000) {
        *(base + drain_item_addr + 0x88) = 0;
      }
      PROBE_LOG(stderr, "FLAGSYNTH cleared %08X item=%08X\n",
                   drain_flag_addr, drain_item_addr);
    }
    static long long lastgate = 0;
    if (lastflag != 0 && gate_byte_addr >= 0x10000 &&
        *(base + gate_byte_addr) == 0 &&
        (long long)time(nullptr) - lastgate > 30) {
      lastgate = (long long)time(nullptr);
      *(base + gate_byte_addr) = 1;
      PROBE_LOG(stderr, "GATESYNTH set %08X\n", gate_byte_addr);
    }
#endif
  }
  probe_o_82B67950(ctx, base);
}
REX_IMPORT(__imp__sub_8236C360, probe_o_8236C360, void());
extern "C" void sub_8236C360(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 8236C360 3d-proc\n");
  }
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  if (gid == 0x3009C018) {
    PROBE_LOG(stderr, "3DPROC-WORKER lr=%08X r3=%08X\n", (uint32_t)ctx.lr,
                 ctx.r3.u32);
  }
  probe_o_8236C360(ctx, base);
  if (gid == 0x3009C018) {
    PROBE_LOG(stderr, "3DPROC-WORKER-EXIT ret=%08X\n", ctx.r3.u32);
  }
}
REX_IMPORT(__imp__sub_822F33B8, probe_o_822F33B8, void());
extern "C" void sub_822F33B8(PPCContext& ctx, uint8_t* base) {
  static int n = 0;
  int mine = ++n;
  PROBE_LOG(stderr, "PROBE-HIT 822F33B8 bank-proc #%d lr=%08X\n", mine,
               (uint32_t)ctx.lr);
  probe_o_822F33B8(ctx, base);
  // If the bank job ever returns, its return + the spin byte reveal whether
  // the +5 store ran. No EXIT line => worker parked inside (see status).
  uint8_t b5 = 0xEE;
  uint32_t dc = 0;
  if (drain_item_addr >= 0x10000) {
    uint32_t b = 0;
    std::memcpy(&b, base + drain_item_addr + 0x5C, 4);
    dc = __builtin_bswap32(b);
    if (dc >= 0x10000 && dc < 0x84000000) {
      b5 = *(base + dc + 5);
    }
  }
  PROBE_LOG(stderr, "BANKPROC-EXIT #%d ret=%08X b5=%02X\n", mine,
               ctx.r3.u32, b5);
}

REX_IMPORT(__imp__sub_822F47F8, probe_o_822F47F8, void());
extern "C" void sub_822F47F8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    uint32_t c = ctx.r3.u32, i0 = 0, vt = 0, t12 = 0;
    if (c >= 0x10000) {
      uint32_t b = 0;
      std::memcpy(&b, base + c, 4);
      i0 = __builtin_bswap32(b);
      if (i0 >= 0x10000) {
        std::memcpy(&b, base + i0, 4);
        vt = __builtin_bswap32(b);
        if (vt >= 0x10000) {
          std::memcpy(&b, base + vt + 12, 4);
          t12 = __builtin_bswap32(b);
        }
      }
    }
    PROBE_LOG(stderr, "DRAIN-ITEM ctx=%08X item=%08X vt=%08X t12=%08X\n", c,
                 i0, vt, t12);
  }
  PROBE_LOG(stderr, "PROBE-47F8-ENTER\n");
  probe_o_822F47F8(ctx, base);
  gt_drain_done.store(true);
  PROBE_LOG(stderr, "PROBE-47F8-EXIT\n");
}
REX_IMPORT(__imp__sub_823781A8, probe_o_823781A8, void());
extern "C" void sub_823781A8(PPCContext& ctx, uint8_t* base) {
  PROBE_LOG(stderr, "PROBE-781A8-ENTER\n");
  probe_o_823781A8(ctx, base);
  PROBE_LOG(stderr, "PROBE-781A8-EXIT\n");
}

REX_IMPORT(__imp__sub_822F5540, probe_o_822F5540, void());
extern "C" void sub_822F5540(PPCContext& ctx, uint8_t* base) {
  PROBE_LOG(stderr, "PROBE-5540-ENTER\n");
  probe_o_822F5540(ctx, base);
  PROBE_LOG(stderr, "PROBE-5540-EXIT\n");
}

REX_IMPORT(__imp__sub_82CBC6B0, probe_o_82CBC6B0, void());
extern "C" void sub_82CBC6B0(PPCContext& ctx, uint8_t* base) {
  // TEST (reversible): `82CBC6B0` tail-calls the flag-wait `82CC2028`
  // with r4=0 (null object); direct tail-call edges bypass the hooked
  // `82CC2028`, so skip here instead — but ONLY GameThread's drain edge
  // (lr `0x82378828`): a null object means "nothing to wait for".
  // Other callers pass through untouched.
  uint32_t lr = (uint32_t)ctx.lr;
  static unsigned n = 0;
  if (n < 10) {
    ++n;
    PROBE_LOG(stderr, "SLEEPBYPASS arg=%u lr=%08X\n", ctx.r3.u32, lr);
  }
  // REVERTED 2026-09-10: the null-object path natively performs one 100 ms
  // ALERTABLE KeDelayExecutionThread then returns (r30 comes from the
  // register, not [0]; the old comment rationale was wrong). Skipping it
  // removes the spin loop's only kernel wait — and with it any APC/DPC
  // delivery point on GameThread. Restore native pacing; if +5 still never
  // sets, the APC theory is dead.
#if 0
  if (lr == 0x82378828) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      PROBE_LOG(stderr, "SKIP-GTSLEEP\n");
    }
    ctx.r3.u32 = 0;
    return;
  }
#endif
  probe_o_82CBC6B0(ctx, base);
}

// TEST: GameThread sequencer step markers (countable: drain-phase hits
// precede DRAINVIRT, populate-phase hits follow populate-dispatch).
REX_IMPORT(__imp__sub_829FF648, probe_o_829FF648, void());
extern "C" void sub_829FF648(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  ++n;
  uint32_t lr = (uint32_t)ctx.lr;
  // Populate-tail call sites (`bl`+4): always log; other callers sampled.
  bool is_tail = (lr == 0x822F2680) || (lr == 0x822F2688) ||
                 (lr == 0x822F2690) || (lr == 0x822F26BC);
  static unsigned nlr = 0;
  if (is_tail) {
    PROBE_LOG(stderr, "PROBE-HIT 829FF648-TAIL #%u lr=%08X\n", n, lr);
  } else if (nlr < 12) {
    ++nlr;
    PROBE_LOG(stderr, "PROBE-HIT 829FF648 #%u lr=%08X\n", n, lr);
  } else if ((n % 64) == 1) {
    PROBE_LOG(stderr, "PROBE-HIT 829FF648 seq-step #%u\n", n);
}
  probe_o_829FF648(ctx, base);
}
REX_IMPORT(__imp__sub_822F5718, probe_o_822F5718, void());
extern "C" void sub_822F5718(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  if ((++n % 4) == 1) {
    PROBE_LOG(stderr, "PROBE-HIT 822F5718 poptail #%u\n", n);
  }
  // r3 entry = r31+108 of `822F2608`; resolve its post-5718 vtable+0
  // target ([r31+80]->[0]->[vtable+0]) for the stall at `822F269C`.
  // Pure observation: only reads.
  uint32_t r31 = ctx.r3.u32 - 108;
  probe_o_822F5718(ctx, base);
  uint32_t slot = 0, obj = 0, vt = 0, tgt = 0;
  if (r31 >= 0x10000) {
    std::memcpy(&slot, base + r31 + 80, 4);
    slot = __builtin_bswap32(slot);
    if (slot >= 0x10000) {
      std::memcpy(&obj, base + slot, 4);
      obj = __builtin_bswap32(obj);
      if (obj >= 0x10000) {
        std::memcpy(&vt, base + obj, 4);
        vt = __builtin_bswap32(vt);
        if (vt >= 0x10000) {
          std::memcpy(&tgt, base + vt, 4);
          tgt = __builtin_bswap32(tgt);
        }
      }
    }
  }
  if (n <= 2) {
    PROBE_LOG(stderr, "VTABLE+0 #%u slot=%08X obj=%08X vt=%08X tgt=%08X\n",
                 n, slot, obj, vt, tgt);
  }
  static bool done = false;
  if (!done) {
    done = true;
    PROBE_LOG(stderr, "PROBE-HIT 822F5718-EXIT returned\n");
  }
}
REX_IMPORT(__imp__sub_822F71A8, probe_o_822F71A8, void());
extern "C" void sub_822F71A8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 822F71A8 poptail-sub\n");
  }
  probe_o_822F71A8(ctx, base);
  static bool done = false;
  if (!done) {
    done = true;
    PROBE_LOG(stderr, "PROBE-HIT 822F71A8-EXIT returned\n");
  }
}
REX_IMPORT(__imp__sub_83231BE8, probe_o_83231BE8, void());
extern "C" void sub_83231BE8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 83231BE8 refcheck\n");
  }
  probe_o_83231BE8(ctx, base);
  static bool done = false;
  if (!done) {
    done = true;
    PROBE_LOG(stderr, "PROBE-HIT 83231BE8-EXIT returned\n");
  }
}
REX_IMPORT(__imp__sub_82356698, probe_o_82356698, void());
extern "C" void sub_82356698(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  if ((++n % 2) == 1) {
    PROBE_LOG(stderr, "PROBE-HIT 82356698 poptail2 #%u\n", n);
  }
  probe_o_82356698(ctx, base);
  static unsigned nx = 0;
  uint32_t exlr = (uint32_t)ctx.lr;
  // r3 entry = r31+56 of `822F2608`; resolve the following vtable+8
  // ([r31+48],[r31+44]) and vtable+0 ([r31+40],[r31+36]) targets the tail
  // calls next. Pure observation: only reads.
  uint32_t r31 = ctx.r3.u32 - 56;
  uint32_t chain[4] = {48, 44, 40, 36};
  uint32_t off[4] = {8, 8, 0, 0};
  PROBE_LOG(stderr, "PROBE-HIT 82356698-EXIT #%u lr=%08X\n", ++nx, exlr);
  if (nx > 2 && exlr != 0x822F26C4) {
    return;
  }
  for (int i = 0; i < 4; ++i) {
    uint32_t slot = 0, obj = 0, vt = 0, tgt = 0;
    if (r31 >= 0x10000) {
      std::memcpy(&slot, base + r31 + chain[i], 4);
      slot = __builtin_bswap32(slot);
      if (slot >= 0x10000) {
        std::memcpy(&obj, base + slot, 4);
        obj = __builtin_bswap32(obj);
        if (obj >= 0x10000) {
          std::memcpy(&vt, base + obj, 4);
          vt = __builtin_bswap32(vt);
          if (vt >= 0x10000) {
            std::memcpy(&tgt, base + vt + off[i], 4);
            tgt = __builtin_bswap32(tgt);
          }
        }
      }
    }
    PROBE_LOG(stderr, "VTAIL+%u off=%u slot=%08X obj=%08X vt=%08X tgt=%08X\n",
                 chain[i], off[i], slot, obj, vt, tgt);
  }
}
REX_IMPORT(__imp__sub_8217E3F8, probe_o_8217E3F8, void());
extern "C" void sub_8217E3F8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 8217E3F8 seq-step\n");
  }
  probe_o_8217E3F8(ctx, base);
}
REX_IMPORT(__imp__sub_822EB0C8, probe_o_822EB0C8, void());
extern "C" void sub_822EB0C8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 822EB0C8 seq-step\n");
  }
  probe_o_822EB0C8(ctx, base);
}
REX_IMPORT(__imp__sub_822F2518, probe_o_822F2518, void());
extern "C" void sub_822F2518(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 822F2518 seq-step\n");
  }
  probe_o_822F2518(ctx, base);
}

REX_IMPORT(__imp__sub_82B68F60, probe_o_82B68F60, void());
extern "C" void sub_82B68F60(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 82B68F60 postlock\n");
  }
  probe_o_82B68F60(ctx, base);
}
REX_IMPORT(__imp__sub_8236C9F8, probe_o_8236C9F8, void());
extern "C" void sub_8236C9F8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 8236C9F8 postlock\n");
  }
  probe_o_8236C9F8(ctx, base);
}

REX_IMPORT(__imp__sub_822F0518, probe_o_822F0518, void());
extern "C" void sub_822F0518(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 822F0518 deep\n");
  }
  probe_o_822F0518(ctx, base);
}
REX_IMPORT(__imp__sub_82309F00, probe_o_82309F00, void());
extern "C" void sub_82309F00(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 82309F00 deep\n");
  }
  probe_o_82309F00(ctx, base);
}
REX_IMPORT(__imp__sub_825BB2C0, probe_o_825BB2C0, void());
extern "C" void sub_825BB2C0(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 825BB2C0 deep\n");
  }
  probe_o_825BB2C0(ctx, base);
}
REX_IMPORT(__imp__sub_8217DA50, probe_o_8217DA50, void());
extern "C" void sub_8217DA50(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 8217DA50 deep\n");
  }
  probe_o_8217DA50(ctx, base);
}

// Drain reporter + one-shot synthesis (DRAINVIRT): `822F27C0` builds
// GameThread's drain work-item; its return low byte decides drain
// (`!=0` -> `822F47F8` loop) vs populate (`822F2608`). On the FIRST
// verdict==1, force r3 low byte to 0 once so the sequencer takes the
// populate path; later calls pass through. If shutdown follows, 0=quit.
// Set when DRAINVIRT fires: `829FF648` then logs caller LRs so drain-phase
// vs populate-tail call sites can be told apart.
bool g_pop_active = false;
REX_IMPORT(__imp__sub_822F27C0, probe_orig_822F27C0, void());
extern "C" void sub_822F27C0(PPCContext& ctx, uint8_t* base) {
  probe_orig_822F27C0(ctx, base);
  uint32_t v = ctx.r3.u32 & 0xFF;
  // BISECT 2026-09-09: synthesis OFF (was: force first verdict==1 to 0).
  // The intro stopped playing; the drain loop may contain intro items.
  // Pure reporter until the bisect resolves.
  PROBE_LOG(stderr, "DRAIN-VERDICT %u\n", v);
}

REX_IMPORT(__imp__sub_8236CC90, probe_o_8236CC90, void());
extern "C" void sub_8236CC90(PPCContext& ctx, uint8_t* base) {
  PROBE_LOG(stderr, "PROBE-CC90-ENTER\n");
  probe_o_8236CC90(ctx, base);
  PROBE_LOG(stderr, "PROBE-CC90-EXIT\n");
}
REX_IMPORT(__imp__sub_8236CA90, probe_o_8236CA90, void());
extern "C" void sub_8236CA90(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 8236CA90 flagwait\n");
  }
  probe_o_8236CA90(ctx, base);
}

REX_IMPORT(__imp__sub_8221F388, probe_o_8221F388, void());
#include <chrono>
extern "C" void sub_8221F388(PPCContext& ctx, uint8_t* base) {
  auto t0 = std::chrono::steady_clock::now();
  uint32_t sz = ctx.r3.u32;
  uint32_t lr = (uint32_t)ctx.lr;
  probe_o_8221F388(ctx, base);
  auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0);
  if (dt.count() > 20) {
    PROBE_LOG(stderr, "SLOWALLOC size=%u ms=%lld lr=%08X out=%08X\n", sz,
                 (long long)dt.count(), lr, ctx.r3.u32);
  }
}
REX_IMPORT(__imp__sub_8236D4E0, probe_o_8236D4E0, void());
extern "C" void sub_8236D4E0(PPCContext& ctx, uint8_t* base) {
  probe_o_8236D4E0(ctx, base);
}

REX_IMPORT(__imp__sub_822F55A0, probe_o_822F55A0, void());
extern "C" void sub_822F55A0(PPCContext& ctx, uint8_t* base) {
  probe_o_822F55A0(ctx, base);
}

REX_IMPORT(__imp__sub_8240DAA8, probe_o_8240DAA8, void());
extern "C" void sub_8240DAA8(PPCContext& ctx, uint8_t* base) {
  uint32_t gaddr = (uint32_t)((int32_t)-2092367872 + 27088);
  uint32_t pool = 0;
  if (gaddr >= 0x10000) {
    uint32_t be = 0;
    std::memcpy(&be, base + gaddr, 4);
    pool = __builtin_bswap32(be);
  }
  uint32_t r31 = ctx.r6.u32;
  uint32_t f0 = 0, f4 = 0, slot = 0, shead = 0, schunk = 0;
  if (pool >= 0x10000) {
    uint32_t b0 = 0, b4 = 0;
    std::memcpy(&b0, base + pool, 4);
    std::memcpy(&b4, base + pool + 4, 4);
    f0 = __builtin_bswap32(b0);
    f4 = __builtin_bswap32(b4);
    if (r31 != 0 && r31 <= 64) {
      uint32_t r9 = ((r31 + 3) & 0xFFFFFFFC);
      uint32_t be = 0;
      std::memcpy(&be, base + r9 + f4, 4);
      slot = r9 + f4;
      shead = __builtin_bswap32(be);
      std::memcpy(&be, base + slot + 16, 4);
      schunk = __builtin_bswap32(be);
    }
  }
  static uint32_t last = 1;
  uint32_t key = f0 ^ f4 ^ shead ^ schunk;
  uint32_t hnext = 0, b16 = 0;
  if (shead >= 0x10000) {
    uint32_t bh = 0;
    std::memcpy(&bh, base + shead, 4);
    hnext = __builtin_bswap32(bh);
    uint32_t b16be = 0;
    std::memcpy(&b16be, base + shead + 16, 4);
    b16 = __builtin_bswap32(b16be);
  }
  if (key != last) {
    last = key;
    PROBE_LOG(stderr, "POOL-STATE pool=%08X f0=%08X f4=%08X slot=%08X head=%08X next=%08X b16=%08X chunks=%08X size=%u\n",
                 pool, f0, f4, slot, shead, hnext, b16, schunk, r31);
  }
  probe_o_8240DAA8(ctx, base);
  uint32_t out = ctx.r3.u32;
  // NOTE: a wait-for-stock retry (re-call on NULL) was tried and REVERTED:
  // re-running the allocator body has double side effects (pool state
  // corruption) and coincided with deterministic early SIGBUS crashes.
  // NULLs pass through to callers' empty paths.
  static uint32_t lastout = 0;
  if ((out >= 0x82000000 && out < 0x83000000 && out != lastout) ||
      out < 0x10000) {
    lastout = out;
    PROBE_LOG(stderr, "ALLOC-RET 8240DAA8 size=%u out=%08X\n", r31, out);
  }
}
REX_IMPORT(__imp__sub_823052C0, probe_o_823052C0, void());
extern "C" void sub_823052C0(PPCContext& ctx, uint8_t* base) {
  probe_o_823052C0(ctx, base);
  uint32_t gaddr = (uint32_t)((int32_t)-2092367872 + 27088);
  uint32_t be = 0;
  std::memcpy(&be, base + gaddr, 4);
  PROBE_LOG(stderr, "POOL-GLOBAL addr=%08X val=%08X\n", gaddr,
               __builtin_bswap32(be));
}

REX_IMPORT(__imp__sub_82BCD7B0, probe_o_82BCD7B0, void());
extern "C" void sub_82BCD7B0(PPCContext& ctx, uint8_t* base) {
  uint32_t r29 = ctx.r3.u32;
  uint32_t r30 = ctx.r5.u32;
  uint32_t mask = ctx.r6.u32;
  uint32_t r6 = 0, t0 = 0, cnt = 0;
  if (r29 >= 0x10000) {
    uint32_t b6 = 0;
    std::memcpy(&b6, base + r29 + 16, 4);
    r6 = __builtin_bswap32(b6);
    if (r6 >= 0x10000) {
      uint32_t bt0 = 0, bcnt = 0;
      std::memcpy(&bt0, base + r6, 4);
      std::memcpy(&bcnt, base + r6 + 8, 4);
      t0 = __builtin_bswap32(bt0);
      cnt = __builtin_bswap32(bcnt);
    }
  }
  static bool dumped = false;
  if (!dumped && r29 >= 0x10000 && r6 >= 0x10000) {
    dumped = true;
    PROBE_LOG(stderr, "BANK-DUMP r29=%08X r6=%08X\n", r29, r6);
    for (int off = 0; off < 64; off += 16) {
      uint32_t w[4] = {0, 0, 0, 0};
      std::memcpy(w, base + r6 + off, 16);
      PROBE_LOG(stderr, "  tbl+%02X: %08X %08X %08X %08X\n", off,
                   __builtin_bswap32(w[0]), __builtin_bswap32(w[1]),
                   __builtin_bswap32(w[2]), __builtin_bswap32(w[3]));
    }
    for (int off = 0; off < 64; off += 16) {
      uint32_t w[4] = {0, 0, 0, 0};
      std::memcpy(w, base + r29 + off, 16);
      PROBE_LOG(stderr, "  obj+%02X: %08X %08X %08X %08X\n", off,
                   __builtin_bswap32(w[0]), __builtin_bswap32(w[1]),
                   __builtin_bswap32(w[2]), __builtin_bswap32(w[3]));
    }
  }
  uint32_t idx = (((cnt - 1) & mask)) * 4;
  uint32_t tgt = t0 + idx;
  static uint32_t last = 0;
  static uint32_t seen_r6 = 0;
  bool bad = (tgt >= 0x82000000 && tgt < 0x83000000) ||
             (r6 >= 0x82000000 && r6 < 0x83000000) || r29 < 0x10000 ||
             (r30 >= 0x82000000 && r30 < 0x83000000);
  if (bad && tgt != last) {
    last = tgt;
    PROBE_LOG(stderr,
                 "BANK-TBL r29=%08X r30=%08X r6=%08X base=%08X cnt=%08X mask=%08X tgt=%08X\n",
                 r29, r30, r6, t0, cnt, mask, tgt);
  }
  if (r29 >= 0x10000 && r6 >= 0x10000 && r6 != seen_r6) {
    seen_r6 = r6;
    PROBE_LOG(stderr, "BANK-TBL-SEEN r6=%08X base=%08X cnt=%08X\n", r6, t0,
                 cnt);
  }
  static uint32_t seen_key = 0;
  uint32_t key = r30 ^ (r30 >> 4);
  if (r29 >= 0x10000 && key != seen_key) {
    seen_key = key;
    PROBE_LOG(stderr, "BANK-R30 r30=%08X r6=%08X r30+16=%08X\n", r30, r6,
                 r30 + 16);
  }

  std::lock_guard<std::recursive_mutex> lk0(bank_table_mutex);
  probe_o_82BCD7B0(ctx, base);
}
// Worker-path trace (2026-09-10): the verdict worker (gid 0x3009C018)
// enters 822F33B8 but never returns; per-gid first-entry + exit logs on
// the chain 82BC9E10 -> 82BCA340 -> 82BC7C20 -> 822C0568 -> 82BC7FB0
// isolate the deepest frame with ENTER but no EXIT (the park container).
static bool pathlog_seen(uint32_t gid, uint32_t* tab, unsigned n) {
  for (unsigned i = 0; i < n; ++i) {
    if (tab[i] == gid) {
      return true;
    }
    if (tab[i] == 0) {
      tab[i] = gid;
      return false;
    }
  }
  return true;
}
#define PATH_PROBE(addr)                                                      \
  REX_IMPORT(__imp__sub_##addr, probe_o_##addr, void());                      \
  extern "C" void sub_##addr(PPCContext& ctx, uint8_t* base) {                \
    uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object(); \
    static uint32_t seen_in[8] = {0};                                         \
    static uint32_t seen_out[8] = {0};                                        \
    bool interesting = (gid == 0x3009C018) || (gid == 0x30097018);            \
    if (interesting || !pathlog_seen(gid, seen_in, 8)) {                      \
      PROBE_LOG(stderr, "PATHENTER %08X gid=%08X lr=%08X\n", 0x##addr,     \
                   gid, (uint32_t)ctx.lr);                                    \
    }                                                                         \
    probe_o_##addr(ctx, base);                                                \
    if (interesting || !pathlog_seen(gid, seen_out, 8)) {                     \
      PROBE_LOG(stderr, "PATHEXIT %08X gid=%08X ret=%08X\n", 0x##addr,     \
                   gid, ctx.r3.u32);                                          \
    }                                                                         \
  }
PATH_PROBE(82BC7C20)
// Resume-jump infrastructure (2026-09-10, TEST reversible): 82CA9260 is a
// context-restore whose blr must transfer to the saved LR (822C05A8, inside
// an ancestor 822C0568 frame). Generated code C++-returns instead. This
// buffer lets the 82CA9260 override run the resume and longjmp back here,
// skipping the dead dispatcher chain. Outermost 822C0568 wins the buffer.
#include <csetjmp>
static thread_local jmp_buf tls_resume_jb;
static thread_local bool tls_resume_armed = false;
REX_IMPORT(__imp__sub_822C0568, probe_o_822C0568, void());
extern "C" void sub_822C0568(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  static uint32_t seen_in[8] = {0};
  static uint32_t seen_out[8] = {0};
  bool interesting = (gid == 0x3009C018) || (gid == 0x30097018);
  if (interesting || !pathlog_seen(gid, seen_in, 8)) {
    PROBE_LOG(stderr, "PATHENTER %08X gid=%08X lr=%08X\n", 0x822C0568,
                 gid, (uint32_t)ctx.lr);
  }
  bool outer = !tls_resume_armed;
  int lj = 0;
  if (outer) {
    tls_resume_armed = true;
    lj = setjmp(tls_resume_jb);
  }
  if (lj == 0) {
    probe_o_822C0568(ctx, base);
  } else {
    uint32_t gid2 = rex::system::XThread::GetCurrentThread()->guest_object();
    PROBE_LOG(stderr, "RESUMED 822C0568 gid=%08X ret=%08X\n", gid2,
                 ctx.r3.u32);
  }
  tls_resume_armed = false;
  if (interesting || !pathlog_seen(gid, seen_out, 8)) {
    PROBE_LOG(stderr, "PATHEXIT %08X gid=%08X ret=%08X\n", 0x822C0568,
                 gid, ctx.r3.u32);
  }
}
PATH_PROBE(82BC7FB0)

REX_IMPORT(__imp__sub_822C05F8, probe_o_822C05F8, void());
extern "C" void sub_822C05F8(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  static uint32_t seen[8] = {0};
  bool interesting = (gid == 0x3009C018) || (gid == 0x30097018);
  if (interesting || !pathlog_seen(gid, seen, 8)) {
    PROBE_LOG(stderr, "C05F8ENTER gid=%08X lr=%08X r3=%08X r4=%08X r5=%08X\n",
                 gid, (uint32_t)ctx.lr, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
  }
  probe_o_822C05F8(ctx, base);
  if (interesting) {
    PROBE_LOG(stderr, "C05F8EXIT gid=%08X ret=%08X\n", gid, ctx.r3.u32);
  }
}

REX_IMPORT(__imp__sub_821E27C8, probe_o_821E27C8, void());
extern "C" void sub_821E27C8(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  static uint32_t seen[8] = {0};
  bool interesting = (gid == 0x3009C018) || (gid == 0x30097018);
  if (interesting || !pathlog_seen(gid, seen, 8)) {
    PROBE_LOG(stderr, "STORMENTER gid=%08X lr=%08X r3=%08X r4=%08X\n",
                 gid, (uint32_t)ctx.lr, ctx.r3.u32, ctx.r4.u32);
  }
  probe_o_821E27C8(ctx, base);
  if (interesting) {
    PROBE_LOG(stderr, "STORMEXIT gid=%08X ret=%08X\n", gid, ctx.r3.u32);
  }
}

REX_IMPORT(__imp__sub_8219EE00, probe_o_8219EE00, void());
extern "C" void sub_8219EE00(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  static uint32_t seen[8] = {0};
  bool interesting = (gid == 0x3009C018) || (gid == 0x30097018);
  uint32_t r1in = ctx.r1.u32;
  if (interesting || !pathlog_seen(gid, seen, 8)) {
    PROBE_LOG(stderr, "EE00ENTER gid=%08X lr=%08X r3=%08X r4=%08X\n",
                 gid, (uint32_t)ctx.lr, ctx.r3.u32, ctx.r4.u32);
  }
  probe_o_8219EE00(ctx, base);
  if (interesting) {
    PROBE_LOG(stderr, "EE00EXIT gid=%08X ret=%08X\n", gid, ctx.r3.u32);
    if (ctx.r1.u32 != r1in) {
      PROBE_LOG(stderr, "R1PROPAGATE gid=%08X in=%08X out=%08X\n", gid,
                   r1in, ctx.r1.u32);
    }
  }
}
REX_IMPORT(__imp__sub_8219F010, probe_o_8219F010, void());
extern "C" void sub_8219F010(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  bool interesting = (gid == 0x3009C018);
  uint32_t r29in = ctx.r29.u32;
  uint32_t r1in = ctx.r1.u32;
  static uint32_t seen10[8] = {0};
  if (interesting || !pathlog_seen(gid, seen10, 8)) {
    uint32_t r11 = ctx.r11.u32;
    auto rd = [&](uint32_t a) -> uint32_t {
      if (a < 0x10000 || a >= 0x84000000) {
        return 0;
      }
      uint32_t b = 0;
      std::memcpy(&b, base + a, 4);
      return __builtin_bswap32(b);
    };
    uint32_t t1 = rd(r11 + 4);
    uint32_t t2 = rd(t1);
    uint32_t tgt = rd(t2 + 16);
    PROBE_LOG(stderr, "F010ENTER gid=%08X lr=%08X r3=%08X r11=%08X bctr=%08X r1=%08X\n",
                 gid, (uint32_t)ctx.lr, ctx.r3.u32, r11, tgt, ctx.r1.u32);
  }
  probe_o_8219F010(ctx, base);
  if (interesting && (ctx.r29.u32 != r29in || ctx.r1.u32 != r1in)) {
    PROBE_LOG(stderr, "R29CLOBBER gid=%08X in=%08X out=%08X r1in=%08X r1out=%08X lr=%08X\n",
                 gid, r29in, ctx.r29.u32, r1in, ctx.r1.u32, (uint32_t)ctx.lr);
    void* bt[16];
    int nbt = backtrace(bt, 16);
    backtrace_symbols_fd(bt, nbt, STDERR_FILENO);
  }
}

// r1-balance probes (2026-09-10): 8219F010 returns with r1 +0x3D0 on the
// worker, so a nested call leaks stack. Check each direct callee.
#define R1BAL_PROBE(addr)                                                     \
  REX_IMPORT(__imp__sub_##addr, probe_o_##addr, void());                      \
  extern "C" void sub_##addr(PPCContext& ctx, uint8_t* base) {                \
    uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object(); \
    bool interesting = (gid == 0x3009C018);                                   \
    uint32_t r1in = ctx.r1.u32;                                               \
    probe_o_##addr(ctx, base);                                                \
    if (interesting && ctx.r1.u32 != r1in) {                                  \
      PROBE_LOG(stderr, "R1LEAK %08X gid=%08X in=%08X out=%08X\n",         \
                   0x##addr, gid, r1in, ctx.r1.u32);                          \
    }                                                                         \
  }
R1BAL_PROBE(82BCD1E8)
R1BAL_PROBE(82BCCD58)
R1BAL_PROBE(82BCCFF8)
R1BAL_PROBE(82BCCE98)
R1BAL_PROBE(82BB29D0)
R1BAL_PROBE(82BB6CA0)
R1BAL_PROBE(82BC68F0)
R1BAL_PROBE(822AF338)
R1BAL_PROBE(82BCBDC8)
R1BAL_PROBE(8227B8B8)
R1BAL_PROBE(82188CF0)
R1BAL_PROBE(82BB1E58)
R1BAL_PROBE(82BC6A18)
R1BAL_PROBE(82BC6980)
R1BAL_PROBE(82BC8490)
R1BAL_PROBE(822CE098)
R1BAL_PROBE(82BC9788)
R1BAL_PROBE(82BCCB88)
// TEST (2026-09-10, reversible): 82CA9260 is a context-restore whose blr
// must transfer to the saved LR (822C05A8, inside an ancestor 822C0568
// frame). Generated code C++-returns instead, running unreachable dispatcher
// 822C0568 wrapper, abandoning the dead chain.
REX_IMPORT(__imp__sub_82CA9260, probe_o_82CA9260, void());
extern "C" void sub_82CA9260(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  uint32_t r1in = ctx.r1.u32;
  uint64_t lrin = ctx.lr;
  probe_o_82CA9260(ctx, base);
  uint32_t lrout = (uint32_t)ctx.lr;
  bool interesting = (gid == 0x3009C018);
  if (tls_resume_armed && lrout == 0x822C05A8 && lrout != (uint32_t)lrin) {
    if (interesting) {
      PROBE_LOG(stderr, "RESUMEJUMP gid=%08X r1=%08X\n", gid, ctx.r1.u32);
    }
    // --- resume at 822C05A8 (mirrors sub_822C0568 loc_822C05A8..blr) ---
    // cmpwi cr6,r3,0
    ctx.cr6.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
    // bne cr6,0x822c05d4
    if (!ctx.cr6.eq) goto loc_822C05D4;
    // lwz r31,1492(r1)
    ctx.r31.u64 = REX_LOAD_U32(ctx.r1.u32 + 1492);
    // lwz r11,1500(r1)
    ctx.r11.u64 = REX_LOAD_U32(ctx.r1.u32 + 1500);
    // lwz r4,1508(r1)
    ctx.r4.u64 = REX_LOAD_U32(ctx.r1.u32 + 1508);
    // mr r3,r31
    ctx.r3.u64 = ctx.r31.u64;
    // mtctr r11
    ctx.ctr.u64 = ctx.r11.u64;
    // bctrl
    ctx.lr = 0x822C05C8;
    REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
    // lwz r10,80(r1)
    ctx.r10.u64 = REX_LOAD_U32(ctx.r1.u32 + 80);
    // stw r10,92(r31)
    REX_STORE_U32(ctx.r31.u32 + 92, ctx.r10.u32);
    // b 0x822c05e0
    goto loc_822C05E0;
  loc_822C05D4:
    // lwz r11,1492(r1)
    ctx.r11.u64 = REX_LOAD_U32(ctx.r1.u32 + 1492);
    // lwz r10,80(r1)
    ctx.r10.u64 = REX_LOAD_U32(ctx.r1.u32 + 80);
    // stw r10,92(r11)
    REX_STORE_U32(ctx.r11.u32 + 92, ctx.r10.u32);
  loc_822C05E0:
    // lwz r3,1440(r1)
    ctx.r3.u64 = REX_LOAD_U32(ctx.r1.u32 + 1440);
    // addi r1,r1,1472
    ctx.r1.s64 = ctx.r1.s64 + 1472;
    // lwz r12,-8(r1)
    ctx.r12.u64 = REX_LOAD_U32(ctx.r1.u32 + -8);
    // mtlr r12
    ctx.lr = ctx.r12.u64;
    // ld r31,-16(r1)
    ctx.r31.u64 = REX_LOAD_U64(ctx.r1.u32 + -16);
    // blr -> abandon the dead chain, resume the ancestor 822C0568 frame
    longjmp(tls_resume_jb, 1);
  }
  if (interesting && ctx.r1.u32 != r1in) {
    PROBE_LOG(stderr, "R1LEAK %08X gid=%08X in=%08X out=%08X\n",
                 0x82CA9260, gid, r1in, ctx.r1.u32);
  }
}
R1BAL_PROBE(82BCC6F0)
R1BAL_PROBE(82BCCAB8)
R1BAL_PROBE(82BCCDE0)
R1BAL_PROBE(82CA9798)
R1BAL_PROBE(82CB5B20)
// Thunk-slot chains (2026-09-10): vtable slots sharing a tail body are
// adjacent manifest entries; HW falls through on bctrl-return, so each
// override runs its stub then explicitly calls the next slot/body.
#define THUNK_CHAIN(from, to) \
  REX_IMPORT(__imp__sub_##from, probe_o_##from, void()); \
  extern "C" void sub_##to(PPCContext& ctx, uint8_t* base); \
  extern "C" void sub_##from(PPCContext& ctx, uint8_t* base) { \
    probe_o_##from(ctx, base); \
    sub_##to(ctx, base); \
  }
THUNK_CHAIN(82C4C320, 82C4C340)
THUNK_CHAIN(82C4C340, 82C4C360)
THUNK_CHAIN(82C4C5C8, 82C4C5E8)
THUNK_CHAIN(82C4C5E8, 82C4C610)
THUNK_CHAIN(82C4C610, 82C4C630)
THUNK_CHAIN(829FCAE8, 829FCB00)
// Park probes (2026-09-10): worker enters 82C65D80 and never returns.
// Log EVERY worker entry/exit on its plausible-blocking callees; the one
// with ENTER-but-no-EXIT is the park. Cold paths only.
#define PARK_PROBE(addr) \
  REX_IMPORT(__imp__sub_##addr, probe_o_##addr, void()); \
  extern "C" void sub_##addr(PPCContext& ctx, uint8_t* base) { \
    uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object(); \
    bool w = (gid == 0x3009C018); \
    if (w) PROBE_LOG(stderr, "PARKIN %08X r1=%08X\n", 0x##addr, ctx.r1.u32); \
    probe_o_##addr(ctx, base); \
    if (w) PROBE_LOG(stderr, "PARKOUT %08X\n", 0x##addr); \
  }
PARK_PROBE(82CA3700)
PARK_PROBE(82366210)
PARK_PROBE(822F54C8)
static uint32_t srs_read(PPCContext& ctx, uint8_t* base, uint32_t addr) {
  if (addr < 0x10000 || addr >= 0x84000000) {
    return 0xDDDDDDDD;
  }
  uint32_t b = 0;
  std::memcpy(&b, base + addr, 4);
  return __builtin_bswap32(b);
}
// U64 BE store at [r1+off]: low 32 bits live 4 bytes higher than an stw slot.
static uint32_t srs_read64(PPCContext& ctx, uint8_t* base, uint32_t r1,
                           int off) {
  return srs_read(ctx, base, r1 + off + 4);
}
struct SRFrame {
  uint32_t r1;
  uint32_t r29;
  uint32_t lr;
};
static SRFrame sr_stack[512];
static unsigned sr_depth = 0;
static void sr_push(uint32_t r1, uint32_t r29, uint32_t lr) {
  if (sr_depth < 512) {
    sr_stack[sr_depth].r1 = r1;
    sr_stack[sr_depth].r29 = r29;
    sr_stack[sr_depth].lr = lr;
    ++sr_depth;
  }
}
static bool sr_pop(uint32_t* r1, uint32_t* r29, uint32_t* lr) {
  if (sr_depth == 0) {
    return false;
  }
  --sr_depth;
  *r1 = sr_stack[sr_depth].r1;
  *r29 = sr_stack[sr_depth].r29;
  *lr = sr_stack[sr_depth].lr;
  return true;
}
REX_IMPORT(__imp____savegprlr_26, probe_o_save26, void());
extern "C" void __savegprlr_26(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  bool interesting = (gid == 0x3009C018);
  uint32_t r1 = ctx.r1.u32;
  probe_o_save26(ctx, base);
  if (interesting) {
    static unsigned long sq = 0;
    sr_push(r1, ctx.r29.u32, (uint32_t)ctx.lr);
    PROBE_LOG(stderr, "SAVE26 #%lu gid=%08X r1=%08X r29=%08X lr=%08X s29=%08X slr=%08X depth=%u\n",
                 ++sq, gid, r1, ctx.r29.u32, (uint32_t)ctx.lr,
                 srs_read64(ctx, base, r1, -32), srs_read(ctx, base, r1 - 8),
                 sr_depth);
  }
}
REX_IMPORT(__imp____restgprlr_26, probe_o_rest26, void());
extern "C" void __restgprlr_26(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  bool interesting = (gid == 0x3009C018);
  uint32_t r1 = ctx.r1.u32;
  if (interesting) {
    static unsigned long rq = 0;
    uint32_t s29 = srs_read64(ctx, base, r1, -32);
    uint32_t slr = srs_read(ctx, base, r1 - 8);
    uint32_t er1 = 0, er29 = 0, elr = 0;
    bool ok = sr_pop(&er1, &er29, &elr);
    const char* verdict = !ok ? "STACK-EMPTY"
                          : (er1 != r1 ? "R1-MISMATCH"
                                       : (s29 != er29 || slr != elr ? "SLOT-CHANGED" : "ok"));
    PROBE_LOG(stderr, "REST26PRE #%lu gid=%08X r1=%08X s29=%08X slr=%08X exp-r1=%08X exp-r29=%08X exp-lr=%08X %s depth=%u\n",
                 ++rq, gid, r1, s29, slr, er1, er29, elr, verdict, sr_depth);
  }
  probe_o_rest26(ctx, base);
  if (interesting) {
    PROBE_LOG(stderr, "REST26POST gid=%08X r29=%08X lr=%08X\n", gid,
                 ctx.r29.u32, (uint32_t)ctx.lr);
  }
}

REX_IMPORT(__imp__sub_822DF280, probe_o_822DF280, void());
extern "C" void sub_822DF280(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  static uint32_t seen[8] = {0};
  bool interesting = (gid == 0x3009C018);
  if (interesting || !pathlog_seen(gid, seen, 8)) {
    // Two-level link snapshot: l20=[r3+20], l20d=[[r3+20]+4] deref [0].
    uint32_t l20 = 0xEEEEEEEE, l20d = 0xEEEEEEEE;
    uint32_t a3 = ctx.r3.u32;
    if (a3 >= 0x10000 && a3 < 0x84000000) {
      uint32_t b = 0;
      std::memcpy(&b, base + a3 + 20, 4);
      l20 = __builtin_bswap32(b);
      if (l20 >= 0x10000 && l20 < 0x84000000) {
        uint32_t c = 0;
        std::memcpy(&c, base + l20 + 4, 4);
        uint32_t r10 = __builtin_bswap32(c);
        if (r10 >= 0x10000 && r10 < 0x84000000) {
          uint32_t d = 0;
          std::memcpy(&d, base + r10, 4);
          l20d = __builtin_bswap32(d);
        } else {
          l20d = r10;
        }
      }
    }
    PROBE_LOG(stderr, "F280ENTER gid=%08X lr=%08X r3=%08X r4=%08X l20=%08X l20d=%08X\n",
                 gid, (uint32_t)ctx.lr, ctx.r3.u32, ctx.r4.u32, l20, l20d);
  }
  probe_o_822DF280(ctx, base);
  if (interesting) {
    PROBE_LOG(stderr, "F280EXIT gid=%08X ret=%08X\n", gid, ctx.r3.u32);
  }
}


// Callback-drain sampler (2026-09-10): 822C0568 loops on 83000200 until it
// returns 0, then takes an indirect call. Counts iterations to distinguish
// a never-emptying callback queue (counter explodes) from a parked indirect
// target (counter static).
REX_IMPORT(__imp__sub_83000200, probe_o_83000200, void());

REX_IMPORT(__imp__sub_8229A518, probe_o_8229A518, void());
extern "C" void sub_8229A518(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  static uint32_t seen[8] = {0};
  bool interesting = (gid == 0x3009C018);
  if (interesting || !pathlog_seen(gid, seen, 8)) {
    PROBE_LOG(stderr, "A518ENTER gid=%08X lr=%08X r3=%08X r4=%08X r5=%08X r6=%08X\n",
                 gid, (uint32_t)ctx.lr, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32,
                 ctx.r6.u32);
  }
  probe_o_8229A518(ctx, base);
  if (interesting) {
    PROBE_LOG(stderr, "A518EXIT gid=%08X ret=%08X\n", gid, ctx.r3.u32);
  }
}
extern "C" void sub_83000200(PPCContext& ctx, uint8_t* base) {
  static unsigned long n = 0;
  ++n;
  if (n == 1 || (n % 1048576) == 0) {
    PROBE_LOG(stderr, "CBDRAIN #%lu ret=? lr=%08X\n", n,
                 (uint32_t)ctx.lr);
  }
  probe_o_83000200(ctx, base);
}

REX_IMPORT(__imp__sub_82BCA340, probe_o_82BCA340, void());
extern "C" void sub_82BCA340(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  static uint32_t seen_in[8] = {0};
  static uint32_t seen_out[8] = {0};
  bool interesting = (gid == 0x3009C018) || (gid == 0x30097018);
  if (interesting || !pathlog_seen(gid, seen_in, 8)) {
    PROBE_LOG(stderr, "PATHENTER %08X gid=%08X lr=%08X\n", 0x82BCA340,
                 gid, (uint32_t)ctx.lr);
  }
  std::lock_guard<std::recursive_mutex> lk1(bank_table_mutex);
  probe_o_82BCA340(ctx, base);
  if (interesting || !pathlog_seen(gid, seen_out, 8)) {
    PROBE_LOG(stderr, "PATHEXIT %08X gid=%08X ret=%08X\n", 0x82BCA340,
                 gid, ctx.r3.u32);
  }
}
REX_IMPORT(__imp__sub_82BC9E10, probe_o_82BC9E10, void());
extern "C" void sub_82BC9E10(PPCContext& ctx, uint8_t* base) {
  uint32_t gid = rex::system::XThread::GetCurrentThread()->guest_object();
  static uint32_t seen_in[8] = {0};
  static uint32_t seen_out[8] = {0};
  bool interesting = (gid == 0x3009C018) || (gid == 0x30097018);
  if (interesting || !pathlog_seen(gid, seen_in, 8)) {
    PROBE_LOG(stderr, "PATHENTER %08X gid=%08X lr=%08X\n", 0x82BC9E10,
                 gid, (uint32_t)ctx.lr);
  }
  std::lock_guard<std::recursive_mutex> lk2(bank_table_mutex);
  probe_o_82BC9E10(ctx, base);
  if (interesting || !pathlog_seen(gid, seen_out, 8)) {
    PROBE_LOG(stderr, "PATHEXIT %08X gid=%08X ret=%08X\n", 0x82BC9E10,
                 gid, ctx.r3.u32);
  }
}
REX_IMPORT(__imp__sub_82BC9EA0, probe_o_82BC9EA0, void());
extern "C" void sub_82BC9EA0(PPCContext& ctx, uint8_t* base) {
  std::lock_guard<std::recursive_mutex> lk3(bank_table_mutex);
  probe_o_82BC9EA0(ctx, base);
}

REX_IMPORT(__imp__sub_83230568, probe_o_83230568, void());
extern "C" void sub_83230568(PPCContext& ctx, uint8_t* base) {
  static unsigned long n = 0;
  uint32_t in = ctx.r3.u32;
  probe_o_83230568(ctx, base);
  uint32_t out = ctx.r3.u32;
  static uint32_t lin = 0, lout = 0;
  static unsigned m = 0;
  if (m < 20 && (in != lin || out != lout)) {
    lin = in;
    lout = out;
    ++m;
    PROBE_LOG(stderr, "POOL-GROW #%lu slot=%08X out=%08X\n", ++n, in, out);
  } else {
    ++n;
  }
}
REX_IMPORT(__imp__sub_82CC2028, probe_o_82CC2028, void());
extern "C" void sub_82CC2028(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  if (n < 6) {
    ++n;
    uint32_t o = ctx.r4.u32;
    uint32_t f = 0;
    if (o >= 0x10000) {
      f = *(base + o) & 0xFF;
    }
    PROBE_LOG(stderr, "FLAGWAIT obj=%08X flag=%02X lr=%08X\n", o, f,
                 (uint32_t)ctx.lr);
  }
  // REVERTED 2026-09-10 (see 82CBC6B0 note): restore the native 100 ms
  // alertable delay. Null object => single KeDelay + return, no park.
#if 0
  if (ctx.r4.u32 < 0x10000) {
    // TEST (reversible): null-object wait. On HW `r4` is never null here;
    // under protect_zero=0 the zero page accumulates silent null-write
    // garbage, so `[0]&0xFF` never clears and every null-wait parks
    // forever (GameThread via `82CBC6B0`, two early spinners). A null
    // object means "nothing to wait for": return immediately (the
    // not-taken branch). Revert if any caller relies on the wait.
    static bool logged = false;
    if (!logged) {
      logged = true;
      PROBE_LOG(stderr, "SKIP-NULLWAIT lr=%08X\n", (uint32_t)ctx.lr);
    }
    ctx.r3.u32 = 0;
    return;
  }
#endif
  probe_o_82CC2028(ctx, base);
}
REX_IMPORT(__imp__sub_823784A0, probe_o_823784A0, void());
extern "C" void sub_823784A0(PPCContext& ctx, uint8_t* base) {
  static unsigned nn = 0;
  uint32_t a3 = ctx.r3.u32, a4 = ctx.r4.u32;
  if (nn < 8) {
    ++nn;
    auto rd = [&](uint32_t a) -> uint32_t {
      if (a < 0x10000) {
        return 0xFFFFFFFF;
      }
      uint32_t b = 0;
      std::memcpy(&b, base + a, 4);
      return __builtin_bswap32(b);
    };
    uint32_t f = (a4 >= 0x10000) ? (*(base + a4) & 0xFF) : 0xEE;
    PROBE_LOG(stderr, "DRAINVIRT r3=%08X r4=%08X [r4]=%02X lr=%08X\n", a3,
                 a4, f, (uint32_t)ctx.lr);
    if (nn == 1) {
      drain_flag_addr = a4;
      drain_item_addr = a3;
      if (a3 >= 0x10000) {
        for (int off = 0; off < 320; off += 32) {
          uint32_t w[8] = {0, 0, 0, 0, 0, 0, 0, 0};
          std::memcpy(w, base + a3 + off, 32);
          PROBE_LOG(stderr, "  item+%03X: %08X %08X %08X %08X %08X %08X %08X %08X\n",
                       off, __builtin_bswap32(w[0]), __builtin_bswap32(w[1]),
                       __builtin_bswap32(w[2]), __builtin_bswap32(w[3]),
                       __builtin_bswap32(w[4]), __builtin_bswap32(w[5]),
                       __builtin_bswap32(w[6]), __builtin_bswap32(w[7]));
        }
      }
    }
    uint32_t vt = (a3 >= 0x10000) ? rd(a3) : 0;
    uint32_t s16 = 0, s20 = 0, s24 = 0, s28 = 0;
    if (vt >= 0x10000 && vt < 0x84000000) {
      s16 = rd(vt + 16);
      s20 = rd(vt + 20);
      s24 = rd(vt + 24);
      s28 = rd(vt + 28);
    }
    PROBE_LOG(stderr, "VIRTTGT vt=%08X [16]=%08X [20]=%08X [24]=%08X [28]=%08X\n",
                 vt, s16, s20, s24, s28);
    // ITEM BEGIN snapshot (2026-09-10): 822F3640 exits only when
    // [drainctx+52]!=0 && [drainctx+5]!=0 (consumes +52, returns +5).
    // drainctx lives at item+0x5C (last word of the +0x40 row).
    uint32_t dctx = (a3 >= 0x10000) ? rd(a3 + 0x5C) : 0;
    if (dctx >= 0x10000 && dctx < 0x84000000) {
      uint32_t w52 = 0;
      std::memcpy(&w52, base + dctx + 52, 4);
      PROBE_LOG(stderr, "DRAINCTX ctx=%08X b5=%02X w52=%08X\n", dctx,
                   *(base + dctx + 5), __builtin_bswap32(w52));
    }
    // Drain-wait exit gate (0x823787F0): virtual [vtable+16] on the object
    // at global [0x8349E6EC] (r28=0x834A0000-ish, [r28-6420]). Resolve it.
    uint32_t gobj = rd(0x8349E6EC);
    uint32_t gvt = (gobj >= 0x10000 && gobj < 0x84000000) ? rd(gobj) : 0;
    uint32_t gtgt = (gvt >= 0x10000 && gvt < 0x84000000) ? rd(gvt + 16) : 0;
    PROBE_LOG(stderr, "GATETGT obj=%08X vt=%08X tgt=%08X\n", gobj, gvt,
                 gtgt);
    if (gobj >= 0x10000) {
      gate_byte_addr = gobj + 44;
    }
  }
  probe_o_823784A0(ctx, base);
  static unsigned nret = 0;
  if (nret < 20) {
    ++nret;
    PROBE_LOG(stderr, "VIRTRET r3=%08X\n", ctx.r3.u32);
  }
}

REX_IMPORT(__imp__sub_82477768, probe_o_82477768, void());
extern "C" void sub_82477768(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  if (n < 8) {
    ++n;
    PROBE_LOG(stderr, "CLEARER r3=%08X r4=%08X lr=%08X\n", ctx.r3.u32,
                 ctx.r4.u32, (uint32_t)ctx.lr);
  }
  probe_o_82477768(ctx, base);
}

REX_IMPORT(__imp__sub_82B9B8D8, probe_o_82B9B8D8, void());
extern "C" void sub_82B9B8D8(PPCContext& ctx, uint8_t* base) {
  static unsigned long n = 0;
  if (++n <= 5 || n % 1000 == 0) {
    PROBE_LOG(stderr, "GPU-IRQ #%lu\n", n);
  }
  probe_o_82B9B8D8(ctx, base);
}
// NOTE: REX_IMPORT hooks on kernel imports (`__imp__NtWaitForSingleObjectEx`,
// `__imp__KeDelayExecutionThread`) do NOT fire: kernel imports resolve
// outside the hookable import layer (verified: zero hits across runs while
// threads demonstrably wait). Thread-wait mapping needs another route
// (perf stacks, guest-side brackets). Hooks removed to avoid false
// "nobody waits" readings.

REX_IMPORT(__imp__sub_82378B10, probe_o_82378B10, void());
extern "C" void sub_82378B10(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "TEARDOWN-RACE 82378B10 zeroes global+26920\n");
  }
  probe_o_82378B10(ctx, base);
}

REX_IMPORT(__imp__sub_822F3640, probe_o_822F3640, void());
extern "C" void sub_822F3640(PPCContext& ctx, uint8_t* base) {
  uint32_t lr = (uint32_t)ctx.lr;
  static bool logged14 = false, logged34 = false;
  bool mine = (lr == 0x82378814 && !logged14) || (lr == 0x82378834 && !logged34);
  if (mine) {
    if (lr == 0x82378814) {
      logged14 = true;
    } else {
      logged34 = true;
    }
    uint32_t gaddr = (uint32_t)((int32_t)-2092367872 + 26960);
    uint32_t gl = 0, o11 = 0, o31 = 0;
    if (gaddr >= 0x10000) {
      uint32_t b = 0;
      std::memcpy(&b, base + gaddr, 4);
      gl = __builtin_bswap32(b);
      if (gl >= 0x10000) {
        std::memcpy(&b, base + gl + 8, 4);
        o11 = __builtin_bswap32(b);
      }
    }
    uint32_t r31 = ctx.r3.u32;
    uint32_t lk = (r31 >= 0x10000) ? r31 + 8 : 0;
    int32_t lc = 0;
    uint32_t ow = 0;
    if (lk >= 0x10000) {
      std::memcpy(&lc, base + lk + 0x10, 4);
      uint32_t beow = 0;
      std::memcpy(&beow, base + lk + 0x18, 4);
      ow = __builtin_bswap32(beow);
    }
    PROBE_LOG(stderr, "GTLOCK global=%08X o11=%08X r31=%08X lock=%08X count=%d owner=%08X\n",
                 gl, o11, r31, lk, lc, ow);
  }
  // Post-loop spin sampler (0x82378834: waits for [r31+5]!=0): sampled
  // + value-logged so post-synth progress stays visible.
  if (lr == 0x82378834) {
    static unsigned long sn = 0;
    uint32_t r3v = ctx.r3.u32;
    uint8_t b5 = 0xEE;
    if (r3v >= 0x10000 && r3v < 0x84000000) {
      b5 = *(base + r3v + 5);
    }
    // TRACE VOLUME: change-only + rare heartbeat. The loop polls at ~7M/s
    // with the 100 ms wait bypassed; every-64th sampling floods I/O (~110k
    // lines/s) and perturbs scheduling. b5/w52 transitions are the signal
    // (822F3640 needs both nonzero).
    static uint8_t sb5 = 0xEE;
    static uint32_t sw52 = 0xEEEEEEEE;
    static uint32_t sr3 = 0;
    uint32_t w52 = 0xEEEEEEEE;
    if (r3v >= 0x10000 && r3v < 0x84000000) {
      std::memcpy(&w52, base + r3v + 52, 4);
      w52 = __builtin_bswap32(w52);
    }
    ++sn;
    if (b5 != sb5 || w52 != sw52 || r3v != sr3 || (sn % 1048576) == 1) {
      sb5 = b5;
      sw52 = w52;
      sr3 = r3v;
      PROBE_LOG(stderr, "SPIN34 #%lu r3=%08X b5=%02X w52=%08X\n", sn,
                   r3v, b5, w52);
    }
    // REPEATING SYNTH OFF 2026-09-10: trace-only mode (see flag/gate note).
#if 0
    // TEST (reversible): synthesize the spin event, REPEATING. Post-gate
    // the loop spins on 822F3640's return ([r3+5]); nothing ever sets it.
    // Re-fire on cooldown while parked; watch for 822F2608 after each.
    static long long wlast = 0;
    static long long wt0 = 0;
    if (wt0 == 0) {
      wt0 = (long long)time(nullptr);
    }
    if (b5 == 0 && r3v >= 0x10000 &&
        (long long)time(nullptr) - wt0 > 15 &&
        (long long)time(nullptr) - wlast > 30) {
      wlast = (long long)time(nullptr);
      *(base + r3v + 5) = 1;
      PROBE_LOG(stderr, "SPINSYNTH set %08X+5\n", r3v);
    }
#endif
  }
  probe_o_822F3640(ctx, base);
}

REX_IMPORT(__imp__sub_8221EB58, probe_o_8221EB58, void());
extern "C" void sub_8221EB58(PPCContext& ctx, uint8_t* base) {
  static unsigned long n = 0;
  probe_o_8221EB58(ctx, base);
  if (++n == 1 || n == 200000) {
    PROBE_LOG(stderr, "GUESTTICK #%lu r3=%08X r4=%08X\n", n, ctx.r3.u32,
                 ctx.r4.u32);
  }
}
REX_IMPORT(__imp__sub_831FD318, probe_o_831FD318, void());
extern "C" void sub_831FD318(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 8;
  if (log) {
    ++n;
    PROBE_LOG(stderr, "V16-ENTER r3=%08X lr=%08X\n", ctx.r3.u32,
                 (uint32_t)ctx.lr);
  } else if ((uint32_t)ctx.lr == 0x823787F4) {
    PROBE_LOG(stderr, "V16-GATE r3=%08X\n", ctx.r3.u32);
  }
  probe_o_831FD318(ctx, base);
  if (log) {
    PROBE_LOG(stderr, "V16-EXIT ret=%08X\n", ctx.r3.u32);
  }
}
REX_IMPORT(__imp__sub_82C43198, probe_o_82C43198, void());
extern "C" void sub_82C43198(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 8;
  if (log) {
    ++n;
    PROBE_LOG(stderr, "V20-ENTER r3=%08X lr=%08X\n", ctx.r3.u32,
                 (uint32_t)ctx.lr);
  } else if ((uint32_t)ctx.lr == 0x823787F4) {
    PROBE_LOG(stderr, "V20-GATE r3=%08X\n", ctx.r3.u32);
  }
  probe_o_82C43198(ctx, base);
  if (log) {
    PROBE_LOG(stderr, "V20-EXIT ret=%08X\n", ctx.r3.u32);
  }
}
REX_IMPORT(__imp__sub_82378868, probe_o_82378868, void());
extern "C" void sub_82378868(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 8;
  if (log) {
    ++n;
    PROBE_LOG(stderr, "V24-ENTER r3=%08X lr=%08X\n", ctx.r3.u32,
                 (uint32_t)ctx.lr);
  } else if ((uint32_t)ctx.lr == 0x823787F4) {
    PROBE_LOG(stderr, "V24-GATE r3=%08X\n", ctx.r3.u32);
  }
  probe_o_82378868(ctx, base);
  if (log) {
    PROBE_LOG(stderr, "V24-EXIT ret=%08X\n", ctx.r3.u32);
  }
}
REX_IMPORT(__imp__sub_829CE870, probe_o_829CE870, void());
extern "C" void sub_829CE870(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 8;
  if (log) {
    ++n;
    PROBE_LOG(stderr, "V28-ENTER r3=%08X lr=%08X\n", ctx.r3.u32,
                 (uint32_t)ctx.lr);
  } else if ((uint32_t)ctx.lr == 0x823787F4) {
    PROBE_LOG(stderr, "V28-GATE r3=%08X\n", ctx.r3.u32);
  }
  probe_o_829CE870(ctx, base);
  if (log) {
    PROBE_LOG(stderr, "V28-EXIT ret=%08X\n", ctx.r3.u32);
  }
}

REX_IMPORT(__imp__sub_82C63098, probe_o_82C63098, void());
extern "C" void sub_82C63098(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) {
    ++n;
    PROBE_LOG(stderr, "ENTRY3098-ENTER r3=%08X\n", ctx.r3.u32);
  }
  probe_o_82C63098(ctx, base);
  if (log) {
    PROBE_LOG(stderr, "ENTRY3098-EXIT r3=%08X\n", ctx.r3.u32);
  }
}
REX_IMPORT(__imp__sub_8236CED8, probe_o_8236CED8, void());
extern "C" void sub_8236CED8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) {
    ++n;
    PROBE_LOG(stderr, "ENTRYCED8-ENTER\n");
  }
  probe_o_8236CED8(ctx, base);
  if (log) {
    PROBE_LOG(stderr, "ENTRYCED8-EXIT\n");
  }
}

REX_IMPORT(__imp__sub_8236CB48, probe_o_8236CB48, void());
extern "C" void sub_8236CB48(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    uint32_t a3 = ctx.r3.u32;
    uint32_t lk = 0;
    int32_t lc = 0;
    uint32_t ow = 0;
    if (a3 >= 0x10000) {
      uint32_t b = 0;
      std::memcpy(&b, base + a3 + 4, 4);
      lk = __builtin_bswap32(b);
      if (lk >= 0x10000) {
        std::memcpy(&lc, base + lk + 0x10, 4);
        uint32_t beow = 0;
        std::memcpy(&beow, base + lk + 0x18, 4);
        ow = __builtin_bswap32(beow);
      }
    }
    PROBE_LOG(stderr, "CB48-ENTER r3=%08X lock=%08X count=%d owner=%08X lwp=%d\n",
                 a3, lk, lc, ow, gettid());
  }
  probe_o_8236CB48(ctx, base);
  if (log) {
    PROBE_LOG(stderr, "CB48-EXIT\n");
  }
}
REX_IMPORT(__imp__sub_82B68FC8, probe_o_82B68FC8, void());
extern "C" void sub_82B68FC8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    PROBE_LOG(stderr, "B68FC8-ENTER\n");
  }
  probe_o_82B68FC8(ctx, base);
  if (log) {
    PROBE_LOG(stderr, "B68FC8-EXIT\n");
  }
}
REX_IMPORT(__imp__sub_82A3B890, probe_o_82A3B890, void());
extern "C" void sub_82A3B890(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    PROBE_LOG(stderr, "A3B890-ENTER\n");
  }
  probe_o_82A3B890(ctx, base);
  if (log) {
    PROBE_LOG(stderr, "A3B890-EXIT\n");
  }
}
REX_IMPORT(__imp__sub_82A3BB78, probe_o_82A3BB78, void());
extern "C" void sub_82A3BB78(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    PROBE_LOG(stderr, "A3BB78-ENTER\n");
  }
  probe_o_82A3BB78(ctx, base);
  if (log) {
    PROBE_LOG(stderr, "A3BB78-EXIT\n");
  }
}

REX_IMPORT(__imp__sub_82378FA0, probe_o_82378FA0, void());
extern "C" void sub_82378FA0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; PROBE_LOG(stderr, "B78FA0-ENTER\n"); }
  probe_o_82378FA0(ctx, base);
  if (log) { PROBE_LOG(stderr, "B78FA0-EXIT\n"); }
}
REX_IMPORT(__imp__sub_823FE988, probe_o_823FE988, void());
extern "C" void sub_823FE988(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; PROBE_LOG(stderr, "BFE988-ENTER\n"); }
  probe_o_823FE988(ctx, base);
  if (log) { PROBE_LOG(stderr, "BFE988-EXIT\n"); }
}
REX_IMPORT(__imp__sub_823FECA0, probe_o_823FECA0, void());
extern "C" void sub_823FECA0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; PROBE_LOG(stderr, "BFECA0-ENTER\n"); }
  probe_o_823FECA0(ctx, base);
  if (log) { PROBE_LOG(stderr, "BFECA0-EXIT\n"); }
}

REX_IMPORT(__imp__sub_823FE828, probe_o_823FE828, void());
extern "C" void sub_823FE828(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; PROBE_LOG(stderr, "FE828-ENTER\n"); }
  probe_o_823FE828(ctx, base);
  if (log) { PROBE_LOG(stderr, "FE828-EXIT\n"); }
}
REX_IMPORT(__imp__sub_823F89B0, probe_o_823F89B0, void());
extern "C" void sub_823F89B0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; PROBE_LOG(stderr, "F89B0-ENTER\n"); }
  probe_o_823F89B0(ctx, base);
  if (log) { PROBE_LOG(stderr, "F89B0-EXIT\n"); }
}
REX_IMPORT(__imp__sub_826CAFA8, probe_o_826CAFA8, void());
extern "C" void sub_826CAFA8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; PROBE_LOG(stderr, "AFA8-ENTER\n"); }
  probe_o_826CAFA8(ctx, base);
  if (log) { PROBE_LOG(stderr, "AFA8-EXIT\n"); }
}

REX_IMPORT(__imp__sub_821E2CC8, probe_o_821E2CC8, void());
extern "C" void sub_821E2CC8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "E2CC8-ENTER\n"); }
  probe_o_821E2CC8(ctx, base);
  if (log) { PROBE_LOG(stderr, "E2CC8-EXIT\n"); }
}
REX_IMPORT(__imp__sub_82B3AEA0, probe_o_82B3AEA0, void());
extern "C" void sub_82B3AEA0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "B3AEA0-ENTER\n"); }
  probe_o_82B3AEA0(ctx, base);
  if (log) { PROBE_LOG(stderr, "B3AEA0-EXIT\n"); }
}
REX_IMPORT(__imp__sub_82C63FB8, probe_o_82C63FB8, void());
extern "C" void sub_82C63FB8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "C63FB8-ENTER\n"); }
  probe_o_82C63FB8(ctx, base);
  if (log) { PROBE_LOG(stderr, "C63FB8-EXIT\n"); }
}
REX_IMPORT(__imp__sub_82C62DE8, probe_o_82C62DE8, void());
extern "C" void sub_82C62DE8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "C62DE8-ENTER\n"); }
  probe_o_82C62DE8(ctx, base);
  if (log) { PROBE_LOG(stderr, "C62DE8-EXIT\n"); }
}
REX_IMPORT(__imp__sub_82C62A08, probe_o_82C62A08, void());
extern "C" void sub_82C62A08(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "C62A08-ENTER\n"); }
  probe_o_82C62A08(ctx, base);
  if (log) { PROBE_LOG(stderr, "C62A08-EXIT\n"); }
}
REX_IMPORT(__imp__sub_82214F08, probe_o_82214F08, void());
extern "C" void sub_82214F08(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "214F08-ENTER\n"); }
  probe_o_82214F08(ctx, base);
  if (log) { PROBE_LOG(stderr, "214F08-EXIT\n"); }
}

REX_IMPORT(__imp__sub_82A43728, probe_o_82A43728, void());
extern "C" void sub_82A43728(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "A43728-ENTER\n"); }
  probe_o_82A43728(ctx, base);
  if (log) { PROBE_LOG(stderr, "A43728-EXIT\n"); }
}

REX_IMPORT(__imp__sub_82378420, probe_o_82378420, void());
extern "C" void sub_82378420(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "378420-ENTER\n"); }
  probe_o_82378420(ctx, base);
  if (log) { PROBE_LOG(stderr, "378420-EXIT\n"); }
}

REX_IMPORT(__imp__sub_82CBD098, probe_o_82CBD098, void());
extern "C" void sub_82CBD098(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "CBD098-ENTER\n"); }
  probe_o_82CBD098(ctx, base);
  if (log) { PROBE_LOG(stderr, "CBD098-EXIT\n"); }
}
REX_IMPORT(__imp__sub_8231E908, probe_o_8231E908, void());
extern "C" void sub_8231E908(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "31E908-ENTER\n"); }
  probe_o_8231E908(ctx, base);
  if (log) { PROBE_LOG(stderr, "31E908-EXIT\n"); }
}

REX_IMPORT(__imp__sub_82356180, probe_o_82356180, void());
extern "C" void sub_82356180(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "356180-ENTER\n"); }
  probe_o_82356180(ctx, base);
  if (log) { PROBE_LOG(stderr, "356180-EXIT r3=%08X\n", ctx.r3.u32); }
}

REX_IMPORT(__imp__sub_82354980, probe_o_82354980, void());
extern "C" void sub_82354980(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; PROBE_LOG(stderr, "354980-ENTER\n"); }
  probe_o_82354980(ctx, base);
  if (log) { PROBE_LOG(stderr, "354980-EXIT\n"); }
}

REX_IMPORT(__imp__sub_825BAFC0, probe_o_825BAFC0, void());
extern "C" void sub_825BAFC0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 8;
  if (log) { ++n; PROBE_LOG(stderr, "5BAFC0-ENTER lr=%08X\n", (uint32_t)ctx.lr); }
  probe_o_825BAFC0(ctx, base);
  if (log) { PROBE_LOG(stderr, "5BAFC0-EXIT\n"); }
}

// TEST (reversible): post-populate sequencer probes (first-hit + passthrough).
// Populate (822F2608) returns into sub_822EA928's shared restore+blr epilogue
// (82CA2C38/3C, not hookable: no __imp symbol), so the sequencer's next known
// calls are probed instead. Static fan-in (guest-image.bin bl scan):
// - 82CBB638: sole caller 822EA8D4 (first call inside the 822EA8C0 branch) =>
//   entry fires iff the sequencer branch runs its body.
// - 82CA97B8: sole caller 82CBBB10 (mr r30,r3; bl right after 822EA8C0
//   returns, LR=82CBBB0C) => entry fires iff the branch RETURNED and
//   sub_82CBB788 advanced past it.
// - 82CBB788: head of the 822EA8C0 caller chain ([82CBB788,82CBB964]); only
//   static caller is its own guarded recursion (82CBB9B0), so first entry
//   arrives indirectly and entry LR reveals the driver.
// Interleave with 822F2608 populate-dispatch #n + 822EA8C0-ENTER/EXIT counters
// to read the post-populate order off the log. Rejected: 82CBBF60 (91 direct
// callers), 821E6388 (105), 82CA34B0 (8) — shared utilities whose first hit
// would fire from unrelated paths; 832B26CC/832B230C (import thunks, no
// DEFINE_REX_FUNC).
REX_IMPORT(__imp__sub_82CBB638, probe_o_82CBB638, void());
extern "C" void sub_82CBB638(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 82CBB638 branch-entry lr=%08X\n",
                 (uint32_t)ctx.lr);
  }
  probe_o_82CBB638(ctx, base);
  static bool done = false;
  if (!done) {
    done = true;
    PROBE_LOG(stderr, "PROBE-HIT 82CBB638-EXIT returned\n");
  }
}
REX_IMPORT(__imp__sub_82CA97B8, probe_o_82CA97B8, void());
extern "C" void sub_82CA97B8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 82CA97B8 post-branch lr=%08X\n",
                 (uint32_t)ctx.lr);
  }
  probe_o_82CA97B8(ctx, base);
  static bool done = false;
  if (!done) {
    done = true;
    PROBE_LOG(stderr, "PROBE-HIT 82CA97B8-EXIT returned\n");
  }
}
REX_IMPORT(__imp__sub_82CBB788, probe_o_82CBB788, void());
extern "C" void sub_82CBB788(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    PROBE_LOG(stderr, "PROBE-HIT 82CBB788 chain-head lr=%08X\n",
                 (uint32_t)ctx.lr);
  }
  probe_o_82CBB788(ctx, base);
  static bool done = false;
  if (!done) {
    done = true;
    PROBE_LOG(stderr, "PROBE-HIT 82CBB788-EXIT returned\n");
  }
}

// TEST: drain-wait exit gate. `823784A0`'s loop (0x823787FC) exits when
// the virtual [[0x8349E6EC]+16] returns low-byte 0; runtime resolution
// (GATETGT) gives target `82185418`. First-8 + gate-site always-log.
REX_IMPORT(__imp__sub_82185418, probe_o_82185418, void());
extern "C" void sub_82185418(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  ++n;
  uint32_t lr = (uint32_t)ctx.lr;
  if (n <= 8) {
    PROBE_LOG(stderr, "GATE16-ENTER #%u r3=%08X lr=%08X\n", n,
                 ctx.r3.u32, lr);
  } else if (lr == 0x823787F4) {
    PROBE_LOG(stderr, "GATE16-GATE r3=%08X\n", ctx.r3.u32);
  }
  probe_o_82185418(ctx, base);
  if (n <= 8) {
    PROBE_LOG(stderr, "GATE16-EXIT #%u ret=%08X\n", n, ctx.r3.u32);
  } else if (lr == 0x823787F4) {
    PROBE_LOG(stderr, "GATE16-GATE-RET ret=%08X\n", ctx.r3.u32);
  }
}

// TEST: gate-object family. The drain-wait gate object (vtable
// 0x82002838, instance 0x4011F250) waits on [+44]!=0. Its vtable
// siblings may touch the object (r3==gateobj?) or produce the event.
// First-hit lr-logging + passthrough only.
#define GATEFAM(addr) \
  REX_IMPORT(__imp__sub_##addr, probe_o_##addr, void()); \
  extern "C" void sub_##addr(PPCContext& ctx, uint8_t* base) { \
    static bool logged = false; \
    if (!logged) { \
      logged = true; \
      PROBE_LOG(stderr, "PROBE-HIT " #addr " gatefam r3=%08X lr=%08X\n", \
                   ctx.r3.u32, (uint32_t)ctx.lr); \
    } \
    probe_o_##addr(ctx, base); \
  }
GATEFAM(82A14BE8)
GATEFAM(82A14CA0)
GATEFAM(82A14E20)
GATEFAM(82A14C48)
GATEFAM(82A14E10)
GATEFAM(822C1BC8)
GATEFAM(822CEEF8)
GATEFAM(82172ED0)
GATEFAM(821CCB20)
GATEFAM(821961A0)
GATEFAM(8236D0A8)
GATEFAM(8229A9D8)
