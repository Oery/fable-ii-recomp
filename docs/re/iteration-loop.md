# Recompilation iteration runbook

This is the mechanical loop for the current Fable II bring-up. Read
`docs/re/status.md` first: it identifies the last verified milestone and the one
current blocker. Preserve the supplied Xbox files and never edit generated C++.

Run repository commands through the pinned Nix shell from the repository root:

```sh
nix develop path:. -c scripts/codegen
nix develop path:. -c scripts/build
nix develop path:. -c scripts/run
```

`scripts/run` defaults to a bounded 10-second run, the SDL offscreen video
driver, the extracted game root, and the empty update root. It retains real
audio because this SDL build has no dummy-audio backend. Set `RUN_SECONDS` for a
different limit. The Nix shell selects Mesa Lavapipe so Xenos can run without a
physical GPU. The script prints the newest runtime log and its high-signal
lines, while complete stdout/stderr is saved to `logs/runtime-console.log` to
prevent repeated faults from flooding an agent transcript. It returns the
program status: a fatal abort is normally 134; the default hard timeout is
normally 137. The diagnostic runner uses `RUN_STOP_SIGNAL=KILL` so broken guest
thread teardown cannot rotate away the active-execution log. Use
`RUN_STOP_SIGNAL=INT` explicitly when investigating clean shutdown.
Inspect an existing result without rerunning it with:

```sh
scripts/inspect-last-run
```

A timeout result means the active path survived for the requested duration.
ReXGlue currently emits a rapid guest-access-violation storm while threads
unwind after SIGINT. Check timestamps and reproduce before calling any fault
that begins at shutdown a startup blocker. Under GDB, pass expected physical
memory SIGSEGVs to ReXGlue's handler; stopping at the first raw SIGSEGV may only
catch normal MMIO/page-protection handling.

## Unregistered guest target loop

Use this procedure only when the newest log ends with:

```text
[FATAL] Call to invalid or unregistered function at guest address 0xADDRESS
```

1. Record the exact log and target. Search the manifest, notes, generated
   registration tables, and codegen diagnostics:

   ```sh
   rg -n -i 'ADDRESS' fable_ii_manifest.toml docs generated logs
   ```

2. Disassemble enough bytes before and after the target. The tool arguments are
   guest image, image base, start address, and byte count:

   ```sh
   tooling/ppc-disasm tooling/guest-image.bin 0x82000000 0xADDRESS 0x100
   ```

   If the tool or image is absent, recreate them with:

   ```sh
   nix develop path:. -c scripts/build-analysis-tool
   nix develop path:. -c scripts/dump-image
   ```

3. Prove the start and exclusive end. Useful evidence includes a preceding
   return or tail branch, padding, the next known function, a complete prologue
   and epilogue, and direct branch targets. A four-instruction `lwz/lwz/mtctr/bctr`
   vtable thunk is complete at its `bctr`. Do not guess a size from proximity.

4. Add only the runtime-observed function to `fable_ii_manifest.toml`:

   ```toml
   [entrypoint.functions.ADDRESS]
   end = 0xEXCLUSIVE_END
   ```

   Do not pre-register adjacent routines just because they look similar. This
   keeps every runtime advance attributable to one graph correction.

5. Add a function entry to `docs/re/functions.md` with address, exclusive end,
   callers, callees, inputs, return, side effects, evidence, and an explicit
   `CONFIRMED`, `STRONG INFERENCE`, `HYPOTHESIS`, or `UNKNOWN` classification.
   Keep semantic names address-based until evidence supports a real name.

6. Run normal codegen. Success means validation reports no unresolved-call or
   boundary errors. The existing message that function `0x82242ED0` exceeds the
   generated file-size threshold is informational. Never use force codegen as
   the fix.

7. Build, then run. If the failure advances to a new guest target, update
   `docs/re/status.md` and repeat. If it does not advance, reassess the boundary
   or switch to the relevant diagnostic path below.

## Other failure classes

- For a native crash or access violation, capture a debugger backtrace and map
  the generated native function to its guest address. Inspect the guest caller
  and state before changing code.
- For a missing import, search the pinned ReXGlue SDK first and Xenia second.
  Match observable behavior and error codes; do not return success by default.
- For a GPU error, keep Xenos enabled and inspect the first semantic failure.
  A GPU-disabled run is only a regression comparison.
- For an audio initialization failure, confirm SDL loaded its dynamic ALSA,
  PulseAudio, or PipeWire library from the Nix environment. Initialization may
  unwind and cause a later secondary null dereference.
- Repeated `BaseHeap::AllocFixed attempting to reserve an already reserved
  range` lines currently precede later deterministic blockers. Record them, but
  do not assume they are the cause without a failing address or state link.

## When to ask a more experienced agent for help

Stop the mechanical loop and ask for review before making a change when any of
these is true:

- the function start or exclusive end is ambiguous;
- the target could be inline data, a jump table, a disconnected chunk, or the
  middle of an already discovered function;
- normal codegen introduces more errors, moves an error unexpectedly, or only
  succeeds with a force/ignore option;
- the same failure remains after two evidence-backed attempts;
- the crash has no guest target and the native backtrace does not identify the
  responsible generated function;
- the proposed fix is a hook, stub, forced return value, guest binary patch,
  broad manual function range, or ReXGlue SDK modification;
- an indirect target looks corrupted or does not point to plausible guest code;
- ABI, endianness, synchronization, callback, or guest-memory semantics are
  uncertain;
- progress appears to require copyrighted input that is not already supplied.

When asking, provide the newest log path, guest address, disassembly around it,
generated caller if known, manifest entries tried, and the exact result of
codegen/build/run. State what is confirmed separately from hypotheses. This
lets the reviewer test the uncertain step instead of repeating inventory.

## Failure patterns to avoid

Never edit `generated/*.cpp` or `generated/*.h`, register a guessed range, add a
chain of success stubs, disable graphics and call that progress, or change
several causal variables in one iteration.

Guest-memory address arithmetic needs special care. ReXGlue accesses this image
as `base + full_guest_address`. Do **not** subtract the XEX image base first.
The earlier diagnostic for global `0x833370E4` incorrectly read
`base + (0x833370E4 - 0x82000000)`, reported a false null, and added a hook that
caused initialization to unwind. The correct read, `base + 0x833370E4`, showed
the singleton was valid. If an observation would justify a hook or override,
have another agent check both the address calculation and the caller first.

After each meaningful iteration, keep `docs/re/status.md` current with the
blocker, guest address, evidence, classification, root cause or hypothesis,
change, codegen result, build result, runtime result, regressions, and next
action. A later agent should be able to start from that file without reading a
terminal transcript.
