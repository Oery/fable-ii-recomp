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
// guest registers). Determines which boot phases execute at all.
#include <atomic>
#include <cstdio>
#include <mutex>
#include <rex/hook.h>
// TEST (reversible): serialize bank table access. `82BCD7B0` faults on
// `[r9+r11]` while fill workers (`82BCA340`/`82BC9E10`/`82BC9EA0`) produce
// the same table; entry snapshots look valid but fault-time values differ,
// implicating a race. A host mutex across all four diagnoses it: if faults
// vanish, the race is confirmed and the game's own missing lock (or a
// ReXGlue timing artifact) is the root cause. All four take the mutex in
static std::mutex bank_table_mutex;
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
    std::fprintf(stderr, "PROBE-HIT 8236C940 populate-caller\n");
  }
  probe_orig_8236C940(ctx, base);
}
REX_IMPORT(__imp__sub_82A47D48, probe_orig_82A47D48, void());
extern "C" void sub_82A47D48(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 82A47D48 G4-populate\n");
  }
  probe_orig_82A47D48(ctx, base);
  static bool done = false;
  if (!done) {
    done = true;
    std::fprintf(stderr, "PROBE-HIT 82A47D48-EXIT returned\n");
  }
}
REX_IMPORT(__imp__sub_82AA8AD0, probe_o_82AA8AD0, void());
extern "C" void sub_82AA8AD0(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 82AA8AD0 G4-sub\n");
  }
  probe_o_82AA8AD0(ctx, base);
}
REX_IMPORT(__imp__sub_82CBB620, probe_o_82CBB620, void());
extern "C" void sub_82CBB620(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 82CBB620 G4-sub2\n");
  }
  probe_o_82CBB620(ctx, base);
}
REX_IMPORT(__imp__sub_822EA928, probe_orig_822EA928, void());
extern "C" void sub_822EA928(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 822EA928 park-sequencer\n");
  }
  probe_orig_822EA928(ctx, base);
}
REX_IMPORT(__imp__sub_822F4690, probe_orig_822F4690, void());
extern "C" void sub_822F4690(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 822F4690 quit-setter\n");
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
      std::fprintf(stderr, "THREADMAP gid=%08X lwp=%d\n", gid, gettid());
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
        std::fprintf(stderr, "LOCKW lr=%08X lock=%08X count=%d owner=%08X\n",
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
      std::fprintf(stderr, "GT2-ENTER lr=%08X lock=%08X count=%d owner=%08X\n",
                   lr, lk, lc, ow);
    }
  }
  probe_orig_82200688(ctx, base);
  if (lr == 0x822F3664 || lr == 0x82378834) {
    static unsigned ngt2x = 0;
    if (ngt2x < 8) {
      ++ngt2x;
      std::fprintf(stderr, "GT2-EXIT lr=%08X\n", lr);
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
    std::fprintf(stderr, "PRESEED-G4 node=%08X\n", node);
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
    std::fprintf(stderr, "PRESEED-7CC node=%08X\n", node2);
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
    std::fprintf(stderr, "PRESEED-7B4 node=%08X\n", node3);
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
    std::fprintf(stderr, "PROBE-7BB58-CALLER lr=%08X r28=%08X tgt=%08X\n", lr,
                 r28, tgt);
  }
  if (n < 12) {
    std::fprintf(stderr, "PROBE-7BB58 r28=%08X r31=%08X tgt=%08X\n", r28, r31,
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
        std::fprintf(stderr, "PASS-NULL 8227BB58\n");
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
  std::fprintf(stderr, "PROBE-HIT 822F2608 populate-dispatch #%u lr=%08X\n",
               ++n, (uint32_t)ctx.lr);
  probe_o_822F2608(ctx, base);
}
// TEST: trace the sequencer branch containing `822EAA8C` (populate
// caller): entry LR identifies who drives populate; exit shows the
REX_IMPORT(__imp__sub_822EA8C0, probe_o_822EA8C0, void());
extern "C" void sub_822EA8C0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  if (++n <= 4) {
    std::fprintf(stderr, "PROBE-HIT 822EA8C0-ENTER #%u lr=%08X r3=%08X\n",
                 n, (uint32_t)ctx.lr, ctx.r3.u32);
  }
  probe_o_822EA8C0(ctx, base);
  static unsigned nx = 0;
  if (++nx <= 4) {
    std::fprintf(stderr, "PROBE-HIT 822EA8C0-EXIT #%u\n", nx);
  }
}
REX_IMPORT(__imp__sub_82378BB8, probe_o_82378BB8, void());
extern "C" void sub_82378BB8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 82378BB8 populate-branch\n");
  }
  probe_o_82378BB8(ctx, base);
}
static uint32_t drain_flag_addr = 0;
static uint32_t drain_item_addr = 0;
REX_IMPORT(__imp__sub_82B67950, probe_o_82B67950, void());
extern "C" void sub_82B67950(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 82B67950 streamer-poll\n");
  }
  if (drain_flag_addr >= 0x10000) {
    static uint8_t last = 0xFF;
    static unsigned long n = 0;
    static bool synthed = false;
    static long long t0 = 0;
    uint8_t cur = *(base + drain_flag_addr);
    if (++n == 1 || cur != last) {
      last = cur;
      std::fprintf(stderr, "FLAGWATCH %02X\n", cur);
    }
    // TEST (reversible): synthesize the missing completion once. The drain
    // flag never clears (no worker owns the item) and GameThread parks on
    // it while populate (its own next step) never runs. 90 s after first
    // sight with the flag still set, clear it once: if GameThread advances
    // to populate/menu, the flag was the sole blocker; if something
    // re-sets it, a legit producer is just slow; on corruption, revert.
    if (!synthed) {
      if (t0 == 0) {
        t0 = (long long)time(nullptr);
      }
      if (cur != 0 && (long long)time(nullptr) - t0 > 90) {
        synthed = true;
        *(base + drain_flag_addr) = 0;
        if (drain_item_addr >= 0x10000) {
          *(base + drain_item_addr + 0x88) = 0;
        }
        std::fprintf(stderr, "FLAGSYNTH cleared %08X item=%08X\n",
                     drain_flag_addr, drain_item_addr);
      }
    }
  }
  probe_o_82B67950(ctx, base);
}
REX_IMPORT(__imp__sub_8236C360, probe_o_8236C360, void());
extern "C" void sub_8236C360(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 8236C360 3d-proc\n");
  }
  probe_o_8236C360(ctx, base);
}
REX_IMPORT(__imp__sub_822F33B8, probe_o_822F33B8, void());
extern "C" void sub_822F33B8(PPCContext& ctx, uint8_t* base) {
  static int n = 0;
  std::fprintf(stderr, "PROBE-HIT 822F33B8 bank-proc #%d\n", ++n);
  probe_o_822F33B8(ctx, base);
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
    std::fprintf(stderr, "DRAIN-ITEM ctx=%08X item=%08X vt=%08X t12=%08X\n", c,
                 i0, vt, t12);
  }
  std::fprintf(stderr, "PROBE-47F8-ENTER\n");
  probe_o_822F47F8(ctx, base);
  gt_drain_done.store(true);
  std::fprintf(stderr, "PROBE-47F8-EXIT\n");
}
REX_IMPORT(__imp__sub_823781A8, probe_o_823781A8, void());
extern "C" void sub_823781A8(PPCContext& ctx, uint8_t* base) {
  std::fprintf(stderr, "PROBE-781A8-ENTER\n");
  probe_o_823781A8(ctx, base);
  std::fprintf(stderr, "PROBE-781A8-EXIT\n");
}

