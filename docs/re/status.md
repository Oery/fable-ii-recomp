# Recompilation status

Updated: 2026-09-08 (fault attribution: 3 worker null-deref phenomena mapped to guest PCs; boot stalls post-lang.ini; init-skipped refuted for 0x834A5C88).

- Current milestone: M3 complete; M4 startup reached; M5 early title boot initializes audio, worker threads, and Xenos. M6 visible output is not yet demonstrated in the offscreen environment.
- Current blocker: boot deadlocks in a use-after-teardown worker spin that starves a critical section (see `Stall mechanism 2026-09-08`). 60 s runs stall identically (1.39M violations, 4 spinning threads). No FATAL.
- Last successful guest state: Xenos loads on Mesa llvmpipe, shader storage initializes, audio opens a 6-channel 48 kHz endpoint, `SetInterruptCallback(82B9B8D8, 44142480)` succeeds, and three graphics pipeline states are created (`build/native/logs/fable_ii_021.log`). The following repaired run reaches its 10-second bound with no additional unregistered call; `build/native/logs/fable_ii_022.log` begins when timeout-triggered shutdown starts.
- Codegen: normal validation is clean with 21 evidence-backed manual entries, including nine runtime-discovered functions added in this session. There is no forced codegen, generated edit, override, or mid-ASM hook. The only informational note remains the `0x82242ED0` large-file split.
- Missing imports: none. `D:\lhdebug.log` access denied and absent `build_version.txt` / `update:\data\tu1_data.bnk` remain expected filesystem results.
- Runtime crashes: the previous null-global diagnosis was caused by inspecting `base + (guest - 0x82000000)` instead of ReXGlue's `base + guest`. The first `sub_82CE5AB8` call actually saw singleton `0x441052AC`. The diagnostic hook then returned `-1`, caused initialization to unwind through destructor `sub_82CDAD90`, cleared `0x833370E4`, and created the later null dereference. The hook is removed. A separate missing ALSA runtime path caused the same initialization unwind; the Nix shell now exposes SDL's dynamically loaded audio libraries. After the latest repair, 15,260 null-read reports begin only when the bounded runner sends SIGINT. GDB runs for 30 seconds without reaching the decoded guest-zero handler breakpoint, confirming this is shutdown behavior rather than the next startup fault. A raw GDB SIGSEGV at guest `0x821E2AF0` was expected physical-memory handling for write `0xFF49603C`, not the logged null read.
- Intentional stubs/overrides: none. `src/hooks.cpp` contains no hooks.
- Fixed issues this iteration: corrected SDL runtime library paths; selected Mesa Lavapipe for reproducible headless Vulkan; enabled and staged Xenos; removed the causative diagnostic hook; and registered runtime targets `82CD8650`, `82CDBAE0`, `82CE5D50`, `82C1DC80`, `82C1DC50`, `82C1DC98`, `82C1DC60`, `82C8D820`, and `82C867E8`. Added `scripts/run`, `scripts/inspect-last-run`, and `docs/re/iteration-loop.md` so the loop and escalation gates are reproducible.
- Regressions: no pre-timeout regression is observed. GPU-disabled startup remains the earlier comparison; the active GPU path now runs beyond every previously recorded indirect target.
- Next actions: capture the first frame/presentation state and determine why async placeholder draws suppress presentation; run longer without conflating SIGINT teardown faults with active execution; then isolate the clean-shutdown race separately. If a new unregistered target appears, follow `docs/re/iteration-loop.md` one function at a time.

## Inventory (CONFIRMED)

Workspace: `/home/oery/Documents/Projects/fable-ii-decomp`.
The root `.git` directory is empty/read-only and `git status` reports that the
workspace is not a Git repository. No root Git history was created or modified.
The nested SDK is a clean Git checkout at
`c94f5ebdcb3c9d1a460ca48e04f9758448f8d518` (version floor 0.10.0).
Its submodules were initially uninitialized; all are now fetched at their pinned
revisions (see `sdk-submodules.txt`). No project flake, title configuration, extraction, generated code,
build, Xenia checkout, or reverse-engineering notes existed initially.

Supplied input:
`Fable II Game of the Year Edition/4D5307F1/00007000/305B6EA890970E515722`
with matching `.data/Data0000` through `Data0040` (41 segments, 6.5 GiB total).
`file` identifies the header as an Xbox Live package, media ID `1358B1A4`.
The supplied tree contains no separate title-update directory or XEX/XEXP.
All original files are recorded in `original-inputs.sha256` before extraction.

## Tooling

The root `flake.nix` / `flake.lock` pins development dependencies. Clang 20.1.8
matches the SDK's Clang 20 Linux CI; SDK CMake requires Clang >=18 and C++23.
Enter with `nix develop path:.` (explicit `path:` works without a root Git repository).

XGDTool upstream: https://github.com/wiredopposite/XGDTool
revision `a372d7a768b77908f266845a54052ee89650eb0f`.
Its checked-out CLI source accepts a GoD directory, not the extensionless header.
Use `--offline` to avoid title database requests and `--extract` for direct files.
Local build fixes are preserved in `xgdtool-linux.patch`: request `liblz4` and
include `<cstdint>` / `<algorithm>` where used. No extraction semantics changed.

## M0 verification (CONFIRMED)

Direct extraction completed with no conversion to ISO. Game root:
`assets-extracted/00007000` (XGDTool's offline directory name).
All 433 file paths and sizes match the GoD listing: 6,942,647,663 bytes.
The manifest is `game-files.json`; no relative asset paths were renamed.
All 42 original input files passed SHA-256 verification after extraction.

Entry: `assets-extracted/00007000/default.xex`, 21,217,280 bytes, XEX2.
SHA-256: `0e1ea96ded3407874cbbb3a9587d79f1340a57a9d1ba2feebcfdbc9ed1e4b6e5`.
Title ID `4D5307F1`, media ID `1358B1A4`, version and base version `0000001B`,
disc 1 of 1. Entry point `82CBB970`, image base `82000000`.
These values were read from XEX optional headers using the SDK's
`include/rex/system/util/xex2_info.h` layouts and are in `xex-identity.json`.
Only one XEX and no XEXP exist in the extracted tree; no title update is applied.
The disc includes `$SystemUpdate`, which is not a title-update patch.

Commands:

```sh
nix develop path:. -c scripts/build-extractor
scripts/extract-game
nix develop path:. -c scripts/build-sdk
```

The extraction helper verifies an existing extraction rather than repeating it,
checks original input hashes, refuses overwrite, checks every file path/size and
the XEX hash, and catches XGDTool's zero exit code on failed conversions.
The extractor build and extraction verification helpers have both been run.
SDK CMake configuration passed; compilation is in progress. The root SDK build
helper uses the architecture flags from the checked-out Linux preset.

Logs live under `logs/`; tools and proprietary assets are excluded by `.gitignore`.

## Fault attribution 2026-09-08

Repro: fresh bounded KILL run (`build/native/logs/fable_ii_037*.log`, 7 segments):
start 21:17:52, `lang.ini` miss 21:17:53.186 (thread t507222, no fallback retry,
file absent from all 433 supplied files per `game-files.json`), first violation
21:17:55.899 (thread 0xF80000BC, read guest 0). ~99k guest-0 reads (0xF80000BC)
plus ~100k guest-4 reads (0xF80000E0). Zero non-violation lines after the
`lang.ini` miss. Process survives to KILL timeout; no FATAL.

GDB attribution (CONFIRMED PCs, `logs/gdb-violation-attrib.log`,
`logs/gdb-spam-attrib.log`, `logs/gdb-first-faults.log`, `logs/gdb-guest4.log`):

- 3D Engine thread, `sub_821E27C8` (`fable_ii_recomp.235.cpp:1049`,
  `stwu r9,4(r3)`): faulting host advances exactly `+0x30100` per hit
  (`0x1ff526360` … `0x1ff7c7160`, 14/14 sampled). Caller-driven element walk via
  `821C38C8` <- `8236C520` <- `8236C360`. Same thread also faults at
  `sub_82B6C060` (`fable_ii_recomp.234.cpp:20426`, `lwz r30,4(r11)` with
  `r11 = 0`, read guest 4): `r29 = 0x834A601C` list head with `[head+4] = 0`.
- Texture Streamer thread, `sub_82B67E98` (`fable_ii_recomp.19.cpp:20703`,
  `lwz r11,0(r11)` with `r11 = [0x834A5C8C] = 0`, read guest 0) via
  `82B67950` <- `82B67DB8`, whose tail is an infinite poll loop
  (`bl 82B67950; li r3,1; bl 82CBC6B0; b 82B67E1C`). No null check precedes the
  deref; `twi` asserts follow it. Faults every iteration while head stays null.
- Init for sentinel `0x834A5C88` RUNS: `sub_8328BA88` <- `82CC1990` <- `xstart`
  on the main thread before workers spawn (GDB HIT, exactly once per boot).
  `sub_828926E0` links `[head+4] = node` with self-links, so a completed init
  cannot produce the observed null. No stores to `[0x834A5C8C]` exist in
  reachable code except that init; `8273FAF8` mutates only stack structs.
  `sub_8328BA88`-style `828926E0` wrappers exist for offsets
  23688/23700/23712/23724; none found yet for 23660/23736/24604 (`0x834A601C`).
  Array-init candidate `sub_8264DEC0` (repeated `828926E0` calls) unexamined.
- `lang.ini`: `XGetLanguage` returns English, yet the game opens
  `fr-fr/lang.ini` with no fallback open, and the opener thread logs nothing
  afterwards. Miss precedes first fault by ~2.7 s (correlation CONFIRMED,
  causation UNKNOWN). The file cannot exist on this disc pressing, so the miss
  itself must be a tolerated path; the stall needs the caller/NOT_FOUND path.
- Previous agent's GameThread-null-service capture is not on disk (only the two
  GDB logs above plus new ones); GameThread exists as a separate live thread.
  Shared-init/teardown over two independent codegen bugs remains the working
  hypothesis (same `0x834A` global block, same null-link shape, three sites).

Next: GameThread/main stacks at stall (blocked-on-what); `lang.ini` caller and
its NOT_FOUND path; live watch on `0x834A5C8C` to catch the zeroing writer;
producer of both lists; purpose of the `0x30100`-stride walk in `821C38C8`.

## Teardown causality 2026-09-08

Watchpoints on `*(int*)0x1834A5C8C` (`[0x834A5C88+4]`) and `*(int*)0x1834A6020`
(`[0x834A601C+4]`) across 4 GDB boots (`logs/gdb-watch.log`,
`logs/gdb-order2.log`, `logs/gdb-fullchain.log`; arena base fixed at
`0x100000000`, so host = base + guest):

- Both heads are initialized once at `xstart` (`sub_8328BA88`, `sub_8328BF58`
  <- `82CC1990`; node allocated, self-linked by `sub_828926E0`). Exactly one
  init hit per boot. Init-skipped and reinit both REFUTED.
- Both heads are then deliberately zeroed (`[head+4] = 0; [head+8] = 0`) by
  destructor-like functions that first walk the list, release the node via
  `sub_8221BE68`, and null the head: `sub_823FB0E8` (head `0x834A5C88`,
  `recomp.66.cpp:4711`) and `sub_82B6BEB0` (head `0x834A601C`,
  `recomp.150.cpp:20415`, which first calls sibling walker `sub_82B6C460`).
- Both destructors are reached through the same indirect table runner
  `sub_82CA9638` (`recomp.141.cpp:23316`, `bctrl` over a fn-pointer table),
  full chain: `821D2C70` <- `821E15A0` <- `822C05F8` <- `822C0568` <-
  `8219EE00` <- `822DF280` <- `8219F010` (v-dispatch) <- `82BB6CA0` <-
  `82BC6A18` <- `82BC9788` <- `82CA9638` <- indirect — running on worker
  thread slot `0x7fff02af36c0`, a DIFFERENT host thread from the Texture
  Streamer poll loop (`0x7fff17fff6c0`).
- Order proven in-run (`logs/gdb-order2.log`): inits -> teardown writes ->
  first `82B67E98:20703` guest-0 faults. The streamer null-spin is
  use-after-teardown, not uninitialized data.
