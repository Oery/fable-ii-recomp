# Function graph repairs

Evidence: supplied XEX SHA-256 in `entry-xex.sha256`, loaded by SDK
`c94f5ebdcb3c9d1a460ca48e04f9758448f8d518` in tool mode. GDB stopped at
`rex::codegen::phases::Validate`; the mapped image was captured without running
guest code. `tooling/ppc-disasm` uses the SDK's Xenon dialect (including VMX128).
Commands and raw PPC are retained in `logs/unresolved-targets.ppc.txt`.
Addresses remain canonical; no semantic renames, overrides, or binary patches.

## 0x82CD7948 — sub_82CD7948

- Start/end: `82CD7948` / `82CD7958` exclusive (16 bytes); no parent chunk.
- Caller: branch at `82CD795C`, after `r3 -= 4` at `82CD7958`.
- Callees: none.
- Inputs: r3 pointer, r4 output pointer; exact high-level types UNKNOWN.
- Return: r3 = 0; LR unchanged; `blr` at `82CD7954`.
- Side effects: r11 = guest word at r3+0xC; writes r11 to [r4]. No globals.
- Evidence: four instructions `lwz r11,12(r3); li r3,0; stw r11,0(r4); blr`.
  Previous thunk ends in a tail branch at `82CD7944`; next thunk starts `82CD7958`.
- Classification: CONFIRMED code/behavior/boundaries. HYPOTHESIS: an accessor
  reached through a this-adjusting thunk. No C++ signature inferred.
- Open question: why automatic discovery omitted this leaf.

## 0x82C000F8, 0x82C00108, 0x82C00110, 0x82C00118, 0x82C00120, 0x82C00128 — forwarding leaves

- Start/end: `82C000F8` / `82C00100`, `82C00108` / `82C00110`,
  `82C00110` / `82C00118`, `82C00118` / `82C00120`, `82C00120` / `82C00128`,
  `82C00128` / `82C00130` (8 bytes each); no parent chunks.
- Call sites: `82BFDA08`, `82BFDA64`, `82BFDA78`, `82BFDA94`, `82BFDAA4`, `82BFDAB4`, respectively.
- Callees: tail branches to `82C106A8`, `82C0B660`, `82C0B678`, `82C0B680`, `82C0B688`, `82C0B690`, respectively.
- Inputs: r3 pointer; other argument registers forwarded, including f1 at
  the observed callers. Exact signatures UNKNOWN.
- Behavior: each loads r3 from [r3+0xC], then tail branches. LR unchanged;
  return and other side effects are those of the callee. No direct writes.
- Globals: observed callers obtain r3 from guest global `83334A08`.
- Evidence: repeated neighboring 8-byte load-and-tail-branch leaves from `82C000F8` through `82C00128`, explicit
  external direct branches to all six starts, terminating branch in each. Raw PPC retained in `logs/unresolved-targets.ppc.txt:45-56` and verified with `tooling/ppc-disasm` at `82C00120` and `82C00128` (`lwz r3,12(r3); b 0x82C0B688/0x82C0B690`).
- Classification: CONFIRMED code/boundaries and forwarding behavior.
- Open questions: callee types and subsystem identity UNKNOWN.

## 0x82C14D58 — sub_82C14D58

- Start/end: `82C14D58` / `82C14D70` (24 bytes); no parent chunk.
- Call sites: `82C0B5D0`, `82C0B610`, after callers validate their r3 pointers.
- Callee: tail branch to `82C1DBA8` at `82C14D68`.
- Inputs: r3 object pointer; return type UNKNOWN.
- Behavior: r11 = [r3+0x4C]; compare unsigned to zero in CR6. If zero,
  conditional LR return at `82C14D60`; otherwise r3 = [r11], tail branch.
- Side effects: no direct writes; callee effects UNKNOWN. No direct globals.
- Evidence: previous function ends `bctr` at `82C14D54`; trailing `blr`
  at `82C14D6C`; next prologue starts `82C14D70`.
- Classification: CONFIRMED code/behavior; STRONG INFERENCE boundary including
  trailing unreachable `blr`. High-level meaning UNKNOWN.

## 0x82E7E4F8 — sub_82E7E4F8

- Start/end: `82E7E4F8` / `82E7E508` (16 bytes); no parent chunk.
- Call site: `832B11A0`, with r3 = `833484C0` prepared at `832B1198`.
- Callee: tail branch `82E7E388` at `82E7E504`.
- Behavior: build `8203934C` in r11, store to [r3], tail branch with r3
  unchanged. LR unchanged; return behavior belongs to callee.