REX_IMPORT(__imp__sub_822F5540, probe_o_822F5540, void());
extern "C" void sub_822F5540(PPCContext& ctx, uint8_t* base) {
  std::fprintf(stderr, "PROBE-5540-ENTER\n");
  probe_o_822F5540(ctx, base);
  std::fprintf(stderr, "PROBE-5540-EXIT\n");
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
    std::fprintf(stderr, "SLEEPBYPASS arg=%u lr=%08X\n", ctx.r3.u32, lr);
  }
  if (lr == 0x82378828) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      std::fprintf(stderr, "SKIP-GTSLEEP\n");
    }
    ctx.r3.u32 = 0;
    return;
  }
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
    std::fprintf(stderr, "PROBE-HIT 829FF648-TAIL #%u lr=%08X\n", n, lr);
  } else if (nlr < 12) {
    ++nlr;
    std::fprintf(stderr, "PROBE-HIT 829FF648 #%u lr=%08X\n", n, lr);
  } else if (g_pop_active) {
    std::fprintf(stderr, "PROBE-HIT 829FF648-POP #%u lr=%08X\n", n, lr);
  } else if ((n % 4) == 1) {
    std::fprintf(stderr, "PROBE-HIT 829FF648 seq-step #%u\n", n);
}
  probe_o_829FF648(ctx, base);
}
REX_IMPORT(__imp__sub_822F5718, probe_o_822F5718, void());
extern "C" void sub_822F5718(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  if ((++n % 4) == 1) {
    std::fprintf(stderr, "PROBE-HIT 822F5718 poptail #%u\n", n);
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
  std::fprintf(stderr, "VTABLE+0 #%u slot=%08X obj=%08X vt=%08X tgt=%08X\n",
               n, slot, obj, vt, tgt);
  static bool done = false;
  if (!done) {
    done = true;
    std::fprintf(stderr, "PROBE-HIT 822F5718-EXIT returned\n");
  }
}
REX_IMPORT(__imp__sub_822F71A8, probe_o_822F71A8, void());
extern "C" void sub_822F71A8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 822F71A8 poptail-sub\n");
  }
  probe_o_822F71A8(ctx, base);
  static bool done = false;
  if (!done) {
    done = true;
    std::fprintf(stderr, "PROBE-HIT 822F71A8-EXIT returned\n");
  }
}
REX_IMPORT(__imp__sub_83231BE8, probe_o_83231BE8, void());
extern "C" void sub_83231BE8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 83231BE8 refcheck\n");
  }
  probe_o_83231BE8(ctx, base);
  static bool done = false;
  if (!done) {
    done = true;
    std::fprintf(stderr, "PROBE-HIT 83231BE8-EXIT returned\n");
  }
}
REX_IMPORT(__imp__sub_82356698, probe_o_82356698, void());
extern "C" void sub_82356698(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  if ((++n % 2) == 1) {
    std::fprintf(stderr, "PROBE-HIT 82356698 poptail2 #%u\n", n);
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
  std::fprintf(stderr, "PROBE-HIT 82356698-EXIT #%u lr=%08X\n", ++nx, exlr);
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
    std::fprintf(stderr, "VTAIL+%u off=%u slot=%08X obj=%08X vt=%08X tgt=%08X\n",
                 chain[i], off[i], slot, obj, vt, tgt);
  }
}
REX_IMPORT(__imp__sub_8217E3F8, probe_o_8217E3F8, void());
extern "C" void sub_8217E3F8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 8217E3F8 seq-step\n");
  }
  probe_o_8217E3F8(ctx, base);
}
REX_IMPORT(__imp__sub_822EB0C8, probe_o_822EB0C8, void());
extern "C" void sub_822EB0C8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 822EB0C8 seq-step\n");
  }
  probe_o_822EB0C8(ctx, base);
}
REX_IMPORT(__imp__sub_822F2518, probe_o_822F2518, void());
extern "C" void sub_822F2518(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 822F2518 seq-step\n");
  }
  probe_o_822F2518(ctx, base);
}