- Worker design (`82B67DB8`): per-iteration QUIT-flag check
  (`[0x83496EEC]`, exit path at `82B67E68`) precedes the poll call, then
  `82CBC6B0` (`li r4,0; b 82CC2028`, yield-class wait) and unconditional poll.
  The flag is set only by `sub_822F4690` when gate byte `[0x83496EAF]` (set by
  the worker itself at loop entry) reads nonzero. Continuous faults prove QUIT
  was never set before teardown: the destroyer does not park the worker.
  `sub_822F33B8` (bank-worker proc head: alloc 288 via `8221F388`, construct,
  dispatch `82378FA0`) has no direct callers — reached via thread-proc pointer.

Classification: shared lifecycle/ordering defect (CONFIRMED mechanism,
HYPOTHESIS trigger). Two candidate triggers: (a) abort-path teardown after a
failed load (language bank is suspect: `lang.ini` miss 2.7 s before first
fault, no fallback, opener silent after); (b) legitimate teardown sequenced
before worker park. Next: identify what spawns thread-proc `822F33B8` and what
condition selects the destructor table entries; timestamp teardown vs the
`lang.ini` open in a single run.

Separate marching-store family reclassified BENIGN (handled physical writes, zero
errors); see `Stall mechanism 2026-09-08`.

## Stall mechanism 2026-09-08

Reclassifications (all single- or multi-run GDB-attributed, no code changed):

- Marching stores (`821E27C8:1049`, `832BB018`, fixed `+0x30100` stride,
  `0x1ff…` hosts) are HANDLED physical-heap writes: zero write-fault errors
  and zero `Recovered stale` warns in any runtime log (`xmemory.cpp`
  `TriggerCallbacks`/recovery path resumes them). BENIGN; dropped from
  blocker list. Region ID deferred.
- `lang.ini` REFUTED as trigger: path-printing session (`logs/gdb-paths.log`,
  GDB python decode of `X_OBJECT_ATTRIBUTES`) shows thread B loading the full
  fr-fr bank AFTER the miss (`lipsync.bnk`, `speech.bnk`, `book.babel`,
  fonts…). The miss is tolerated; assets stream fine.
- Teardown thread is `Permanent Bank` (bank loader, slot `0x7fff02af36c0`);
  after teardown it parks idle in cond-wait. Its 86-file-open career runs
  through `822F33B8` (linear, single-pass: sleep, `822F3B18` bank opens,
  allocs, then unconditionally `82378FA0` -> `82CA9638` destructor table).
  Its only failed open in the boot is `lang.ini`, but loads continue after it,
  so teardown is a scheduled finalize step, not an abort path.
- Park path never runs: `822F4690` (sole setter of worker QUIT flag
  `[0x83496EEC]`) is called only from `822EA928`, which has no direct callers
  and never hit in any session (`logs/gdb-park.log`: init -> worker-loop ->
  teardown, zero PARK hits). Teardown executes with workers polling.
- Livelock, not just spin: the streamer fault (`82B67E98:20703`) executes
  INSIDE `RtlEnterCriticalSection(0x82B679AC)` … `RtlLeave(0x82B67A6C)`
  (`recomp.24.cpp:19425/19529`, call at :19432). Every fault resumes into the
  same fault holding the CS. GameThread parks in
  `RtlEnterCriticalSection` via `823781A8` <- `82200688` (`recomp.165.cpp:4075`,
  INFINITE wait, `logs/gdb-stall-deep.log:376`); Main XThread likewise
  (`NtWaitForSingleObjectEx`). Same-CS identity unconfirmed (GameThread's CS
  object is dynamic: `r4 = [[r31+16]+4]`).
- 60 s bounded run (`fable_ii_051*.log`, 12 segments): identical stall, no new
  pipelines/FATAL/progress; faulting threads grow 2 -> 4 over time.

Remaining trigger question: what orders (or should order) bank-finalize
teardown vs worker park under ReXGlue timing, and what spawns/conditions
`822EA928`. Next: identify the CS owner at stall (ReXGlue XCriticalSection
owner field, live); find `822EA928`'s indirect caller; then fix-layer decision
(game timing assumption vs ReXGlue sync divergence). Candidate
observation-only experiment: pause streamer/3D threads at teardown and see if
boot proceeds (tells whether worker spin is the sole stall cause).


## Park-injection result 2026-09-08

Observation-only experiment (`logs/gdb-inject.log`, run `fable_ii_054*.log`,
20 segments): at each teardown watch-hit, wrote guest byte `[0x83496EEC] = 1`
(the worker QUIT flag the game sets in `822F4690` but never sets in this boot).
Both injections landed (`QUIT-INJECTED at W2/W1-teardown`).

- Streamer worker parked EXACTLY as designed: injection run shows ZERO
  guest-0 reads (vs ~99k/10 s normally). `82B67DB8` saw the flag, took its
  `82B67E68` exit path, stopped faulting. Model CONFIRMED end to end.
- 3D worker unaffected: 657k guest-4 reads continue (`82B6C060` path does not
  honor `[0x83496EEC]`), boot still stalls with no new milestones. Its loop
  (`8236C360`/`8236C520` region, frame math + `82200688` lock/unlock around
  `821C38C8`) has no found exit check yet.
- Conclusion: worker spin is confirmed as A stall cause (removing one spinner
  changes the fault profile exactly as predicted), but the 3D worker's spin
  alone suffices to hold the boot. GameThread remains parked in
  `RtlEnterCriticalSection`.

Next: exit condition (if any) of the 3D loop; whether the 3D lists
(`0x834A601C` family) are meant to be restored or the thread parked, and by
which path; indirect caller of `822EA928` (the never-run park path). No
permanent fix attempted; all experiments observation-only or reverted
(injection was in-memory only, nothing committed).

## Ordering verdict 2026-09-08

- Teardown is UNCONDITIONAL in `822F33B8`: the apparent branch at
  `822F3540` only guards a virtual call; both paths fall through to
  `82378FA0` -> destructor table. `[r28+80]` is overwritten, not tested.
- Park path (`822EA928` -> `822F4690`) has no direct callers, no
  pointer-table materialization of `0x822EA928` in generated code, and never
  executes in any observed boot. Workers it should park poll unconditionally.
- ReXGlue honors `X_CREATE_SUSPENDED` (`xthread.cpp:483`, guest
  `suspend_count` init); no SDK lifecycle bug found. GDB capture of guest
  creation flags failed (optimized SDK symbols; register mapping wrong).
- Interventions `return`-at-entry and store-neutralize both corrupt state
  (`return` unbalances software-managed guest SP; neutralize converts clean
  null-faults into use-after-free heap writes) and produce artifact
  `FATAL 0x82B84350` + SIGABRT never seen in normal runs. DO NOT add
  `0x82B84350` to the manifest; it is corruption fallout, not a missing
  function. Intervention family CLOSED; ordering-level fix only.
- Net: healthy boot must park workers before (or instead of) bank-finalize
  teardown; this boot never establishes parked state. Remaining unknown is
  what should order them (thread-phase contract under ReXGlue timing vs a
  never-taken park path). Fix-layer decision pending review: mid-ASM/native
  hook parking workers at teardown entry vs deeper hunt (3D exit condition,
  `822EA928` indirect caller).

## Override test verdict 2026-09-08

User-approved reversible test: strong `sub_823FB0E8` / `sub_82B6BEB0` no-ops
in `src/hooks.cpp` (linkage verified: strong `T`, register table binds the
weak `sub_` symbols, GDB breakpoints resolve to `hooks.cpp` and HIT via
`82CA9638`). Results (`fable_ii_061*.log`, `fable_ii_066*.log`):

- Teardown nulling of the two watched heads is SKIPPED, yet null faults
  persist at baseline volume — so remaining faults come from OTHER null
  state, not unwatched writes to the same heads.
- Overrides UNBLOCKED thread B past teardown to genuinely new code:
  `FATAL 0x82B84350` (8-byte `addi r3,r3,8; b` thunk, registered
  `end = 0x82B84358`, codegen clean). After registering it, the FATAL is
  gone; boot still stalls on the next null layer.
- New null layer (current binary, `logs/gdb-override-nulls2.log`): streamer
  faults at `82B67950:19689` (`lwz r31,0(r11)`, `[0x8331AFA4] = 0`, a THIRD
  global after passing `82B67E98` cleanly — heads stay valid, override
  works); thread B faults at `825F7B10:8985` (`lwz r27,4(r28)`, r28 = 0
  propagated via `82237060` <- `82A67D38`, itself a release-and-zero
  teardown not covered by the overrides).
- `FATAL 0x82B84350` in intervention runs is a REAL undiscovered function,
  not corruption (valid thunk, registered). The earlier corruption abort
  (SIGABRT) came only from `return`-at-entry / store-neutralize methods
  (guest-SP imbalance / use-after-free writes); both methods CLOSED.
- Writer hunt for `[0x8331AFA4]` inconclusive statically (`-20572` offset
  shared by `0x834A0000`- and `0x83320000`-based accessors; `sub_8325AEA8`
  writes the former). Needs live watchpoint, same as before.

Next: watchpoint `[0x8331AFA4]` writer (init? teardown? never?); decide whether
to extend no-op coverage to `82A67D38`-family destroyers or restore-then-park;
the pattern is now mechanical: unblock -> register -> attribute -> repeat.
Overrides stay in `src/hooks.cpp` (clearly marked TEST) until boot advances.

## Timing variance + table shape 2026-09-08

- Destructor table (`82CA9638`) is nearly EMPTY at its first invocation
  (bounds words read as non-addresses; `blt` skips the loop) and fills later
  (atexit-style registration). It has MULTIPLE destroyer entries for MULTIPLE
  heads: `823FB0E8` (generic head-arg, hit 4x), `82B6BEB0` (`0x834A601C`),
  `832AF210` (`0x8331AF98`, zeroes `[0x8331AFA4]`). All three are no-op'd in
  `src/hooks.cpp`. Direct tail-call wrappers to `823FB0E8` also exist
  (`176/201/68/90.cpp`, `147/282.cpp`), all routed through the overridable
  `sub_` symbols.
- Fault mix is TIMING-DEPENDENT per run: `075` (GDB-slowed) shows only
  guest-4 + write-`0x14`, zero guest-0 (streamer never faulted); full-speed
  runs show guest-0 + guest-4 + sometimes write-`0x14`. Per-run attribution
  is a moving target; the stable facts are the mechanism (teardown-then-spin,
  fault-while-holding-CS) not the mix.
- CS-owner live read not yet obtained (3000-hit ignore gate never filled
  under changed fault mix). GameThread parked in `RtlEnterCriticalSection`
  stands from `gdb-stall-deep.log`.
- Open: CS-owner identity at stall; 3D-loop exit (if any); `822EA928`
  indirect caller; then the ordering fix (park-before-teardown) at the
  correct layer.

## Boot event order 2026-09-08

Single-run breakpoint sequencing (`logs/gdb-events.log`, current binary):
`STREAMER-POLL x3` -> `3D-USE x3` -> (bank loading) -> `DESTROY-B`,
`DESTROY-C`, `DESTROY-A x4` (all override no-ops firing). `POPULATE-G4`
(`82A47D48`, writer of `[0x8349F554]`) NEVER fires in 90 s.

- Consumers poll long before teardown: the streamer/3D null faults are NOT
  all teardown-caused. Fourth global `[0x8349F554]` is never populated
  (producer phase never arrives), a second disease class (D2) beside
  teardown (D1).
- D1 (teardown) is neutralized by the three no-op overrides (watch-confirmed
  no writes; override breakpoints hit). D2 (never-built state) remains and
  now dominates: 3D faults at `821C38C8:878` on never-populated `[0x8349F554]`;
  thread B faults on null inputs downstream.
- Circle: main/GameThread stuck in `RtlEnterCriticalSection` behind
  fault-spinners -> producer phases (`8236C940` -> `82A47D48` etc.) never
  run -> more nulls -> more spin. Breaking it requires stopping the spins
  (park) or completing the builds (unblock main), not more per-list covers.
- Next decision (needs review): mid-ASM skip-when-null hooks at the faulting
  uses (emulate poll-again-later), or ReXGlue-side bounded fault retries with
  guest-thread termination (emulate HW crash), or deeper producer hunt.

## Producer never runs 2026-09-08