- Side effects: guest word [r3] overwritten; callee side effects UNKNOWN.
- Evidence: previous function ends in backward branch at `82E7E4F4`;
  next separate indirect forwarding leaf begins `82E7E508`.
- Classification: CONFIRMED code/behavior/boundaries. HYPOTHESIS: vtable
  assignment during object lifetime management; not enough evidence to name it.

## 0x82F279D8 — sub_82F279D8

- Start/end: `82F279D8` / `82F279E0` (8 bytes); no parent chunk.
- Call site: `82F048CC`, after r3 -= 0x80 at `82F048C8`.
- Callees: none. Return: r3 = 1. No memory writes or globals; LR unchanged.
- Evidence: `li r3,1; blr`; preceding function ends with backward branch
  at `82F279D4`; separate no-op `blr` at `82F279E0`.
- Classification: CONFIRMED constant-return leaf; semantic meaning UNKNOWN.

## 0x82C0B680, 0x82C0B688 — this-adjusting thunks

- Start/end: `82C0B680` / `82C0B688`, `82C0B688` / `82C0B690` (8 bytes each); no parent chunks.
- Call sites: `82C0011C` (inside `82C00118` leaf) branches to `82C0B680`; `82C00124` (inside `82C00120` leaf) branches to `82C0B688`.
- Callees: `82C0B680` tail branches to `82C49670` (`mr r3,r4; mr r4,r5; b 82BFFD30`); `82C0B688` tail branches to `82C4BDD8` (`mflr r12; bl 82CA2BEC; …`).
- Inputs: r3 is this-pointer; behavior is `r3 += 96` then tail call. LR unchanged.
- Side effects: no direct writes; callee effects are those of the target functions which are already discovered.
- Evidence: `tooling/ppc-disasm tooling/guest-image.bin 0x82000000 0x82C0B680 0x10` shows `addi r3,r3,96; b 0x82C49670` and `addi r3,r3,96; b 0x82C4BDD8`; previous function `82C0B678` (`addi r3,r3,96; b 82C49668`) and next function `82C0B690` (`mr r11,r3; …`) frame the thunks. Both thunks are 8-byte `addi`+`b` pairs identical to neighboring discovered thunks.
- Classification: CONFIRMED code/boundaries and this-adjustment forwarding.
- Open questions: high-level class hierarchy UNKNOWN.

## 0x82CD8650 — sub_82CD8650

- Start/end: `82CD8650` / `82CD8668` exclusive (24 bytes); no parent chunk.
- Caller: runtime indirect call through an object table during audio startup.
- Callees: none.
- Inputs: r4 points to a writable output structure; exact type UNKNOWN.
- Return: r3 = 0; LR unchanged; `blr` at `82CD8664`.
- Side effects: writes byte `6` to `[r4]` and halfword `0` to `[r4+2]`.
- Evidence: deterministic runtime fatal named unregistered target `82CD8650` in
  `build/native/logs/fable_ii_012.log`. The preceding function starts at
  discovered `82CD8648` and tail-branches at `82CD864C`; the next discovered
  function starts at `82CD8668`. The six instructions form a complete leaf.
- Classification: CONFIRMED code/behavior/boundaries. HYPOTHESIS: an audio
  format/default-configuration accessor based on the startup context.
- Open question: why static indirect-target discovery omitted this table entry.

## 0x82CDBAE0 — sub_82CDBAE0

- Start/end: `82CDBAE0` / `82CDBAF4` exclusive (20 bytes); no parent chunk.
- Caller: runtime indirect call during audio-object initialization.
- Callees: none.
- Inputs: r3 object pointer; exact type UNKNOWN.
- Return: r3 = incremented 32-bit value from `[r3+4]`; `blr` at `82CDBAF0`.
- Side effects: increments the guest word at object offset `+4`.
- Evidence: deterministic runtime fatal named unregistered target `82CDBAE0` in
  `build/native/logs/fable_ii_013.log`. The preceding discovered forwarding
  thunk ends in `bctr` at `82CDBADC`; padding follows this leaf at `82CDBAF4`,
  and the next discovered function starts at `82CDBAF8`.
- Classification: CONFIRMED code/behavior/boundaries. STRONG INFERENCE:
  reference-count increment reached through an interface vtable.

## 0x82CE5D50 — sub_82CE5D50