REX_IMPORT(__imp__sub_82B68F60, probe_o_82B68F60, void());
extern "C" void sub_82B68F60(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 82B68F60 postlock\n");
  }
  probe_o_82B68F60(ctx, base);
}
REX_IMPORT(__imp__sub_8236C9F8, probe_o_8236C9F8, void());
extern "C" void sub_8236C9F8(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 8236C9F8 postlock\n");
  }
  probe_o_8236C9F8(ctx, base);
}

REX_IMPORT(__imp__sub_822F0518, probe_o_822F0518, void());
extern "C" void sub_822F0518(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 822F0518 deep\n");
  }
  probe_o_822F0518(ctx, base);
}
REX_IMPORT(__imp__sub_82309F00, probe_o_82309F00, void());
extern "C" void sub_82309F00(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 82309F00 deep\n");
  }
  probe_o_82309F00(ctx, base);
}
REX_IMPORT(__imp__sub_825BB2C0, probe_o_825BB2C0, void());
extern "C" void sub_825BB2C0(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 825BB2C0 deep\n");
  }
  probe_o_825BB2C0(ctx, base);
}
REX_IMPORT(__imp__sub_8217DA50, probe_o_8217DA50, void());
extern "C" void sub_8217DA50(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 8217DA50 deep\n");
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
  std::fprintf(stderr, "DRAIN-VERDICT %u\n", v);
}