Breakpoint sequencing (`logs/gdb-producer.log`, current binary): worker
threads spawn, then `3D-USE` (821C38C8 <- 8236C520:3830 <- 8236C360:3848) fires
IMMEDIATELY on the fresh 3D thread; bank thread spawns after. `POPULATE-G4`
(`8236C940` -> `82A47D48`, writer of `[0x8349F554]`) never fires in the
window; teardown watch fires much later. Mid-asm skip is MECHANICALLY
IMPOSSIBLE here (hooks key on block base; `0x821C3C18` is mid-block;
hook-at-block-head sees wrong `r3`; hooks take register values only, no
ctx/base) — manifest/function reverted, binary rebuilt clean.
Refined circle: 3D faults at `821C38C8:878` inside its `82200688`-held
region (`8236C520:3842` enter), ReXGlue resumes the fault holding the lock;
GameThread parks in `RtlEnterCriticalSection` (`823781A8`); main-side
producer phases never run; more nulls; more spin. Next: prove/disprove
same-lock (GameThread target vs 3D-held) via live CS-owner read, or break
the circle by parking 3D (exit still unknown) or pre-populating G4.

## Park path runs but stuck 2026-09-08

Full stall stacks (`logs/gdb-stall-full2.log:518`): GameThread =
`RtlEnterCriticalSection` <- `82200688` <- `823781A8` <- `822F47F8` <-
`822EA928` (`recomp.201.cpp:2515`) <- `82CA3388` bootstrap. The park path
`822EA928` DOES run (on GameThread, during boot) but is STUCK at its
`822F47F8` sub-step (`recomp.201.cpp:2515`) waiting on a critical section,
never reaching the QUIT setter (`822F4690` at `:2520`). Earlier
"never runs" verdict was wrong (breakpoint was on the setter, downstream
of the stuck point). Main XThread waits in `NtWaitForSingleObjectEx`.
- No worker kill/suspend observed in 120 s (`logs/gdb-death.log`: only 3
  early VFS-helper completions). Workers live forever by design; no
  lifecycle bug in ReXGlue thread exit (exiting threads exit cleanly).
- Net: boot sequence needs a CS held elsewhere; fault-spinners are the
  prime suspects for holding it across resume-livelock, but same-lock is
  still unproven (GameThread target is dynamic). Candidates: streamer-held
  `0x834A5C6C` region, 3D-held `82200688` region, or a leaked hold on
  thread B's path. Next: CS-owner identity (guest `OwningThread` live read)
  or unstick GameThread by stopping all spinners first.

## GameThread runs the park path 2026-09-08

Full guest stacks at stall (`logs/gdb-stall-full2.log:518`): GameThread =
`RtlEnterCriticalSection` <- `82200688` <- `823781A8` (`165.cpp:4075`) <-
`822F47F8` <- `822EA928` (`201.cpp:2515`) <- bootstrap. The never-run park
path DOES run, on GameThread, but is stuck at `822F47F8` (CS wait) before
reaching the QUIT setter (`822F4690` at `:2520`). Earlier PARK breakpoints
missed it (downstream of the stuck point). Main XThread waits in
`NtWaitForSingleObjectEx`.
- No worker kill/suspend in 120 s (`logs/gdb-death.log`: 3 early VFS exits
  only). ReXGlue thread exit works; game never terminates/suspends workers.
- CS-owner live ID blocked: GameThread target is dynamic
  (`r4 = [[r31+16]+4]`), guest regs unreadable in optimized frames, candidate
  enumeration needs the object address first. Same-lock (spinner-held)
  unproven; leaked-hold on thread B's path not excluded.
- Open threads: (a) is `82A67D38` (B's r28-null destroyer) table-driven?
  (b) which thread runs `8236C940` (G4 populate)? (c) CS-owner identity.
  Static reads this turn: `822F47F8` is vtable-dispatch heavy; `823781A8`
  call reached indirectly (no direct `sub_823781A8` site in `21.cpp`).

## CS protocol sound 2026-09-08

Audited `RtlEnter/LeaveCriticalSection_entry` (`xboxkrnl_rtl.cpp:379-447`)
plus `PosixCondition<Event>` predicate/latch/auto-reset-consume
(`threading_posix.cpp:286-330,443-472`): enter spins, then
`atomic_inc != 0` gates an event wait; leave `atomic_dec != -1` signals;
signals latch until consumed. No lost-wakeup hole at either layer
(Xenia-derived, balanced inc/dec). GameThread's wait is on a GENUINELY
HELD lock, not a dropped signal. `XThread::Exit` performs no CS rundown,
so killing spinners would abandon, not free.
Next: holder identity. Cheapest decisive instrument: separable SDK patch
logging `(guest_cs, thread_id)` enter/leave transitions (capped/sampled),

## Release funnel rejected 2026-09-09

- `8221BE68` -> `83231BE8` is NOT raw free: gated by `[0x83496CAB]`, null
  object early-out, then vtable-dispatched work (`832304E8`) with status
  returns. Overriding it would drop guard protocol + virtual side effects.
  REJECTED; per-entry no-ops stand.
- B's `822DF280` fault (`[r29+20] = 0xC` then `[0xC+8]`): `r29` object
  half-built (count present, pointer pending) = producer-interrupted state,
  not use-after-free (no-ops prevent frees yet fault persists).
- Lock word at stall (`0x4C11E740`): `lock_count = 2`, `recursion = 2`,
  `owning = KTHREAD 0x30097018` (BE-decoded; mixed-endian struct verified
  against `X_RTL_CRITICAL_SECTION` layout). Frozen 12 s+ (full-speed probe
  samples). Next: dump holder KTHREAD (`0x30097018`) wait blocks
  (`+0x40`) + `thread_state` (`+0x6C`) to see what the holder itself waits
  on; unwind one level.

## Single-core + stale pointer 2026-09-09

- `taskset -c 0` 30 s run (`fable_ii_146.log`): serialization does NOT
  unblock (same probe wall: no QUIT/populate). Only remaining fault:
  6k writes of guest `0x83230568` on thread B.
- `0x83230558` (the "CS" B leaves) disassembles as CODE epilogue
  (`addi r1,r1,96`, file bytes match) in a CODE section per XEX page
  descriptors. B operates on a stale/garbage object pointer (return-address
  shaped), not a protection divergence: unwritable on HW too. B is lost,
  not merely early.
- GameThread reaches `823781A8` (lock-acquire probe fires) in every regime
  but never returns from it; QUIT/populate producers never run in any
  observed boot. The circle (unbuilt state -> spin/fault -> stuck sequencer
  -> unbuilt state) holds across core counts, with and without zero-page.
- Next: B's input chain (where its `r3` goes stale) or the CS holder — both
  now require tracing live object provenance, not more fault PCs.

## Work-drain loop wedge 2026-09-09

- GameThread is NOT stuck in `RtlEnter`: enter/exit brackets prove
  `82200688` returns; `823781A8` enters/exits; all its post-lock callees
  fire. It wedges in `822F47F8`'s work loop (`loc_822F48B8` -> ... ->
  `loc_822F4940`: re-loop while `[r31+4] == 0`), whose iterations include a
  10 s sleep (`82CBC6B0(10000,0)` -> `KeDelayExecutionThread`) and a virtual
  work call whose return (`r30`) never reads 0 (empty). Flag `[r31+4]` is
  only set on the `r30 == 0` path, so the loop is a drain-to-empty wait that
  never drains.
- The drained work object (`r3 = [r31]` -> vtable `+12`) is dynamic; its
  producer/consumer and why it never empties are UNKNOWN. Candidates: bank
  job queue (B fault-loops on stale state instead of draining), streaming
  queues (workers poll but never consume under zero-page/override regime).
- Next: identify the work object + its drain condition (probe the virtual
  target + `r30` values per iteration), or unstick by completing one drain
  cycle artificially and watching for cascade.

## Session handoff 2026-09-09 (cheats stripped, slow organic boot, audio reaches intros)

- Binary is now cheat-free: all behavioral TEST overrides reverted to pure
  reporter probes (`82200688` acquire-skip, `822F27C0` drain-skip,
  `8236CA90` wait-skip all removed). Remaining hooks are observation-only
  EXCEPT data preseeds: fourth-global `[0x8349F554]` + G4 nodes (zeroed
  32-byte heap objects, self-heal when the real producer overwrites).
- Why each cheat was reverted:
  - `82200688` 3D-lockskip broke the callee data contract (`[slot]=lock`
    store skipped) leaving stack garbage in `[r1+88]`, which `8236DC20`
    later used as a lock pointer -> 26k `write of guest 0x83230568`
    fault-resume storm per 20 s run. Revert -> zero faults everywhere.
  - `822F27C0` drain-skip faked "queue empty" -> mass worker exits ->
    process end / brain-dead main-spin. Never use again.
  - `8236CA90` wait-skip + `82200688` faithful acquire-skip only raced
    ahead into the same drain; no new information.