- Start/end: `82CE5D50` / `82CE5D74` exclusive (36 bytes); no parent chunk.
- Caller: runtime indirect call after the Xenos interrupt callback is installed.
- Callees: tail path enters discovered `sub_82CE5D74`; no linked calls.
- Inputs: r3 object pointer, low byte of r4 selects an entry; r5-r7 are
  forwarded after register shuffling. Exact types UNKNOWN.
- Return: HRESULT-like `0x80070057` when the index is out of range; otherwise
  tail-dispatch behavior belongs to `sub_82CE5D74`.
- Side effects: none before the tail dispatch.
- Evidence: deterministic runtime fatal named unregistered target `82CE5D50`
  in `build/native/logs/fable_ii_015.log`. The preceding discovered sibling
  `sub_82CE5D08` ends in `bctr` at `82CE5D4C`; the discovered shared tail begins
  at `82CE5D74`. The instruction layout mirrors `sub_82CE5D08`.
- Classification: CONFIRMED code/behavior/boundaries. STRONG INFERENCE: indexed
  forwarding method from a related interface vtable.

## 0x82C1DC80 — sub_82C1DC80

- Start/end: `82C1DC80` / `82C1DC98` exclusive (24 bytes); no parent chunk.
- Caller: runtime indirect call during Xenos startup after the interrupt
  callback and initial GPU state setup.
- Callee: tail-dispatches through vtable slot `+0xC` of the object loaded from
  `[r4+4]`, forwarding the word at `[r4]` as r4.
- Inputs: r4 points to a two-word call descriptor; exact types UNKNOWN.
- Return/side effects: those of the indirect callee; LR is forwarded unchanged.
- Evidence: deterministic runtime fatal named unregistered target `82C1DC80`
  in `build/native/logs/fable_ii_016.log`. The preceding discovered forwarding
  thunk ends at `82C1DC7C`; the next function begins at `82C1DC98`.
- Classification: CONFIRMED code/behavior/boundaries. HYPOTHESIS: deferred GPU
  command/callback adapter.

## 0x82C1DC50 — sub_82C1DC50

- Start/end: `82C1DC50` / `82C1DC60` exclusive (16 bytes); no parent chunk.
- Caller: runtime indirect call during Xenos startup after `sub_82C1DC80` was
  registered.
- Callee: tail-dispatches through vtable slot `+0x1C` of the r3 object.
- Inputs: r3 object pointer; remaining arguments are forwarded unchanged.
- Return/side effects: those of the indirect callee; LR is forwarded unchanged.
- Evidence: deterministic runtime fatal named unregistered target `82C1DC50`
  in `build/native/logs/fable_ii_017.log`. The function is a complete four-
  instruction vtable thunk, bounded by the preceding thunk's `bctr` at
  `82C1DC4C` and the next thunk at `82C1DC60`.
- Classification: CONFIRMED code/behavior/boundaries. HYPOTHESIS: GPU interface
  forwarding method.

## 0x82C1DC98 — sub_82C1DC98

- Start/end: `82C1DC98` / `82C1DCCC` exclusive (52 bytes); no parent chunk.
- Caller: runtime indirect call during Xenos startup after `sub_82C1DC50` was
  registered.
- Callee: tail-branches to discovered `sub_82CA6320`.
- Inputs: r4 and r5 each point to a word that may be null; when non-null the
  routine dereferences that word once and forwards the resulting pair in r3/r4.
- Return/side effects: those of `sub_82CA6320`; LR is forwarded unchanged.
- Evidence: deterministic runtime fatal named unregistered target `82C1DC98`
  in `build/native/logs/fable_ii_018.log`. The routine starts immediately after
  `sub_82C1DC80`, ends in an unconditional tail branch at `82C1DCC8`, and is
  followed by padding at `82C1DCCC` and discovered `sub_82C1DCD0`.
- Classification: CONFIRMED code/behavior/boundaries. HYPOTHESIS: nullable
  wrapper comparison adapter; the callee's semantics remain UNKNOWN.

## 0x82C1DC60 — sub_82C1DC60

- Start/end: `82C1DC60` / `82C1DC70` exclusive (16 bytes); no parent chunk.
- Caller: runtime indirect call during Xenos startup after `sub_82C1DC98` was
  registered.
- Callee: tail-dispatches through vtable slot `+0x18` of the r3 object.
- Inputs: r3 object pointer; remaining arguments are forwarded unchanged.
- Return/side effects: those of the indirect callee; LR is forwarded unchanged.
- Evidence: deterministic runtime fatal named unregistered target `82C1DC60`
  in `build/native/logs/fable_ii_019.log`. The function is a complete four-
  instruction vtable thunk, bounded by `sub_82C1DC50` and discovered
  `sub_82C1DC70`.