REX_IMPORT(__imp__sub_8236CC90, probe_o_8236CC90, void());
extern "C" void sub_8236CC90(PPCContext& ctx, uint8_t* base) {
  std::fprintf(stderr, "PROBE-CC90-ENTER\n");
  probe_o_8236CC90(ctx, base);
  std::fprintf(stderr, "PROBE-CC90-EXIT\n");
}
REX_IMPORT(__imp__sub_8236CA90, probe_o_8236CA90, void());
extern "C" void sub_8236CA90(PPCContext& ctx, uint8_t* base) {
  static bool logged = false;
  if (!logged) {
    logged = true;
    std::fprintf(stderr, "PROBE-HIT 8236CA90 flagwait\n");
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
    std::fprintf(stderr, "SLOWALLOC size=%u ms=%lld lr=%08X out=%08X\n", sz,
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
    std::fprintf(stderr, "POOL-STATE pool=%08X f0=%08X f4=%08X slot=%08X head=%08X next=%08X b16=%08X chunks=%08X size=%u\n",
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
    std::fprintf(stderr, "ALLOC-RET 8240DAA8 size=%u out=%08X\n", r31, out);
  }
}
REX_IMPORT(__imp__sub_823052C0, probe_o_823052C0, void());
extern "C" void sub_823052C0(PPCContext& ctx, uint8_t* base) {
  probe_o_823052C0(ctx, base);
  uint32_t gaddr = (uint32_t)((int32_t)-2092367872 + 27088);
  uint32_t be = 0;
  std::memcpy(&be, base + gaddr, 4);
  std::fprintf(stderr, "POOL-GLOBAL addr=%08X val=%08X\n", gaddr,
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
    std::fprintf(stderr, "BANK-DUMP r29=%08X r6=%08X\n", r29, r6);
    for (int off = 0; off < 64; off += 16) {
      uint32_t w[4] = {0, 0, 0, 0};
      std::memcpy(w, base + r6 + off, 16);
      std::fprintf(stderr, "  tbl+%02X: %08X %08X %08X %08X\n", off,
                   __builtin_bswap32(w[0]), __builtin_bswap32(w[1]),
                   __builtin_bswap32(w[2]), __builtin_bswap32(w[3]));
    }
    for (int off = 0; off < 64; off += 16) {
      uint32_t w[4] = {0, 0, 0, 0};
      std::memcpy(w, base + r29 + off, 16);
      std::fprintf(stderr, "  obj+%02X: %08X %08X %08X %08X\n", off,
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
    std::fprintf(stderr,
                 "BANK-TBL r29=%08X r30=%08X r6=%08X base=%08X cnt=%08X mask=%08X tgt=%08X\n",
                 r29, r30, r6, t0, cnt, mask, tgt);
  }
  if (r29 >= 0x10000 && r6 >= 0x10000 && r6 != seen_r6) {
    seen_r6 = r6;
    std::fprintf(stderr, "BANK-TBL-SEEN r6=%08X base=%08X cnt=%08X\n", r6, t0,
                 cnt);
  }
  static uint32_t seen_key = 0;
  uint32_t key = r30 ^ (r30 >> 4);
  if (r29 >= 0x10000 && key != seen_key) {
    seen_key = key;
    std::fprintf(stderr, "BANK-R30 r30=%08X r6=%08X r30+16=%08X\n", r30, r6,
                 r30 + 16);
  }
  std::lock_guard<std::mutex> lk(bank_table_mutex);
  probe_o_82BCD7B0(ctx, base);
}

REX_IMPORT(__imp__sub_82BCA340, probe_o_82BCA340, void());
extern "C" void sub_82BCA340(PPCContext& ctx, uint8_t* base) {
  std::lock_guard<std::mutex> lk(bank_table_mutex);
  probe_o_82BCA340(ctx, base);
}
REX_IMPORT(__imp__sub_82BC9E10, probe_o_82BC9E10, void());
extern "C" void sub_82BC9E10(PPCContext& ctx, uint8_t* base) {
  std::lock_guard<std::mutex> lk(bank_table_mutex);
  probe_o_82BC9E10(ctx, base);
}
REX_IMPORT(__imp__sub_82BC9EA0, probe_o_82BC9EA0, void());
extern "C" void sub_82BC9EA0(PPCContext& ctx, uint8_t* base) {
  std::lock_guard<std::mutex> lk(bank_table_mutex);
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
    std::fprintf(stderr, "POOL-GROW #%lu slot=%08X out=%08X\n", ++n, in, out);
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
    std::fprintf(stderr, "FLAGWAIT obj=%08X flag=%02X lr=%08X\n", o, f,
                 (uint32_t)ctx.lr);
  }
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
      std::fprintf(stderr, "SKIP-NULLWAIT lr=%08X\n", (uint32_t)ctx.lr);
    }
    ctx.r3.u32 = 0;
    return;
  }
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
    std::fprintf(stderr, "DRAINVIRT r3=%08X r4=%08X [r4]=%02X lr=%08X\n", a3,
                 a4, f, (uint32_t)ctx.lr);
    if (nn == 1) {
      drain_flag_addr = a4;
      drain_item_addr = a3;
      if (a3 >= 0x10000) {
        for (int off = 0; off < 320; off += 32) {
          uint32_t w[8] = {0, 0, 0, 0, 0, 0, 0, 0};
          std::memcpy(w, base + a3 + off, 32);
          std::fprintf(stderr, "  item+%03X: %08X %08X %08X %08X %08X %08X %08X %08X\n",
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
    std::fprintf(stderr, "VIRTTGT vt=%08X [16]=%08X [20]=%08X [24]=%08X [28]=%08X\n",
                 vt, s16, s20, s24, s28);
  }
  probe_o_823784A0(ctx, base);
  static unsigned nret = 0;
  if (nret < 20) {
    ++nret;
    std::fprintf(stderr, "VIRTRET r3=%08X\n", ctx.r3.u32);
  }
}

REX_IMPORT(__imp__sub_82477768, probe_o_82477768, void());
extern "C" void sub_82477768(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  if (n < 8) {
    ++n;
    std::fprintf(stderr, "CLEARER r3=%08X r4=%08X lr=%08X\n", ctx.r3.u32,
                 ctx.r4.u32, (uint32_t)ctx.lr);
  }
  probe_o_82477768(ctx, base);
}

REX_IMPORT(__imp__sub_82B9B8D8, probe_o_82B9B8D8, void());
extern "C" void sub_82B9B8D8(PPCContext& ctx, uint8_t* base) {
  static unsigned long n = 0;
  if (++n <= 5 || n % 1000 == 0) {
    std::fprintf(stderr, "GPU-IRQ #%lu\n", n);
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
    std::fprintf(stderr, "TEARDOWN-RACE 82378B10 zeroes global+26920\n");
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
    std::fprintf(stderr, "GTLOCK global=%08X o11=%08X r31=%08X lock=%08X count=%d owner=%08X\n",
                 gl, o11, r31, lk, lc, ow);
  }
  probe_o_822F3640(ctx, base);
}

REX_IMPORT(__imp__sub_8221EB58, probe_o_8221EB58, void());
extern "C" void sub_8221EB58(PPCContext& ctx, uint8_t* base) {
  static unsigned long n = 0;
  probe_o_8221EB58(ctx, base);
  if (++n == 1 || n == 200000) {
    std::fprintf(stderr, "GUESTTICK #%lu r3=%08X r4=%08X\n", n, ctx.r3.u32,
                 ctx.r4.u32);
  }
}

REX_IMPORT(__imp__sub_8236CC28, probe_o_8236CC28, void());
extern "C" void sub_8236CC28(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    std::fprintf(stderr, "PROBE-CC28-ENTER\n");
  }
  probe_o_8236CC28(ctx, base);
  if (log) {
    std::fprintf(stderr, "PROBE-CC28-EXIT\n");
  }
}

REX_IMPORT(__imp__sub_831FD318, probe_o_831FD318, void());
extern "C" void sub_831FD318(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    std::fprintf(stderr, "V16-ENTER\n");
  }
  probe_o_831FD318(ctx, base);
  if (log) {
    std::fprintf(stderr, "V16-EXIT\n");
  }
}
REX_IMPORT(__imp__sub_82C43198, probe_o_82C43198, void());
extern "C" void sub_82C43198(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    std::fprintf(stderr, "V20-ENTER\n");
  }
  probe_o_82C43198(ctx, base);
  if (log) {
    std::fprintf(stderr, "V20-EXIT\n");
  }
}
REX_IMPORT(__imp__sub_82378868, probe_o_82378868, void());
extern "C" void sub_82378868(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    std::fprintf(stderr, "V24-ENTER\n");
  }
  probe_o_82378868(ctx, base);
  if (log) {
    std::fprintf(stderr, "V24-EXIT\n");
  }
}
REX_IMPORT(__imp__sub_829CE870, probe_o_829CE870, void());
extern "C" void sub_829CE870(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    std::fprintf(stderr, "V28-ENTER\n");
  }
  probe_o_829CE870(ctx, base);
  if (log) {
    std::fprintf(stderr, "V28-EXIT\n");
  }
}