- Current organic behavior (20 s runs, `--protect_zero=0`): DRAIN-VERDICT 1,
  GameThread `822EA928:2515 -> 822F47F8 -> 823781A8 -> 82200688(lock
  `0x4C11E740`) -> drain virtual-spin in `822F47F8` (`loc_822F48B8` loop,
  `[r31+4]==0`, no `82CBC6B0(10000)` sleeps observed). Zero access
  violations, zero FATALs. Bank `822F33B8` #1 runs with heap `r29`.
- W1 lock race (timing-dependent): GameThread's `82200688` acquire on
  `0x4C11E740` sometimes hangs (ENTER without EXIT, run 186: count=1,
  owner=`0x30097018` sampled pre-acquire), sometimes completes (run
  187/188: ENTER+EXIT). SDK `RtlEnter` slow path (`xeKeWaitForSingleObject`
  on the CS-as-autoreset-event) is Xenia-derived and looks sound; 23
  slow-waits/60 s observed via GDB, none from GameThread's site. Root of
  the occasional hang is NOT identified (holder `0x30097018` unmapped to a
  thread; lost-wakeup vs parked-holder-nesting unresolved).
- Slow-boot evidence: 300 s cheat-free run reached drain + bank, workers
  HOT (6+ full cores, VFS asset IO) then parking, still no populate
  (`822F2608` absent) at 240 s. Drain appears asset-bound (minutes), not
  wedged. User audibly confirmed studio-intro sound during these runs.
- M6 blocker is now environment, not emulation: with `DISPLAY=:0`
  (X.Org reachable, no auth needed) the run dies at startup with
  `SDL_InitSubSystem(SDL_INIT_VIDEO) failed: No available video device`.
  SDK SDL3 is built X11=ON/Wayland=ON but SHARED (dynamic); auto-init
  falls over where explicit `SDL_VIDEODRIVER=offscreen` works. Strace run
  (`/tmp/sdlstrace.log`, bg_2) was cancelled before delivery.
- Next actions in order:
  1. Diagnose SDL video: strace `openat` for x11/wayland libs under the
     launch env, compare with `SDL_VIDEODRIVER=x11` explicit error.
  2. Visible run on `:0` (software Vulkan via `VK_ICD_FILENAMES` lvp as
     set by flake shellHook); confirm window + intros on screen.
  3. Longvisible run to title; only then revisit W1 (if it reproduces)
     with holder identification (`0x30097018` -> thread).
  4. Remove data preseeds once producers are observed overwriting them.
  5. `docs/re/functions.md` still ends at the 21-entry codegen note; the
     runtime-discovered call graph (`823781A8`, `822F47F8` drain loop,
     bank chain) is only in this file + `src/hooks.cpp` probe names.

## Visible milestone + white-screen stall 2026-09-09 (M6 partial)

- M6 PARTIAL (user-verified): window opens on `:0`, Lionhead intro plays
  with video + audio. Then white screen (renderer presents clear color,
  llvmpipe hot) while boot never reaches populate (`822F2608` absent).
- SDL fix (persistent): flake `shellHook` `LD_LIBRARY_PATH` now includes
  the X11/Wayland helpers SDL3 dlopens (`libXrandr`, `libXfixes`, `libXi`,
  `libXcursor`, `libXss`, `libxcb`, `libXtst`, `libXext`, `libX11`,
  `libxkbcommon`, `wayland`, `libdecor`). Root cause of `No available
  video device` was these missing at runtime (strace-proven).
- White-screen mechanism (all evidence current-run):
  - Permanent Bank fault-spins (~18k faults/s, write to code bytes,
    address varies per run: `0x82A700A8`/`0x82AC6028`/`0x82AE0728`) in
    `82BCD7B0:21275` (`stwx r31,r9,r11`, proven by perf-sample IP +
    addr2line to `fable_ii_recomp.104.cpp:21275`).
  - Chain: `822F33B8` -> `82378FA0` -> `823FE988` -> `823FECA0` ->
    `822AC668` -> `821D2C70` -> `821E15A0` -> `822C05F8` -> `822C0568` ->
    `8219EE00` -> `822DF280` -> (`82BCC3A8`|`8229A518`) -> `82BC9640` ->
    `82BC9860` -> `82BC8490` -> `8227BA30` -> `82BCD7B0` (perf, two runs).
  - Upstream defect: item pool `8240DAA8` returns NULL x256 (free-list
    slot for the requested size class empty; pool struct itself valid
    heap, `POOL-GLOBAL`/`POOL-STATE` probes). Pool init `823052C0` runs.
  - Entry snapshots at `82BCD7B0` look valid (heap table `r6=4F640284`,
    `cnt=0x20`, small `r30`), so fault-time values differ -> concurrent
    mutation race with fill workers (`82BCA340`/`82BC9E10`/`82BC9EA0`
    cycle on the same object `4F640220`, `7BB58-CALLER` probe).
- Active TEST interventions (all reversible, all in `src/hooks.cpp`):
  - `bank_table_mutex` across `82BCD7B0` + 3 fill workers: fault storm
    drops to ZERO while active (run18/20: 0 faults, all workers alive:
    Main/GameThread/Streamer/Cloth/3D/Bank). Strong race confirmation.
  - `8227BB58` null-item scratch (zeroed 64 B via `probe_alloc32`,
    re-zeroed per call, shared): keeps bank mechanically completing
    instead of fault-spinning on pool-empty NULL. Cuts storm ~10x alone.
  - Reporters only: `DRAIN-ITEM` (`47F8` ctx stack, item NULL at entry),
    `822F5540` brackets, `8240DAA8`/`BANK-TBL*`/`POOL-*`, GT LWP capture.
- GameThread parks sleeping inside `822F47F8` drain (nanosleep, ~16
  switches/6 s) waiting for `r30==0`; drain item needs real worker
  completions that never come (pool empty + producers behind populate).
  Circular: GameThread waits for bank items, bank items need
  populate/streaming data, populate is GameThread's own next step.
- Next: stock the pool for real (find what returns/fills blocks for the
  starved size class) or make drain/bank skip genuinely-empty items the
  way the game does on HW; then remove scratch + mutex + preseeds in that
  order, verifying fault count stays zero and populate fires.

## Drain circle mapped 2026-09-09 (no game running; static + prior probes)

- GameThread parks in `82CC2028` (1-tick `KeDelay` poll loop) via
  `823784A0` via `822F47F8:48CC`-virtual (perf stacks + addr2line, two
  runs). It waits for `[flag]==0` where flag/item =
  `DRAINVIRT r3=42205020 r4=42205148 [r4]=01` (heap pair, +0x128 apart;
  addrs stable across runs). `FLAGWATCH` proves the flag NEVER changes.
- The item is referenced NOWHERE else (log-wide grep): no worker owns it.
  `82356180` publishes the drain ctx to global `lis-31927+26920` for
  subscribers (audio `826C63D8`, bank readers in `104.cpp`, others), but
  the item slot `[ctx]` is filled only at `47F8:48A0` (r30 from
  `82356180`/`823789D0` or 0) AFTER subscribers may have polled it as
  null. Timing race: early readers see null and never re-check (or park
  on events never signaled on fill).
- Pool `8240DAA8` NULLs are a symptom (size-class free list exhausted by
  checked-out stuck items; `POOL-GROW` succeeds for other slots; struct
  valid heap). `82477768` writes `[r31+296]` = the same flag offset
  (`0x42205020+296 = 0x42205148`) but is only reachable indirectly and its
  `CLEARER` probe never fired: the flag has no running clearer.
- VFS/streamer go fully idle (0 ticks): no requests flow because every
  requester waits on GameThread, which waits on the orphaned item. Quiet
  stalemate, not slow loading. Soak will not resolve it.
- Next live run: timestamped (guest tick) event order log — publish
  (`82356180`), fill (`47F8:48A0`), subscriber reads, `DRAINVIRT`,
  `FLAGWATCH` — to catch the exact race order. Then either re-order
  (delay GameThread's drain until subscribers run) or synthesize the
  missing completion exactly once (flag clear + r30==0 path) and verify
  populate fires and the game continues on real state after.

## GPU-interrupt suspect 2026-09-09 (no game running; static)

- New prime suspect for the stuck flag: GPU-completion signaling.
  `823784A0` (head at `250.cpp:3578`) does inline frame/timing work on the
  item, then parks in `82CC2028`. The item flag may be cleared downstream
  of the registered GPU interrupt callback `82B9B8D8` (46.cpp, short ISR
  that stores `[r31]` and signals two kernel objects via `832B2BFC` /
  `832B2C3C`). Chain would be: ISR fires -> event signaled -> worker
  wakes -> flag cleared. If the ISR never fires, everything downstream
  parks (bank futex, GameThread flagwait) while 3D keeps submitting
  frames that never generate the awaited interrupt.
- SDK side is implemented (`GraphicsSystem::DispatchInterruptCallback`
  from CP-generated interrupts in `command_processor.cpp:922`), so the
  question is whether the game submits the packets that generate them.
- New probe `GPU-IRQ` (hook on `82B9B8D8`, logs #1-5 then every 1000th)
  answers it in the next live run: zero hits = interrupts never
  delivered = likely root of the white-screen circle. Built, awaiting run.

## Holder identified + timing spiral 2026-09-09 (live runs + perf)

- `THREADMAP` (guest-id -> host-LWP via `XThread::GetCurrentThread`) maps
  the `0x4C11E740` lock holder `0x30097018` to 3D Engine (also:
  `0x30030018`=GameThread, `0x30092018`=Cloth, `0x3009C018`=Bank).
- 3D holds the shared drain/stream object lock while yield-spinning in
  `822E0508` (`82CBD098` yield, perf-proven) waiting on frame
  data/timing; GameThread spin-waits on the same lock (CAS never wins).
  On HW 3D's hold is milliseconds; under llvmpipe each frame costs far
  more than wall-clock pacing expects, so the hold stretches to minutes
  (never observed released). GameThread starves, drain never empties,
  populate never runs, menu never loads.
- Mitigation attempts: small host window does nothing (guest backbuffer
  fixed by game); `VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/nvidia_icd.json`
  fails (`ErrorIncompatibleDriver`, nix loader vs host driver skew);
  unsetting it falls back to llvmpipe. Real-GPU needs nix-opengl
  integration (out of scope for now).
- Live interventions active: `bank_table_mutex` (0 faults), 7BB58
  null-scratch, FLAGSYNTH (flag+item+0x88, proved insufficient: GameThread
  waits on a deeper object), SKIP-GTSLEEP (GameThread advanced past the
  null-wait into `822F3640`, now parked in its `82200688` on the same
  3D-held lock). Progress is real (each skip advances one wait deeper)
  but the 3D hold is the wall behind all of them.

## Drain virtuals mapped 2026-09-09 (live visible runs)

- Item vtable `V=0x820A7E20` slots (live reads): `[16]=0x831FD318`,
  `[20]=0x82C43198`, `[24]=0x82378868`, `[28]=0x829CE870`. All four
  bracketed: every call ENTERs+EXITs cleanly (none is the park).
- GameThread's park moved deeper: past all four early virtuals, before
  the 2nd `822F5540`, inside the 4028/4066-virtual region of `823784A0`
  (`[[r3+240]+16]` / `[[r3]+16]`, dynamic targets, unhooked). Fully
  descheduled (0 perf samples even at 999 Hz): infinite wait (CS/event),
  not a poll.
- Live interventions holding: `bank_table_mutex` (0 faults),
  7BB58 null-scratch, FLAGSYNTH (insufficient: actual wait object is
  deeper), SKIP-GTSLEEP + faithful acquire-skip (GameThread advances
  through every wait until the 4028/4066 region), GPU-IRQ flowing (14k+,
  interrupts healthy), `THREADMAP` (holder IDs), capped contention log.
- Open: identify the 4028/4066 target (needs callee-side hook on unknown
  address: enumerate candidates via live `[r3+240]` read at a fired hook,
  or catch GameThread running with high-freq perf at the exact moment).
- Built, awaiting run: entry-processor brackets (`82C63098` with arg,
  `8236CED8`, capped 6) to see whether GameThread enters the per-entry
  build and which call doesn't return.

## Bank chain walks 2026-09-09 (offscreen runs, no user needed)

- Bank chain brackets: `82378FA0` -> `823FE828` -> (`821E2CC8`,
  `82C63FB8`, `82214F08` all return) -> `8236CED8` ENTERs, and its
  callees (`8236CB48`, `82B68FC8`, `82A3B890`, `82A3BB78`) all return;
  `8236CED8` itself EXITs. Entries complete genuinely (not stuck).
- Drain processes entry after entry (multiple `ENTRYCED8` pairs), each
  ~60 s, fault-free, many workers running (not parked). populate not
  reached in 150 s. Verdict shifting from "stuck" to "very slow":
  300 s background run in flight to test whether the drain empties
  organically. `CB48-ENTER` re-proves the park point when it happens:
  `lock=0x4C11E740 count=0 owner=3D`.

## Drain loops, not parks 2026-09-09 (offscreen, capped probes)

- Correction: GameThread is not parked in one spot. `GT2` showed 10k+
  `822F3640`-lock acquires in one run: `823784A0` iterates internally
  (10k inner calls, ~70/s) without returning (`VIRTRET` never fired in
  150 s). It is slowly working, and `VIRTRET r3=` (cap 20, new probe)
  will show what it returns when it finishes: `0` unblocks the drain.
- Scratch-null substitution removed (it fed infinite fake-pending items:
  zeros read as pending forever). Mutex kept (0 faults without it
  unproven; re-test if faults return).
- 2.9 GB `runtime-console.log` flood from uncapped `GT2` pair logging
  deleted; all hot-path probes capped. I/O flood was skewing timing.

## Correction: 3D releases per iteration 2026-09-09 (static)

- `8236C360` takes the shared lock via `8236C520` but `8236C520` itself
  contains five `RtlLeave` sites: the take is balanced per call, NOT held
  forever. 3D releases constantly; GameThread should interleave. Its
  failure to do so implicates the inner waits (frame-data/scarcity), not
  the lock hold itself. The faithful-skip (not-held report, no matching
  leave) preserves 3D's count while letting GameThread's work proceed.
- `823781A8`/`8236CB48` both gate their leave on `[slot+4]` (the
  acquire-reported flag): reporting not-held skips the leave cleanly.
- Live: both `SKIP-ACQUIRE` sites firing, null-waits skipped, scratch
  removed (was feeding infinite fake-pending), mutex kept, `VIRTRET`
  pending to reveal the virtual's return contract.
- Live: `378420` bracket (tail call of `823784A0`) added to test whether
  GameThread parks in the function tail after passing all known waits.

## Slow grind, not stuck 2026-09-09 (offscreen)

- GameThread reaches both `822F3640` calls (free locks), `SKIP-GTSLEEP`,
  all four V-targets (returning), entry builds completing. It is inside
  one long `823784A0` call grinding entries (~60 s each). 150 s bounds
  never see it return. 600 s soak running to test organic completion.
- Interventions holding (mutex, skips, null-skip, synth as backstop).
  0 faults throughout. If the backlog drains, populate fires.

## Empty exit, not hang 2026-09-09 (offscreen)

- Without scratch, null items take empty paths: queues drain for real,
  workers finish, threads exit, and the process ends cleanly with zero
  work done (no populate, 0 faults). The game concludes "nothing to do".
- Root is request-side: VFS works early then idles (no more requests);
  requesters (GameThread-populate) wait on deliveries that need requests.
  The drain item (fully built, valid sub-objects) waits for a completion
  whose worker was never assigned: no thread references it.
- Next: trace main-thread early-init streaming requests (who should ask
  VFS for the menu/boot assets and whether that path runs).

## Hook coverage rule found 2026-09-09

- Direct `sub_X(ctx, base)` calls sometimes bypass hooks, sometimes not
  (link-override applies inconsistently; `__imp__` edges always hook).
  Silent brackets therefore mean "unreached OR bypassed" — ambiguous.
  Confirmed bypass: `378420` (direct from `823784A0:4122`, silent while
  GameThread provably proceeds past it); confirmed hook: `82CBC6B0`
  (SKIP-GTSLEEP fires). `GTLOCK` logged both `822F3640` sites.
- GameThread provably reaches: 47F8 -> 5540 -> 781A8 -> 48CC/DRAINVIRT
  -> V16/V20/V24/V28 (return) -> CB48 (skipped) -> GT2 (free) ->
  SKIP-GTSLEEP -> 822F3640#1+#2 (GTLOCK both) -> ??? (378420 silent).
- Empty-exit (no scratch): null items take empty paths, queues drain for
  real, workers finish and exit, process ends cleanly with zero work
  done. Proves the wedge is request-side starvation, not a hang.

## Gate timeout + owner puzzle 2026-09-09 (offscreen)

- Phase gate (block 3D's first take until drain-done, 120 s bound)
  held 3D correctly (`GATE-WAIT`) but timed out (`GATE-TIMEOUT`):
  GameThread's drain does not complete even uncontended. Gate alone
  insufficient; drain has its own independent stuckness (pool/flags).
- Owner puzzle: `0x4C11E740` showed `owner=3D` while 3D was gate-blocked
  (never took it this run). Either a bypassed take path or stale owner
  from an unbalanced leave elsewhere.
- Import traces (`REXKRNL_IMPORT_TRACE`) are compiled out in
  RelWithDebInfo: `REX_LOG_VERBOSE=true` shows no `Nt*` lines. File
  opens visible only via `/proc/PID/fd` (bundles mapped: Globals + GUI)
  or strace (blocked: ptrace_scope=1, not a child).
- 300 s scratch-free soak: 0 faults, no populate. Stuck, not slow.

## Probe tax 2026-09-09

- A 150 s run never reached the drain (previous: ~60 s). ~50 active
  probes (LOCKW memcpys on every `82200688`, per-poll FLAGWATCH) slow
  boot measurably. Next idle pass: trim to essentials (keep fault
  guards + milestone markers, drop per-call readers).

## Wait-retry + slow boot 2026-09-09 (offscreen)

- `8240DAA8` wait-retry (50 x 2 ms on NULL) built: lets a lagging carver
  stock the pool instead of instant-NULL fault-spin. Untested in anger:
  latest 150 s run never emptied the pool (no nulls at all) but also
  barely reached the drain (47F8-ENTER at ~140 s) before timeout.
- Boot time keeps growing (drain at 60 s weeks ago, now 120-140 s):
  probe I/O tax is real. 300 s run in flight.

## Stale-binary trap 2026-09-09

- Several confusing runs used a stale binary: `scripts/build` output was
  misread (codegen lines, not ninja) and `hooks.cpp` edits after 18:30
  never compiled until a forced `touch` rebuild at 18:52. Rule: after
  every hook edit, `touch src/hooks.cpp`, rebuild, and confirm the
  binary mtime is newer than the source before launching.

## Observer effect 2026-09-09

- Concurrent observation (perf + procfs scans + polls during runs)
  steals CPU from llvmpipe-bound boot and slows phases measurably.
  600 s hands-off run launched 18:59 to test drain completion without
  interference; poll rarely until it ends.

## Build blocked: codegen SIGBUS 2026-09-09 ~19:20

- `rexgluerd codegen` dies with SIGBUS in `Memory::Initialize`
  (xmemory.cpp:250) deterministically since ~19:20, blocking ALL builds
  (ninja always reruns codegen: no usable depfile). XEX hash verified
  intact; manifest intact; disk/inodes/memory fine; no stuck processes.
- Worked around briefly with `setarch -R` (then failed too); SDK tree
  restored (stale wrapper copies removed except build products).
- Likely host VA-layout collision (fixed 16 GB arena mmap) that shifted
  during the day; a reboot will probably clear it. Current binary
  (19:10, WITH the pool wait-retry) is the newest available.
- New early crash (bank SIGBUS ~60 s, coredump 19:10:57) coincides with
  the pool-retry binary; the retry's double side effects are prime
  suspect (revert staged in source but unbuilt). Bisect after builds
  work again.

## RESUME AFTER REBOOT (read first)

- Reboot clears processes but not disk: repo, docs, `src/hooks.cpp`,
  logs, and managed skills all persist. `/tmp` does not (nothing
  critical there; GDB one-liners are recreatable from this file).
- Binary state: `build/native/fable_ii` is 19:10 (contains pool
  wait-retry + SLOWALLOC + all probes to date). Source has MORE:
  retry REMOVED (double side effects), `378420`/`A43728`/`82354980`/
  `5BAFC0`-lr/`823FE828`-family/`82CBD098`/`8231E908` brackets,
  per-site `GT2`/`GTLOCK`, change-based `LOCKW`, capped everything.
  MUST rebuild after reboot (`touch src/hooks.cpp` first!) — the new
  binary supersedes 19:10.
- First three actions after reboot:
  1. Rebuild, launch 150 s offscreen, confirm no early SIGBUS (retry
     removal fixed it?) and 0 faults (mutex holds?).
  2. Watch for `356180`/`378420`/`VIRTRET`/populate: GameThread's
     deepest reach after all skips.
  3. If drain still never empties: synthesize `823784A0`-return-0
     one-shot (DRAINVIRT hook) and observe populate-then-menu.
- Key facts: lock `0x4C11E740` (3D=0x30097018 holds); flag
  `0x42205148` stuck 01 (synth insufficient); item `0x42205020`
  vtable `0x820A7E20`; pool slot size-32 `0x40021890`-family empty;
  G4 preseeds + mutex + skips active; no scratch (removed).
- Do NOT re-add: pool wait-retry (double side effects), scratch-null
  (infinite fake-pending), drain-verdict-0 (shutdown flag), 3D-lockskip
  without store (broke `[r1+88]`, 26k faults).

## Post-reboot 2026-09-09 ~19:52 (fresh binary, retry reverted in source)

- Reboot fixed codegen: full `scripts/build` linked cleanly at 19:52
  (binary newer than touched source; environmental SIGBUS confirmed).
- 150 s offscreen (`fable_ii_296.log`): 0 faults, no early SIGBUS —
  retry removal (or reboot) resolved the bank crash. Drain verdict 1
  once, bank-proc once, `LOCKW` balanced (count=0).
- SDK per-run log stops seconds after launch (AllocFixed spam tail);
  `logs/runtime-console.log` (probe log) is the truth for long runs.
- First post-reboot run quieter than pre-reboot storms (cold caches?);
  300 s run launched 19:55 to see if drain storm develops.

## Populate breakthrough 2026-09-09 ~20:05-20:15 (DRAINVIRT + gate removal)

- One-shot `822F27C0`-verdict-0 (DRAINVIRT) routes the sequencer into
  populate `822F2608`, which executes fully: `8236CC90` (now returns),
  `8236CA90`, `8236C940`, real producer `82A47D48` (returns via
  `82CBB620`; `82AA8AD0` branch skipped), tail `829FF648` + sequencer
  steps (`8217E3F8`, `822F0518`, `82309F00`, `822EB0C8`, `822F2518`).
  0 faults in every post-reboot run.
- `822F2608` shape (ppc-disasm): `[r31+72]`-gated calls
  `8236CC90`/`8236CA90`/`8236C940`, then virtual `[r3]->[0](r4=1)`,
  then `822F5718` + `829FF648` x2.
- `82A47D48` shape: byte at `0x83496C3A` nonzero continues (calls
  `82AA8AD0`/`82CBB620`); zero escapes to `0x82A48894`.
- Gate (`82200688` lr==0x8236C588 120 s block) REMOVED: it deadlocked
  populate (`8236CC90` needs a 3D-held lock; GATE-TIMEOUT observed).
  Never re-add without evidence.

## Natural populate + slow tail 2026-09-09 ~20:40-20:47

- A 240 s run reached populate WITHOUT synthesis (verdict naturally 0;
  `822F27C0` hook printed nothing — `822F2608` may also be reached via
  another sequencer path). `g_pop_active` never set; tail `9648`s show
  as `seq-step`.
- Gate removal validated: `8236CC90` takes `0x4C11E740` while 3D holds
  it (LOCKW count=1 owner=3D) and RETURNS (CC90-EXIT). Orig lock handles
  contention; the gate was pure harm.
- `822F5718` RETURNS (EXIT observed) but `822F71A8` inside it takes
  MINUTES at 605% CPU (asset work under llvmpipe?); log goes silent
  (probes sparse there), then resumes. Not a stall: high CPU + return.
- Post-5718 tail (`9648` x3, vtable+0, `9648`, `82356698`, vtable+8/0
  chain, tail-call `82CA2C3C`) still unobserved — run hit its bound one
  LOCKW past 5718-EXIT. 0 faults. 600 s run launched 20:47.

## Vtable stall isolated 2026-09-09 ~20:58 (10-min run, 0 faults)

- 600 s run ends at the same frontier: `822F5718`-EXIT, then silence.
  Post-5718 `9648` x3 would print `seq-step #269` (mod-4 arithmetic);
  absent, so GameThread stalls AT the vtable+0 `bctrl` (`822F269C`,
  r3=[r31+80], r4=1) or never reaches it. No fault, other threads burn
  CPU (605%).