- Classification: CONFIRMED code/behavior/boundaries. HYPOTHESIS: GPU interface
  forwarding method.

## 0x82C8D820 — sub_82C8D820

- Start/end: `82C8D820` / `82C8D898` exclusive (120 bytes); no parent chunk.
- Caller: runtime indirect call shortly after the first graphics pipeline
  states are created and the missing French `lang.ini` lookup returns.
- Callees: none.
- Inputs: r4 and r6 hold mutable cursor pointers; r5 and r7 are corresponding
  bounds. Other argument semantics are UNKNOWN.
- Return: no explicit result; `blr` at `82C8D894`.
- Side effects: copies a bounded byte range from the r4 cursor to the r6 cursor,
  updating both cursor words. It may back the source end over bytes whose upper
  two bits are `10`, consistent with avoiding a split inside a UTF-8 continuation
  sequence.
- Evidence: deterministic runtime fatal named unregistered target `82C8D820`
  in `build/native/logs/fable_ii_020.log`. A preceding routine tail-branches at
  `82C8D81C`; this routine has a complete leaf return and the discovered next
  function starts at `82C8D898`.
- Classification: CONFIRMED code/boundaries/cursor side effects. STRONG
  INFERENCE: bounded text-byte transfer. Exact higher-level purpose UNKNOWN.

## 0x82C867E8 — sub_82C867E8

- Start/end: `82C867E8` / `82C867F4` exclusive (12 bytes); no parent chunk.
- Caller: runtime indirect call after the missing French `lang.ini` lookup.
- Callee: tail-branches to discovered `sub_82C863E8`.
- Inputs: r3 object pointer; the word at object offset `+0x10` is forwarded in
  r5, and r6 is set to zero. Other input semantics are UNKNOWN.
- Return/side effects: those of `sub_82C863E8`; LR is forwarded unchanged.
- Evidence: deterministic runtime fatal named unregistered target `82C867E8`
  in `build/native/logs/fable_ii_021.log`. The preceding function tail-branches
  at `82C867E4`; this routine ends in its own tail branch at `82C867F0` and is
  followed by padding at `82C867F4`.
- Classification: CONFIRMED code/behavior/boundaries. Higher-level purpose
  UNKNOWN.

## Validation

Baseline normal codegen: eight UnresolvedCall diagnostics, seven unique targets.
After defining `82CD7948`: 7 errors; after `82C000F8`: 6.
Defining `82C00108` removed that target but exposed adjacent `82C00110`, keeping
the count at 6. Defining `82C00110` alone left 5; defining `82C00118` exposed
`82C00120` and `82C0B680`. Incremental single-function validation continued:

- `82C14D58`: 6 → 4 errors (removed two duplicate callers)
- `82E7E4F8`: 4 → 3
- `82F279D8`: 3 → 2
- `82C00120`: 2 → 3 (exposed `82C0B688` and `82C00128` as next in the 8-byte runway)
- `82C0B680`: 3 → 2
- `82C00128`: 2 → 1
- `82C0B688`: 1 → 0

Normal codegen through iteration 12 was clean before runtime began exercising
indirect targets. Nine runtime-observed functions have since been added with
verified boundaries: `82CD8650`, `82CDBAE0`, `82CE5D50`, `82C1DC80`,
`82C1DC50`, `82C1DC98`, `82C1DC60`, `82C8D820`, and `82C867E8`. Normal
validation remains clean after the last entry. Only the informational
large-function file-splitting note for `0x82242ED0` remains unrelated to
function-graph correctness.
The original instruction bytes and execution behavior are preserved; changes are coverage only.

## 0x82B84350 — this-adjusting thunk (override-unblocked)

- Start/end: `82B84350` / `82B84358` exclusive (8 bytes); no parent chunk.
- Caller: Permanent Bank thread (`822F33B8` job) via indirect dispatch;
  exact call site UNKNOWN (caught as `FATAL ... 0x82B84350`, never reached
  without the `823FB0E8`/`82B6BEB0` no-op overrides).
- Callee: tail branch to `821FC1F0`.
- Inputs: r3 this-pointer; behavior is `r3 += 8` then tail call. LR unchanged.
- Side effects: those of the callee. No direct writes/globals.
- Evidence: `tooling/ppc-disasm tooling/guest-image.bin 0x82000000 0x82B84350
  0x80` shows `addi r3,r3,8; b 0x821fc1f0`; preceding function ends `blr` at
  `82B84328` with padding at `82B8432C`; next thunk starts `82B84358`.
  Identical shape to the registered `82C0B680`/`82C0B688` thunks.