REX_IMPORT(__imp__sub_82C63098, probe_o_82C63098, void());
extern "C" void sub_82C63098(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) {
    ++n;
    std::fprintf(stderr, "ENTRY3098-ENTER r3=%08X\n", ctx.r3.u32);
  }
  probe_o_82C63098(ctx, base);
  if (log) {
    std::fprintf(stderr, "ENTRY3098-EXIT r3=%08X\n", ctx.r3.u32);
  }
}
REX_IMPORT(__imp__sub_8236CED8, probe_o_8236CED8, void());
extern "C" void sub_8236CED8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) {
    ++n;
    std::fprintf(stderr, "ENTRYCED8-ENTER\n");
  }
  probe_o_8236CED8(ctx, base);
  if (log) {
    std::fprintf(stderr, "ENTRYCED8-EXIT\n");
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
    std::fprintf(stderr, "CB48-ENTER r3=%08X lock=%08X count=%d owner=%08X lwp=%d\n",
                 a3, lk, lc, ow, gettid());
  }
  probe_o_8236CB48(ctx, base);
  if (log) {
    std::fprintf(stderr, "CB48-EXIT\n");
  }
}
REX_IMPORT(__imp__sub_82B68FC8, probe_o_82B68FC8, void());
extern "C" void sub_82B68FC8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    std::fprintf(stderr, "B68FC8-ENTER\n");
  }
  probe_o_82B68FC8(ctx, base);
  if (log) {
    std::fprintf(stderr, "B68FC8-EXIT\n");
  }
}
REX_IMPORT(__imp__sub_82A3B890, probe_o_82A3B890, void());
extern "C" void sub_82A3B890(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    std::fprintf(stderr, "A3B890-ENTER\n");
  }
  probe_o_82A3B890(ctx, base);
  if (log) {
    std::fprintf(stderr, "A3B890-EXIT\n");
  }
}
REX_IMPORT(__imp__sub_82A3BB78, probe_o_82A3BB78, void());
extern "C" void sub_82A3BB78(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) {
    ++n;
    std::fprintf(stderr, "A3BB78-ENTER\n");
  }
  probe_o_82A3BB78(ctx, base);
  if (log) {
    std::fprintf(stderr, "A3BB78-EXIT\n");
  }
}