- `sub_822F5718` now resolves the target at runtime (r3entry-108=r31;
  [r31+80]->[0]->[vtable+0], reads only) and logs `VTABLE+0 ...`.
  240 s run launched 20:58 to capture it.

## Tail flows past the vtable 2026-09-09 ~21:10 (site-filtered proof)

- Per-site `829FF648` logging (tail `bl`+4 LRs always print) proves all
  four tail calls fire: `822F2680/2688/2690/26BC`. The vtable+0 is
  skipped ([r31+80]==0) and execution CONTINUES (`9648(r29)` runs).
  The earlier "stall" was a mod-4 sampling artifact polluted by the
  hot `824EF2F4`-loop caller. Lesson: shared counters across threads
  lie; filter by LR.
- Populate's `6698` call entered (internal `9648`s from `823566C8`).
  Frontier now inside/after populate's `6698`: vtable+8/0 chain and
  tail-call `82CA2C3C` (latter NOT hookable: no `__imp` symbol, link
  error — removed the probe).
- `822F2608` now logs its caller LR every call (natural-populate path
  bypasses `822F27C0`: no VERDICT lines in natural runs). 300 s run
  launched 21:13.

## Populate completes 2026-09-09 ~21:30 (300 s, 0 faults)

- Populate-phase VTAIL (at `6698`-EXIT lr=`822F26C4`): all four slots
  NULL — every post-`6698` virtual in the tail is a null-guarded skip.
  `82CA2C3C` disassembled: shared restore+`blr` epilogue, so populate
  RETURNS to the sequencer (`822EAA90`); the stall theory is dead.
- Natural populate caller: `822EAA8C` (stores 1 to `[0x83496898]`);
  sibling branch `822EAA74` stores r28 + calls quit-setter `822F4690`.
  `822F2608` logs caller LR every call now.
- Drain-stall todo CLOSED: drain -> populate -> full tail -> sequencer
  continues, LOCKWs balanced, 0 faults in all post-reboot runs.
- Next frontier (new work): post-populate sequencer toward menu/title.
  No 82CA2C3C hook possible (no `__imp` symbol); probe the sequencer's
  next known calls instead. DRAINVIRT synthesis is OFF (bisect: it
  skips the intro); the hook is a pure verdict reporter now.

## Intro bisect resolved 2026-09-09 ~22:17 (user-observed)

- With DRAINVIRT synthesis OFF, the intro plays (visible + audible,
  user-confirmed). The `822F27C0`-verdict-0 force SKIPS intro/drain
  content — the drain loop carries the intro items. Never re-add the
  force for normal runs; natural verdict-0 (via `822EAA8C`) still
  reaches populate on its own (proven in earlier 240 s runs).