- Classification: CONFIRMED code/boundaries and this-adjustment forwarding.
  Higher-level class UNKNOWN.

## Runtime-discovered call graph 2026-09-09 (behavioral, no boundary changes)

- GameThread sequencer `822EA928` -> drain verdict `822F27C0`
  (r3 low byte: 0 = empty/populate, 1 = drain) -> drain `822F47F8`
  (r3 = stack ctx) -> `823781A8` -> `82200688` (slot/lock protocol:
  `[slot]=lock`, `[slot+4]=held-flag`, r3=slot out) on shared lock
  `0x4C11E740` (heap object, stable across runs).
- Drain item path: `47F8:48CC`-virtual `[12]` = `823784A0`
  (r3=item `0x42205020`, r4=flag `0x42205148`, item vtable
  `0x820A7E20` = `[831FD318, 82C43198, 82378868, 829CE870, ...]`).
  Item `+0x88` and `+0x128` are pending bytes (stuck 01).
- `823784A0` internals: early virtuals (all return), `822F5540`,
  `822F3640` x2 (free stack lock), `82CBC6B0` sleep (null-object wait
  when r4=0), entry loop (`82C63098` returns, `8236CED8` returns with
  callees `8236CB48`/`82B68FC8`/`82A3B890`/`82A3BB78` returning),
  tail `82378420` -> `82356180` (item build, publishes ctx to global
  `lis-31927+26920`) + `829FF648`.
- Bank worker `822F33B8` chain (perf-proven): `82378FA0` -> `823FE988`
  -> `823FECA0` -> `822AC668` -> `821D2C70` -> `821E15A0` -> `822C05F8`
  -> `822C0568` -> `8219EE00` -> `822DF280` -> (`82BCC3A8`|`8229A518`)
  -> `82BC9640` -> `82BC9860` -> `82BC8490` -> `8227BA30` -> `82BCD7B0`
  (faults at `:21275` `stwx r31,r9,r11` when pool empty).
- Item pool `8240DAA8` (global `lis-31927+27088`, valid heap struct):
  size classes; grow path `83230568` works for many slots; bank's
  class slot has stale chunks + empty free list, never grows, returns
  NULL. Pool init `823052C0` runs.
- Lock holder IDs (THREADMAP, stable): `0x30097018`=3D Engine (holds
  `0x4C11E740`), `0x30030018`=GameThread, `0x30092018`=Cloth,
  `0x3009C018`=Bank.
- Fill workers cycle on shared table object `4F640220` via `8227BB58`
  (tgt `8227CB70` -> `8240DAA8`): call sites `82BCA368`/`82BC9E50`/
  `82BC9F60`/`82BCD7A0`/`82BCCD9C`/`82BCD7FC`.
- GPU interrupt callback `82B9B8D8` fires normally (14k+, healthy).
- Populate `822F2608` <- `8236C940` <- `82A47D48` (G4 producer) never
  runs; `822F4690` quit-setter never runs; `823052C0` pool init runs.

## Populate tail chain 2026-09-09 (subagent disasm, CONFIRMED shape)

- `822F2608` tail from `822F2668`: `822F5718` (r3=r31+108) ->
  `829FF648` x3 (r3=r31+100/92/84) -> null-guarded vtable+0 bctrl
  (r3=[r31+80], r4=1) -> `829FF648` (r3=r29) -> `82356698`
  (r3=r31+56) -> null-guarded vtable+8 bctrls ([r31+48], [r31+44])
  -> null-guarded vtable+0 bctrls ([r31+40], [r31+36], each cleared
  after). No `blr`: `822F2744 addi r1,r1,112; 822F2748 b 0x82CA2C3C`
  (tail-call out; probes must sit before it).
- `822F5718` (head): prologue, `bl 822F71A8`, `bl 8221BE68`, zeroes
  `[r31+4,8]`, `blr` at `822F577C`. Fast.
- `829FF648` = intrusive-refcount release: atomic dec (lwarx/stwcx.),
  zero-check, virtual release bctrl, field null-out, `bl 8221BE68`.
  No file/asset path, no sleep/lock, no strings. Fast.
- `8221BE68` = bare thunk `b 83231BE8`; that fn reads byte-global
  `0x83496BCB` and branches.
- Neighbor wrapper `829FF6D0`: acquire-side (atomic inc, vtable+60
  gating on return==3), re-calls `829FF648` at `+0xA8`, `bl 83229940`.