REX_IMPORT(__imp__sub_82378FA0, probe_o_82378FA0, void());
extern "C" void sub_82378FA0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; std::fprintf(stderr, "B78FA0-ENTER\n"); }
  probe_o_82378FA0(ctx, base);
  if (log) { std::fprintf(stderr, "B78FA0-EXIT\n"); }
}
REX_IMPORT(__imp__sub_823FE988, probe_o_823FE988, void());
extern "C" void sub_823FE988(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; std::fprintf(stderr, "BFE988-ENTER\n"); }
  probe_o_823FE988(ctx, base);
  if (log) { std::fprintf(stderr, "BFE988-EXIT\n"); }
}
REX_IMPORT(__imp__sub_823FECA0, probe_o_823FECA0, void());
extern "C" void sub_823FECA0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; std::fprintf(stderr, "BFECA0-ENTER\n"); }
  probe_o_823FECA0(ctx, base);
  if (log) { std::fprintf(stderr, "BFECA0-EXIT\n"); }
}

REX_IMPORT(__imp__sub_823FE828, probe_o_823FE828, void());
extern "C" void sub_823FE828(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; std::fprintf(stderr, "FE828-ENTER\n"); }
  probe_o_823FE828(ctx, base);
  if (log) { std::fprintf(stderr, "FE828-EXIT\n"); }
}
REX_IMPORT(__imp__sub_823F89B0, probe_o_823F89B0, void());
extern "C" void sub_823F89B0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; std::fprintf(stderr, "F89B0-ENTER\n"); }
  probe_o_823F89B0(ctx, base);
  if (log) { std::fprintf(stderr, "F89B0-EXIT\n"); }
}
REX_IMPORT(__imp__sub_826CAFA8, probe_o_826CAFA8, void());
extern "C" void sub_826CAFA8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 6;
  if (log) { ++n; std::fprintf(stderr, "AFA8-ENTER\n"); }
  probe_o_826CAFA8(ctx, base);
  if (log) { std::fprintf(stderr, "AFA8-EXIT\n"); }
}