- Visible window now persists (SDK keep-open patch: don't quit the app
  when the guest entry thread returns ~4 s in; workers own the boot).
  Rebuild note: `rexui` (ReXApp) compiles into the game binary — Game
  rebuild picks up `rexglue-sdk/src/ui/*` edits; `scripts/build-sdk`
  alone does not.

## Post-populate sequencer probes 2026-09-09 (drafted, not yet built/run)

- Static chain (worktree `tooling/ppc-disasm` + guest-image.bin bl scan,
  main-repo generated/ read-only for DEFINE_REX_FUNC):
  - `sub_82CBB788` [82CBB788,82CBB964] calls the sequencer branch
    `822EA8C0` at `82CBBB08` (sole image-wide caller); on return
    (LR=`82CBBB0C`) it does `mr r30,r3; bl 82CA97B8`, then import-thunk
    calls (`832B26CC`, `832B230C`, not hookable), ending with blr at
    `82CBB964`. Its only static caller is its own guarded recursion
    (`82CBB9B0`), so first entry arrives indirectly (funcptr/thread).
  - `822EA8C0` body calls, in order: `82CBB638`, `82CBB570`,
    `821E6388`, `82CA34B0`, `82196C58`, `82CBBF60`, then blr.
  - Populate (`822F2608`) has exactly two callers, `822EAA74`
    (drain path, after `822F47F8`+`822F4690`) and `822EAA8C`
    (direct path, stores 1 to `[0x83496898]`), both inside the large
    `sub_822EA928` sequencer, both followed by frame teardown +
    `b 82CA2C38` (shared restore+blr epilogue, not hookable).
  - `sub_822EA928` itself has zero direct callers (indirect entry).
- New probes in `src/hooks.cpp` (first-hit entry+exit, LR, passthrough
  only; existing hooks untouched):
  - `82CBB638` (`branch-entry`): sole caller `822EA8D4`, first call in
    the branch body. Reveals whether the sequencer branch runs at all.
  - `82CA97B8` (`post-branch`): sole caller `82CBBB10`. Reveals whether
    the branch RETURNED and the caller advanced past LR=`82CBBB0C`.
  - `82CBB788` (`chain-head`): reveals the indirect driver via entry LR
    (no static external caller exists to name it).
  - Rejected: `82CBBF60` (91 callers), `821E6388` (105), `82CA34B0`
    (8) — shared utilities, first hit would mislead; `832B26CC` /
    `832B230C` — import thunks without DEFINE_REX_FUNC.
- Proposed post-populate sequence (to confirm from log interleave with
  `822F2608 populate-dispatch #n` + `822EA8C0-ENTER/EXIT` counters):
  `822F2608` returns -> `82CA2C38` epilogue -> `sub_822EA928` returns to
  its indirect caller -> `sub_82CBB788` chain-head (entry LR names the
  driver) -> `822EA8C0` branch body (`82CBB638` first) -> branch return
  -> `82CA97B8` continuation. If `82CA97B8` hits land after
  populate-EXIT, the branch runs post-populate and its LRs give the
  next frontier toward menu/title.

## Probe volume regression 2026-09-09 ~23:03

- The 600 s headless run showed one verdict and no populate. Prime
  suspect is NOT game state but MY uncapped logging: `829FF648`-POP
  every call (hot `824EF2F4` loop, hundreds/s), `VTAIL` 4 lines/call,
  `VTABLE+0` every `5718` call — stderr volume I/O-stalls the boot.
- Throttled: `seq-step` every 64th, `VTABLE+0` first 2 `5718` calls,
  `VTAIL` first 2 `6698` calls + always the populate-tail call
  (lr=`822F26C4`). Tail-site `829FF648-TAIL` logging kept (rare).
- GDB watchpoint side-note: flag `[0x42205148]` 0->1 writer is
  XrnmThread via `824E3350 <- 82CA3388 <- 82CA3430 <- 82CCA400`
  (HYPOTHESIS: producer-side set; the drain consumer's clear is the
  unobserved half). GDB timing also reproduces a 3D-only SIGSEGV at
  `821E27C8:1049` (`8236C360` chain) absent from normal runs.

## Self-driving drain 2026-09-10 ~01:21 (repeating synths)

- One-shots exhausted after the first drain item (sequential items reuse
  the same slots). All three synths now repeat on cooldown (flag 90 s,
  gate 120 s, spin 120 s). The machine grinds items unattended:
  SPINSYNTH -> VIRTRET -> populate-branch observed; cycle 2 in flight.
- Post-loop spin scale: 135M `822F3640@78834` iterations (b5=00) before
  the spin synth — tight busy-spin (sleep skipped), ~225k/s. Harmless
  but log-heavy (2M+ lines/run; greps slowing).
- Correction: V16 (831FD318) was NEVER the wait-loop gate (no
  lr=823787F4 calls); the gate is 82185418 ([0x8349E6EC]+16, zero-test
  on [obj+44]). V16/V20/V24/V28 lr-filters stay as traffic markers.
- Cycle-1 note: flag+gate opened WITHOUT synth (not always stuck);
  the spin byte ([drainctx+5]) is the consistent blocker. SPINSYNTH
  sets it; VIRTRET follows; 0 faults.
## Stop point 2026-09-10 ~01:45 (resume: grind drain cycles headless)
- State: repeating synths (flag/gate/spin, 30 s cooldowns) grind drain
  items unattended (VIRTRET per item, 0 faults). Sequencer map proves
  populate is unconditional once the verdict path returns; GameThread
  parks only inside callees (the drain loop). Just needs more cycles.
- Next: long headless run (1200 s+), watch for populate-dispatch #n,
  then 82CA97B8/menu-leg probes, then visible title check.
- Do NOT re-add: DRAINVIRT force (skips intro), pool retry, 3D gate.
- SDK patch (keep-open, in binary via game rebuild) + tracked diff in
  docs/re/patches/sdk-keep-open.patch.

## Grind relaunch + branch-thread map 2026-09-10 ~12:45

- Bare `scripts/run` died in 0.25 s (`Failed to load libvulkan.so.1`):
  runs MUST go through `nix develop path:. -c env RUN_SECONDS=…
  scripts/run` (runbook already says so; the bare env lacks the loader).
  1800 s offscreen grind relaunched under nix (~13:15 expected end).
- Archived 01:45 log (logs/console-archive-20260910-0145.log, 1M lines,
  0 faults): chain-head `82CBB788` x1 (lr=`82CBB9B4`), branch
  `822EA8C0`-ENTER x1 (lr=`82CBBB0C`), `82CBB638` entry+exit, then branch
  never returns. STRONG INFERENCE: outer `82CBB788` entered via raw guest
  address as a thread start, BYPASSING the wrapper probe (its entry was
  never logged; only the guarded-recursion `bl` at `82CBB9B0` routed via
  the wrapper, lr=`82CBB9B4`). Wrapper probes are blind to thread-entry
  invocations — EXIT lines undercount still-parked outer frames.
- Branch body (recomp.223.cpp:2214) decoded: `82CBB638` (returned) ->
  `82CBB570` (trivial getter, `r3=[0x82000968]`, cannot park) ->
  `821E6388` -> `82CA34B0` -> `82196C58` -> `82CBBF60`. Park is at/after
  `821E6388`.
- `821E6388` (recomp.121.cpp:667) is CONFIRMED a strstr-like pure routine
  (`r4`=`"waitformem"` at 0x8208FB10, guests strings table); returns fast,
  not the park. `[0x82000968]`=`0x30008000` in the static image.
- Next (after grind run, one variable): LR-filtered entry/exit probes on
  the four shared branch callees (`821E6388` lr=`822EA8E8`, `82CA34B0`
  lr=`822EA8FC`, `82196C58` lr=`822EA908`, `82CBBF60` lr=`822EA910`).
  All are DEFINE_REX_FUNC starts (`nm` proves `__imp__` text symbols
  exist, e.g. `__imp__sub_821E6388`); the `82CA2C3C` link failure was
  mid-function-label-only. LR filter keeps the 105/91-caller utils quiet.

## Spin predicate decoded 2026-09-10 ~13:00 (trace-only run A in flight)

- Loop (recomp.250.cpp `82378800/20`): poll `822F3640`; low byte != 0
  exits; else `82CBC6B0(100)` then re-poll. `82CBC6B0` = r4=0 +
  tail-call `82CC2028`.
- `82CC2028(r3,r4)` (recomp.87.cpp:25195): interval = r3*(-10000) 100 ns
  units (r3=100 -> 100 ms relative); single `KeDelayExecutionThread`
  then exit iff `(r4&0xFF)==0` else re-wait on status 257. r30 comes
  from the REGISTER (`clrlwi r30,r31,24`), not a `[0]` memory read —
  the SKIP-NULLWAIT comment rationale (zero-page garbage parks null
  waits forever) does NOT match the generated code; null-object wait =
  one 100 ms kernel delay + return. HYPOTHESIS: comment is stale (older
  codegen emitted a load) or describes a different path. VERIFY before
  trusting either skip.
- Two stacked TEST skips remove the pacing: SKIP-GTSLEEP (82CBC6B0,
  lr=`82378828`) + SKIP-NULLWAIT (82CC2028, null obj). Result: ~7M
  polls/s hot spin (SPIN34 #1.4B in 300 s) + ~110k log lines/s I/O
  flood. Original design polls at 10 Hz with alertable kernel waits
  (DPC/APC delivery points). Whether this starves the [drainctx+5]
  writer is UNKNOWN — revert candidates, one variable at a time.
- SPIN34 sampler throttled to change-only + 2^20 heartbeat (staged in
  hooks.cpp, not yet built): the flood itself perturbs scheduling.
- Bink CONFIRMED statically linked (RAD strings 0x820FF080+):
  `GAME:\data\art\videos\microsoft_logo.bik`,
  `lionhead_logo.bik` (0x820A1860/8C) + in-game `videos/*.bik` table
  (0x820A7C34+). No `BinkOpen` symbol (static link, stripped).
- #387 lost-wakeup fix PRESENT: pinned SDK v0.10.0 contains 96bee61.
  ReXGlue-side thread-start race ruled out at this revision.

## Completion-signal hunt 2026-09-10 ~13:00-13:35 (APC theory dead)

- Writer map (GDB hw watchpoints, host=0x100000000+guest, both aliases):
  - `[drainctx+52]` = return of `82CA34B0`, stored by the verdict builder
    `822F27C0` (`stw r3,52(r27)`, recomp.276.cpp). Value F80000E0/FC =
    worker THREAD HANDLE (82CA34B0 -> 82CBD280 -> 82CC8758 ->
    ExCreateThread(start=82CA3430, 256 KB, suspended) -> resume same path).
    Second 82CA3430 thread runs the 82B41738 job loop (alive, churning);
    it never enters 822EA928 (one SEQENTER per boot, GDB-proven).
  - `[drainctx+5]` has NO writer in 220 s of watched boot (virtual +
    physical aliases). Verdict's `stb r28,5` always stores 0: r28 is
    `li 0` at 822F27C0 head, never reassigned (CONFIRMED full-body scan).
    Only other writers ever: 0xBE stack-fill clobber (pre-constructor,
    self-healed) + 822F2518 constructor zero. +5=1 never exists.
  - Gate byte [4011F250+44]: NEVER written (110 s watched). Gate closed.
  - "Flag" [42205148]=01 is NOT a flag: it's a CRITICAL SECTION's
    signal_state, set by RtlInitializeCriticalSection from 82A496A8 <-
    8238DF78 <- 823781A8 <- 822F47F8. Old FLAGSYNTH corrupted a live CS.
  - `82CBB570` CONFIRMED trivial (`r3=[0x82000968]`, 3 insns): the branch
    parks at/after `821E6388` (strstr "waitformem", pure). Sideline.
- work-flag [42100010+68]=1 set once at xstart (82B40A80); worker pool
  consumes normally. Pool is healthy; the drain item was never completed
  through it.
- 822F3640 CONFIRMED: lock [r31+8]; needs [+52]!=0 AND [+5]!=0 (consumes
  +52, returns +5); RtlLeaveCriticalSection; return. Two-byte AND gate.
- APC/starvation theory DEAD: SKIP-GTSLEEP + SKIP-NULLWAIT reverted
  (native 100 ms alertable KeDelay restored), 150 s run => IDENTICAL
  stall (VERDICT 1, b5=00, 0 faults), just parked at the gate instead of
  hot-spinning. The old SKIP-NULLWAIT rationale was wrong (r30 comes
  from the register `clrlwi r30,r31,24`, not a `[0]` load).
- First drain item identity (deterministic x3): item 42205020, vt
  820A7E20 (virtuals: empty stubs 82C43198/829CE870 + drain-region
  continuations 82378868/8AC0/BB0/BB8/C78), drainctx at item+0x5C
  (=822EA928 stack frame r1+96, NOT heap). Speech-bank neighborhood
  strings (speech.bnk/adb) adjacent to vtable.
- Next: A/B natural-EOF vs normal-input-skip (needs visible run B with
  user keypress); then Bink EOF lifecycle vs Xenia.

## Bank-mutex deadlock + 3D fault transient 2026-09-10 ~13:45-13:55

- Live-stack snapshot (interactive GDB, 75 s in) caught Permanent Bank
  thread SELF-DEADLOCKED in our hooks: 82BCA340 wrapper holds
  `bank_table_mutex` (hooks.cpp:908) while reentering 82BC9E10 wrapper
  (hooks.cpp:912) on the same thread. Same-thread holder+waiter,
  CONFIRMED from one backtrace.
- No-mutex trial: Permanent Bank unblocked, but 3D Engine thread faults
  11-16k times on read [0x14] at 821E27C8 (recomp.235.cpp:1049, 3d-proc
  chain) in a ~0.5 s burst ~90 s in; game stays alive after (GPU-IRQ,
  GATE16 continue). Recursive mutex does NOT suppress it: the plain
  mutex "worked" only by freezing bank state via the deadlock.
- Drain stall IDENTICAL in all three mutex states (VERDICT 1, b5=00,
  w52=worker handle, 0 faults outside the burst). The bank/3D issues
  are orthogonal to the +5 completion stall so far.
- Current binary (recursive bank mutex, native pacing, trace-only) is
  the run-B candidate: intro window (first ~30 s) is clean in every run;
  the burst comes much later. User warned to weight the first minute.

## A/B outcome + job architecture 2026-09-10 ~14:05

- A/B: skip half IMPOSSIBLE. User pressed everything (MNK + controller):
  intro plays to last frame, no input skips it. Treat as authentic 360
  behavior (HYPOTHESIS). Natural-EOF half CONFIRMED x4 (3 headless + 1
  visible run B, same bytes: VERDICT 1, vt 820A7E20, b5=00). The stall is
  display-independent; the completion must come from natural EOF handling.
- Input note: `mnk_mode` defaults false (START=X/Return, A=`;`/Space only
  when enabled). mnk run authorized but moot if nothing is skippable.
- 82CA34B0's r3 in the verdict call is 0x822F33B8 (NOT 0x823F33B8; lis
  arithmetic corrected: -32209 -> 0x822F high). It submits the bank-load
  JOB (fn=822F33B8, size 0x40000) to the pool; +52 = worker thread handle.
  Worker (start 82CA3430, ctx 82CCA400, resumed by 82CC1610) joins the
  82B41738 pool loop alive. Whether it dequeues THIS job is unproven.
- Next: prove whether any pool thread enters 822F33B8 (GDB entry log by
  host thread, no rebuild), then Bink EOF -> close -> +5 chain or the
  job-completion store.

## Baseline restored 2026-09-10 ~14:23 (plain bank mutex back)

- 90 s headless: 0 faults, VERDICT 1, b5=00, w52=E0. Known stall state.
- Storm mechanism CONFIRMED from SDK source: ReXGlue's posix signal
  handler falls off the end when no handler claims the fault, so the
  kernel resumes the faulting PC -> infinite signal loop (32k faults/s).
  Any unhandled guest fault becomes log-destroying; rotation (20x5MB)
  eats all boot content within ~20 s. Storm onset varies (~50-100 s).
- Physical Xbox 360 controller IS present (SDL OnControllerDeviceAdded,
  045E:028E). User's skip presses likely registered at the HID layer;
  unskippable-intro reading strengthened (still HYPOTHESIS: XAM mapping
  unverified).
- Noisy import tracing (--log_noisy + --log_verbose) emits NOTHING usable:
  no NtCreateFile success lines; storm rotations dominate. File-level
  Bink lifecycle still needs the GDB path decoder or a hook.

## Worker unblock refuted 2026-09-10 ~14:45

- Recursive mutex lets the verdict worker past the bank deadlock, but
  +5 STILL never sets (DRAINCTX b5=00, no populate, 150 s). The deadlock
  was A blocker, not THE blocker: the worker parks again further along
  (past 82BC9E10) or never reaches 822F33B8's +5 store.
- Current binary (14:43) is recursive-mutex: worker unblocked, storm
  possible but INTERMITTENT (a full 150 s GDB run just had ZERO faults
  with the drain at SPIN34/b5=00). Keep this binary for run C; do NOT
  revert to plain without new evidence.
- Storm watch: a full 150 s GDB run just completed with ZERO faults and
  the drain at SPIN34 (b5=00). The 3D [0x14] storm is INTERMITTENT
  across runs, not deterministic. Fault-PC capture configured for it
  (filters on host 0x100000014) but the storm didn't fire that run.

## Worker identity + parked shape 2026-09-10 ~15:20 (long run grinding)

- Verdict worker (gid 3009C018, deterministic) IS the "Permanent Bank"
  audio thread (host comm confirms). 0% CPU for 10+ min: BLOCKED in a
  wait, not spinning. Unbalanced frames: 822F33B8 -> ... -> 822C05F8 ->
  822C0568 (lr=822C062C), whose bctr target (entry r4) never returns.
- 822C0568 body: drain-queue loop on 83000200 (slot [0x83321A8C]
  dispatch) + final indirect call. Worker past the fills (138 buffers
  50A40010..50A544D0, then stops), now in the bctr wait.
- Shape looks like GameThread<->Bank circular wait: drain needs +5 from
  the job; job's tail needs a wait that only post-drain (or audio-side)
  progress signals. CBDRAIN counter probe built (15:16 binary) to
  separate loop-spin from parked-target; runs after the 1500 s grind.
- 822F33B8's own tail holds the +5 store (`stb r11,5(r28)`,
  recomp.284.cpp) — the job KNOWS how to complete; it never gets there.

## Null link + R29FIX progress 2026-09-10 ~17:30

- Fault PC nailed to the instruction (aligned objdump): 822DF280 does
  `r11=[r29+20]; r10=[r11+8 or +4]; r22=[r10]` and faults dereferencing a
  null link in the bank-descriptor list. r29 itself is VALID (4F640220);
  an earlier r29==0 reading was wrong (misaligned disasm + addr2line
  drift). The real fault: null link field, not null base.
- R29FIX (restore r29 across 8219F010 when clobbered; fired once:
  in=4F640220 out=0) moves execution PAST the [0x14] fault to a LATER
  fault at [0x10] (+0x3bc1d10): `r22=[r10]` with [r10]==0, i.e. a node
  whose first word is zero (allocated but never filled).
- So the bank list the worker walks contains an EMPTY node: loader filled
  138 buffers then stopped; the walker finds a hole. Candidates: (a)
  loader quota reached but a fill failed silently (file read error ->
  zero node); (b) walker overruns by one (off-by-one); (c) node freed
  early (use-after-free); (d) link filled by a step that never runs.
- SDK TEMP-DIAGs live: fault PC+offset (mmio_handler.cpp, tracked in
  docs/re/patches/sdk-fault-pc.patch). Revert when storm is solved.
- Fills hold at 138 even over 25 min; +5 never sets; drain never exits.
  Open: Bink EOF chain, Xenia oracle.

## Holding for expert feedback 2026-09-10 ~18:30

- Bank object dump (GDB, deterministic boot): `4F640220` holds valid
  pointers throughout (`[+20]=50640028` intact at rest). Holes appear
  mid-walk, not at rest. Full brief sent to expert; see chat summary.
- Xenia oracle shut down (result banked: gameplay with cosmetic patches
  only). Xvfb :98 retained if needed.
- Current binary: recursive bank mutex + R29FIX + path probes (all TEST).
  No runs in flight. Awaiting direction: bank-list completion trigger vs
  Xenia-differential vs expert input.

## Xenia oracle banked + r1 verdict 2026-09-10 ~19:15

- Xenia (edge AppImage, Lavapipe, same files, cosmetic patches only)
  boots Fable II into 3D gameplay (screenshots). Our stall is definitively
  ours. Oracle shut down; logs/screenshots retained.