REX_IMPORT(__imp__sub_821E2CC8, probe_o_821E2CC8, void());
extern "C" void sub_821E2CC8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "E2CC8-ENTER\n"); }
  probe_o_821E2CC8(ctx, base);
  if (log) { std::fprintf(stderr, "E2CC8-EXIT\n"); }
}
REX_IMPORT(__imp__sub_82B3AEA0, probe_o_82B3AEA0, void());
extern "C" void sub_82B3AEA0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "B3AEA0-ENTER\n"); }
  probe_o_82B3AEA0(ctx, base);
  if (log) { std::fprintf(stderr, "B3AEA0-EXIT\n"); }
}
REX_IMPORT(__imp__sub_82C63FB8, probe_o_82C63FB8, void());
extern "C" void sub_82C63FB8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "C63FB8-ENTER\n"); }
  probe_o_82C63FB8(ctx, base);
  if (log) { std::fprintf(stderr, "C63FB8-EXIT\n"); }
}
REX_IMPORT(__imp__sub_82C62DE8, probe_o_82C62DE8, void());
extern "C" void sub_82C62DE8(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "C62DE8-ENTER\n"); }
  probe_o_82C62DE8(ctx, base);
  if (log) { std::fprintf(stderr, "C62DE8-EXIT\n"); }
}
REX_IMPORT(__imp__sub_82C62A08, probe_o_82C62A08, void());
extern "C" void sub_82C62A08(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "C62A08-ENTER\n"); }
  probe_o_82C62A08(ctx, base);
  if (log) { std::fprintf(stderr, "C62A08-EXIT\n"); }
}
REX_IMPORT(__imp__sub_82214F08, probe_o_82214F08, void());
extern "C" void sub_82214F08(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "214F08-ENTER\n"); }
  probe_o_82214F08(ctx, base);
  if (log) { std::fprintf(stderr, "214F08-EXIT\n"); }
}

REX_IMPORT(__imp__sub_82A43728, probe_o_82A43728, void());
extern "C" void sub_82A43728(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "A43728-ENTER\n"); }
  probe_o_82A43728(ctx, base);
  if (log) { std::fprintf(stderr, "A43728-EXIT\n"); }
}

REX_IMPORT(__imp__sub_82378420, probe_o_82378420, void());
extern "C" void sub_82378420(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "378420-ENTER\n"); }
  probe_o_82378420(ctx, base);
  if (log) { std::fprintf(stderr, "378420-EXIT\n"); }
}

REX_IMPORT(__imp__sub_82CBD098, probe_o_82CBD098, void());
extern "C" void sub_82CBD098(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "CBD098-ENTER\n"); }
  probe_o_82CBD098(ctx, base);
  if (log) { std::fprintf(stderr, "CBD098-EXIT\n"); }
}
REX_IMPORT(__imp__sub_8231E908, probe_o_8231E908, void());
extern "C" void sub_8231E908(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "31E908-ENTER\n"); }
  probe_o_8231E908(ctx, base);
  if (log) { std::fprintf(stderr, "31E908-EXIT\n"); }
}

REX_IMPORT(__imp__sub_82356180, probe_o_82356180, void());
extern "C" void sub_82356180(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "356180-ENTER\n"); }
  probe_o_82356180(ctx, base);
  if (log) { std::fprintf(stderr, "356180-EXIT r3=%08X\n", ctx.r3.u32); }
}

REX_IMPORT(__imp__sub_82354980, probe_o_82354980, void());
extern "C" void sub_82354980(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 4;
  if (log) { ++n; std::fprintf(stderr, "354980-ENTER\n"); }
  probe_o_82354980(ctx, base);
  if (log) { std::fprintf(stderr, "354980-EXIT\n"); }
}

REX_IMPORT(__imp__sub_825BAFC0, probe_o_825BAFC0, void());
extern "C" void sub_825BAFC0(PPCContext& ctx, uint8_t* base) {
  static unsigned n = 0;
  bool log = n < 8;
  if (log) { ++n; std::fprintf(stderr, "5BAFC0-ENTER lr=%08X\n", (uint32_t)ctx.lr); }
  probe_o_825BAFC0(ctx, base);
  if (log) { std::fprintf(stderr, "5BAFC0-EXIT\n"); }
}