- r1 verdict: worker's 8219F010 returns with r1 +0x3D0 (704FF300→704FF6D0)
  and r29/lr zeroed. All 6 direct callees balance r1 (R1BAL probes, zero
  hits); 8219F010's own stwu/addi balanced; save/restore helpers correct
  in isolation. The +0x3D0 must come from the single indirect call
  (8219F010's bctr) or deeper nesting. No 976-byte frame on the probed
  path; 5 such functions exist elsewhere, none observed on worker path.
- Two bounded steps concluded: (1) [0x7C] fault = null table [r31+16] on
  first iteration (loader hasn't built it); (2) loader stops at 138 fills
  by its own quota while the walker finds holes. Next: bctr-target
  identity (GDB, no rebuild) or expert input on r1+0x3D0.

## Context-restore root cause + resume-jump fix 2026-09-10 ~21:30 (CONFIRMED)

- R1LEAK descent (each +0x3D0 exact, chained in/out pairs) converged:
  8219F010(outer, from 822DF280:lr=822DFEC4) -> bctr 82BB6CA0 ->
  82BC6A18 -> 82BC9788 -> 82BCCB88 -> 82CA9260 (+0x440; 82BCCB88's own
  missing epilogue accounts the rest: -112+1088=+976).
- 82CA9260 is a context-restore: r7=r3=CONTEXT*, restores FP/GPR/VMX/CR,
  `ld r1,144(r7)`, `mtlr [r7+308]`, blr. CTXREST probe: ctx on worker
  stack (704FF510), savedR1=704FF4B0, savedLR=822C05A8 (mid-822C0568,
  just past its `bl 83000200` sleep). Generated code C++-returns instead
  of jumping -> dispatcher continues with resume-r1 -> fault storm.
- On HW the restore is load-bearing: everything after it is unreachable
  (82BCCB88 genuinely ends `bl 82CA9798` + padding; the path only works
  because the restore jumps away). Neutralize-and-continue disproven:
  it entered unreachable code, new fault guest=0x8 at 8219F010/loc_8219F284
  (r31=0, clobbered nonvolatile on the dead path).
- Fix (hooks.cpp-only, TEST reversible): 82CA9260 override runs the body,
  then runs the 822C05A8 resume inline (copied loc_822C05A8..blr) and host
  longjmps to a setjmp buffer in the ancestor 822C0568 wrapper, abandoning
  the dead chain. Gate: saved-LR==822C05A8 and buffer armed, else fallback
  passthrough + R1LEAK log. Outermost 822C0568 wins the thread_local buffer.
- Result: RESUMEJUMP x2 (deterministic values), RESUMED ret=2, ZERO guest
  faults over 150 s / 12.3M console lines. Storm (0x14) gone. Game advanced
  to new frontier: FATAL call to undiscovered thunk 0x829FCB00.
- 829FCB00: vtable-style thunk (lwz/mtctr/bctrl + loop, blr at 829FCBA0).
  Manifest entry added [entrypoint.functions.829FCB00] end=0x829FCBA4
  (sibling thunk at 829FCAE8 shares the tail; add if it FATALS).
  Manual codegen (44 s) + rebuild: next 150 s run reached time limit with
  0 faults, worker in new regions (8220893C/8217ABA4/82C63860), sequencer
  82CBB788 x2, drain-verdict x1, vtable tail 829FF648 active.
- ENV/DEBT (must not lose): (1) /dev/shm fills with xenia_memory_* per
  KILLed run -> startup SIGBUS in Memory::Initialize; rm them when runs
  die at ~2 s. (2) Codegen SIGBUSes under ninja (VA-layout luck); bypass
  in generated/rexglue.cmake (TEMP-DEBUG-BYPASS, ignored file, local
  only): codegen runs MANUALLY (`./rexgluerd codegen`), builds skip it.
  (3) Regen wiped temp gen patches (BCTRTGT/CTXREST served purpose).
  (4) Resume-jump uses host longjmp across ReXGlue frames (Tracy/fiber
  caveats); chain-abandon leaks dead C++ frames per wake (rare: 2/run).
  Proper fix = codegen mtlr+blr->indirect-jump + mid-function targets.
- Next: grind the new frontier queue (undiscovered functions as FATALs
  arrive), then populate/menu/title per the headless plan.

## Frontier grind + worker park 2026-09-10 ~22:00
- Post-resume runs advance minutes then FATAL on undiscovered vtable
  thunks. Cleared: 829FCB00, 82C0B400, 82C09188, 82C4C320, 82C4C5C8.
  Twin/triple slot groups sharing a tail are ADJACENT manifest entries
  (codegen rejects overlaps); hooks.cpp THUNK_CHAIN overrides mirror HW
  fall-through-on-return. Sibling slots (82C4C340, 829FCAE8) pre-added.
- scripts/grind-frontier automates run->FATAL->decode->manifest->codegen
  ->build (refuses insane/overlapping/branchy targets for manual review).
- New stall (no faults, worker silent): worker parks deterministically in
  82C65D80 (SAVE26 lr=82C65D88, same r1 both runs). No backward loop in
  82C65D80 -> blocked in a callee. PARK_PROBE (every worker entry/exit)
  on 82CA3700/82366210/822F54C8; 82200688 already has a wrapper (by
  elimination if the three stay balanced). ptrace denied, so no live GDB.
- Debt unchanged: codegen bypass (re-apply after each cmake configure:
  configure rewrites generated/rexglue.cmake), shm cleanup per run.

## Perf + GPU triage 2026-09-10 ~23:15
- Silenced all 196 hooks.cpp probes (PROBE_LOG no-op): FPS still ~1/s,
  so rendering (llvmpipe) dominates, not logging. Resume-jump + thunk
  chains kept (no logging in them).
- NVIDIA attempt (RTX 3060, host driver 610.57): nix loader + staged host
  libs in /tmp/nvk (libGLX_nvidia, glcore, glsi, glvkspirv, allocator,
  gpucomp, tls, libvulkan) get vkCreateInstance working with HOST
  libvulkan, but the game renders BLACK with GPU at ~66% and VRAM
  leaking 1.6GB -> 8.9GB. ReXGlue GPU emulation + NVIDIA looks broken;
  llvmpipe shows the legal/title screens correctly. Full notes for the
  GPU worktree: host ICD /usr/share/vulkan/icd.d/nvidia_icd.json,
  custom /tmp/nvk/nvidia_icd.json, launch outside nix with
  LD_LIBRARY_PATH=rexglue-sdk/out:/tmp/nvk:<nix-stlibs> (never prefix
  /usr/lib: glibc conflict). Do NOT run two games at once (shm/CPU).
- New-game path playable to FATALs: 822142D0, 82267C88, 82E8F9E8,
  82267568 (tiny getters, all registered). Visible run in progress.

## Night stop 2026-09-11 ~01:30 — title + Bowerstone load, M7/M8 edge
- M7 TITLE CONFIRMED visibly (FABLE II logo + "Pour commencer, appuyez
  sur A" + full 3D scene). Demo/attract loop identified (not gameplay).
  User drove: A (menu) -> A (new game) -> left (boy) -> A (confirm) with
  a physical controller; reached Bowerstone loading screen (tips cycle,
  3D character renders). Load never completes in-run: 0x34 fault loop
  in twin-thunk slot 82C4C340 (13k-300k faults, one thread).
- 0x34 analysis (CHAIN tracking): faulting calls are DIRECT vtable
  dispatches to C340 (silent: tls=0, non-worker), not the C320 chain
  (1 logged chained instance had r11=0 too). [r3+4]=0 on a heap object:
  consumer racing an unfilled field; every prior race self-resolved.
  Verdict: infinitely SLOW, not infinite (producer render-starved at
  ~1 FPS). Window shrunk to 960x540 (survived, still loading).
- vpad (scripts/vpad.py): virtual Xbox 360 pad via uinput (VID 045e),
  buttons+dpad (AbsInfo(0,-1,1,0,0,0) form required; persistent process
  takes line commands on stdin). Long holds (3-5 s) required at 1 FPS;
  taps fall between frames. Start breaks demo->title; A exits demo->menu
  (menus return to attract if idle ~30 s: screenshot quickly after input).
- Current binary is NOISY (PROBE_LOG re-enabled for 0x34 replay) and
  SLOW. First job tomorrow: re-silence, then long quiet load attempt.
- Frontier queue added: 822142D0, 82267C88, 82E8F9E8, 82267568 (tiny
  getters), 825EF568, 82996258. scripts/grind-frontier automates the
  cycle (refuses insane/overlap/branchy targets).
- Resume point: relaunch visible quiet run, vpad drive (start:3000,
  wait title, a:5000 xN with <15 s checks), Bowerstone load needs
  30-60+ min at 1 FPS. Consider: smaller window, release build, attack
  the 0x34 producer if load still never completes.

## Autonomous drive + THUNK_CHAIN removal 2026-09-11 ~15:00-17:30

- Re-silenced (PROBE_LOG no-op, quiet binary 15:19). Xvfb :98 + vpad drive
  works fully autonomously: title -> tap-burst A -> menu -> A (nouvelle) ->
  A (continuer sans sauvegarder) -> left (boy) -> A -> Bowerstone load.
  Screenshots verify every step. Per-press reliability ~1/3 at ~1 FPS;
  3x `a:1500` tap bursts beat single holds (holds up to 15 s ignored).
- vpad FIXES (scripts/vpad.py): (1) added missing HAT dict (NameError on
  left/right/up/down); (2) added full 360 stick axes (ABS_X/Y/RX/RY/Z/RZ):
  the buttons-only pad never produced SDL OnControllerDeviceAdded (game
  never saw it); with axes SDL classifies it "Xbox 360 Controller"
  is_gamepad=1 mapping=yes (tooling/sdl-pad-probe.c verifies against the
  SDK's own SDL3; needs nix systemd-minimal-libs libudev at runtime, never
  /usr/lib: glibc conflict). Pad enumerates at boot AND via hotplug.
- ADVISORY-CONFIRMED BUG: THUNK_CHAIN (hooks.cpp) was semantically wrong.
  C320/C340 end in bctr (not bctrl): target inherits caller LR, blr returns
  to caller; HW never falls through to the adjacent slot. The macro
  unconditionally re-ran the next slot with clobbered registers after every
  call — fabricating the 0x34 storm ([r3+4]=0 -> [0+52]=0x34 read in the
  chained C340). DELETED all 6 chains + tls tracking (hooks.cpp:1190-1225);
  corrected manifest comments (entries kept: real independent slots).
  Result: ZERO 0x34 faults in every chain-free run (was 13k-300k/run).
  The "infinitely SLOW" verdict is void: the storm's signal-handler churn
  was the suspected starver, and it was ours, not the game's.
- Frontier grind since (run->FATAL->decode->manifest->codegen->build,
  all bctr-thunk arrays registered contiguously after bounds scans):
  82C4C300, 82C4C2C8, 82C4C2E8, 82C4C2A8 (C2 array now C2A8-C360);
  82C4C5A8, 82C4C588 (C5 array now C588-C630); 8274B798 (li r3,0x43);
  82988E98-EF48 (12-slot array); 8274B728-788 (7-slot array);
  82EAA980 (guard+dispatch) + 82EAA9A8 (setter); 82EC1918, 82E87A10,
  82B56870 (guarded dispatch), 824D56C8, 822D6678. Manifest now 60+ entries.
  grind-frontier canNOT auto-decode these (decoder seeks blr 0x4E800020;
  thunks end bctr 0x4E800420 -> MANUAL REVIEW). Run 512 (binary 17:19)
  is past every prior FATAL at new-game load with 0 faults: long quiet
  load observation in progress under Xvfb.
- Debt: nix store GC re-fetched xwd/imagemagick mid-session (screenshot
  latency). Xvfb :98 + vpad persist via hub; game relaunched per build.
  Route recipe: Start:4000 -> title shot -> 3x a:1500 -> shot -> step A
  presses with shots (popup -> A -> charselect -> left:3000 -> A).

## M8 gameplay 2026-09-11 ~17:45 (run 512, binary 17:19, 66 manifest entries)

- Bowerstone load COMPLETED chain-free: loading vista -> snowy Old Town
  gameplay with boy character, golden trail, objective text "Suivez le chemin
  lumineux pour atteindre votre prochain objectif." Screenshots r15-6/walk1.
- Gameplay INPUT works: new vpad `stick:<x>,<y>[:<ms>]` left-stick drive
  (scripts/vpad.py) walked the boy forward along the path; camera followed.
  vpad was restarted for stick support; the game survived the device
  remove/add (hotplug) without restart.
- Run 512 census at gameplay: ZERO FATALs, ZERO guest access violations in
  all segments. The 0x34 storm never returned after chain deletion.
- Open cosmetic: heavy red/pink tint over Bowerstone prologue (geometry,
  snow, text all render; tint may be area lighting or a shader issue).
  Not investigated; needs Xenia-oracle color compare.
- M9 remaining: audio (unverified headless), save/load (slots full on this
  profile; "continuer sans sauvegarder" path used), longer stability,
  red-tint verdict, release-build perf (~1 FPS llvmpipe).
- Live state: game (run 512, Bowerstone gameplay) + Xvfb :98 + vpad all
  persist via hub. Binary build/native/fable_ii @17:19 is the M8 binary.

## GPU attempt 2026-09-11 ~17:40-18:15 (user asked; llvmpipe stays default)

- Nix-pinned NVIDIA userspace BUILT: `(linuxPackages.nvidia_x11_latest.override
  { libsOnly = true; })` from the locked nixpkgs = 610.57.04, EXACT match for
  the loaded kernel module. Out-link /tmp/nvidia-libs (lib/ + ICD json with
  absolute store path). Repro: NIXPKGS_ALLOW_UNFREE=1 + --impure (unfree).
  No flake change; no kernel build. /tmp/nvk (host-lib staging) and /tmp/nvkx
  (host X11) are SUPERSEDED experiments, not needed.
- ICD negotiation saga, all resolved: nix loader + nix ICD failed inside
  `nix develop` but worked outside it; root cause never isolated to a var
  (159-var bisect: all innocent; env -i segfaults on locale stripping).
  Current recipe that WORKS: VK_ICD_FILENAMES=/tmp/nvidia-libs/.../nvidia_icd.json
  (+ optional VK_LOADER_LAYERS_DISABLE="~implicit~"), LD_LIBRARY_PATH with
  /tmp/nvidia-libs/lib FIRST. Instance + RTX 3060 device creation PROVEN
  (519.log: Using "NVIDIA GeForce RTX 3060", driver store path).
- Offscreen NVIDIA (SDL_VIDEODRIVER=offscreen): 120 s boot, ZERO violations,
  ZERO FATALs, VRAM 183 MiB during / 9 MiB after (no leak). Device +
  command-processor path healthy.
- Windowed NVIDIA on Xvfb :98: deterministic native SIGSEGV ~0.6 s after
  device creation (exit 139), no window, no error log (110-line log ends at
  device line). Under gdb the same run reaches 3D guest code and stops at
  the BENIGN 821E27C8:1049 marching store (debugger intercepts before the
  ReXGlue handler). Suspect: surface/swapchain creation on a GLX-less Xvfb
  (nix Xvfb advertises 22 extensions, no GLX; +extension GLX didn't take).
  NVIDIA WSI needs its X driver in the X server. No video-group membership
  for a real Xorg either (oery lacks `video`; card1 is root:video).
- Combined with 09-10 (:0 real Xorg: BLACK + GPU 66% + VRAM 1.6->8.9 GB leak):
  even where presentation exists, frames are wrong. Next GPU steps need a
  real NVIDIA X screen AND likely Xenos-side work (fragment stores/atomics
  per the vulkan_require_* cvars). llvmpipe remains the correct-rendering
  (slow) default. Parked here; no game-code impact (all experiments env-only).
