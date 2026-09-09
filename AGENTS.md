# ReXGlue Xbox 360 Recompilation Agent Instructions

## Mission

Autonomously produce a working native recompilation of the supplied Xbox 360 game using `rexglue-sdk`.

This is a static recompilation and reverse-engineering task.

The objective is not to reconstruct the game's original source code. The objective is to preserve the observable behavior of the original Xbox 360 PowerPC program while replacing execution of its PPC machine code with ReXGlue-generated native code and runtime implementations.

Work continuously through reproducible blockers. Do not stop merely because codegen, compilation, or runtime execution fails.

When a failure is actionable from the repository, binaries, generated code, SDK source, debugger, or available external tooling, investigate it and continue.

Only ask the user when progress genuinely requires information or material that cannot be derived locally.

# Legal / Data Boundary

Operate only on the Xbox files supplied by the user.

Do not download game binaries, title updates, DLC, keys, copyrighted assets, or replacement executables from the Internet.

Downloading open-source reverse-engineering tools, SDKs, documentation, emulator source code, compilers, and development dependencies is allowed.

Treat original game files as immutable input.

# Source of Truth

Use sources in this priority order:

1. Files in this repository.
2. The exact `rexglue-sdk` revision used by this project.
3. ReXGlue source code and headers.
4. ReXGlue-generated code and diagnostics.
5. Original Xbox 360 machine code.
6. Execution traces and debugger state.
7. Xenia source code.
8. Official/current ReXGlue documentation.
9. Other reverse-engineering documentation.

Never assume a ReXGlue API from memory.

ReXGlue is evolving quickly.

Before using a configuration field, CLI option, hook macro, runtime API, or generated-code convention, inspect the checked-out SDK or run the relevant command with `--help`.

# Environment: Nix

The host uses Nix.

A command not existing in `$PATH` does NOT mean that the tool is unavailable.

Do not ask the user to globally install ordinary development tools.

Do not use `sudo` to install packages.

## Temporary CLI tools

If a required external CLI exists in nixpkgs, it may be invoked directly with:

```
nix run nixpkgs#<package> -- <arguments>
```

Examples of potentially useful tool categories include:

```
git
cmake
ninja
clang
llvm
gdb
lldb
python3
ripgrep
file
binutils
jq
radare2
rizin
```

Determine actual nixpkgs attribute names instead of guessing them.

Use:

```
nix search nixpkgs <name>
```

when necessary.

If an upstream project provides a flake, it may also be run directly with:

```
nix run github:<owner>/<repo> -- <arguments>
```

Check the upstream project first. Do not assume that every GitHub repository exposes a runnable flake.

# Missing Libraries and Build Dependencies

If software requires libraries that are not available in the current environment, create or extend a Nix development environment.

Prefer a repository `flake.nix` when the dependency is relevant to continued project development.

For one-off experimentation, a temporary flake under a tooling/build directory is acceptable.

Use:

```
nix develop
```

or:

```
nix develop -c <command>
```

to supply compilers, headers, pkg-config dependencies, development libraries, Python environments, etc.

Do not modify the host OS merely to satisfy a build dependency.

If an external tool is only distributed as source:

1. clone it under a tooling directory outside tracked game assets;
2. create an appropriate Nix build/dev environment;
3. build it there;
4. record the revision used;
5. avoid adding its build products to version control unless deliberately required.

A missing system library is normally an environment problem to solve with Nix, not a reason to abandon a tool.

# Reproducible Development Environment

Prefer eventually expressing persistent project dependencies in the repository's flake.

The long-term development environment should provide at least the compiler and build tools required by the pinned ReXGlue version plus any libraries required by the project.

Do not arbitrarily change compiler versions when ReXGlue specifies one.

Inspect:

```
rexglue-sdk/CMakeLists.txt
rexglue-sdk/CMakePresets.json
project CMakePresets.json
```

before selecting compiler versions.

# Input Format

Do NOT initially expect `default.xex`.

The supplied Xbox files may be in installed Games-on-Demand form.

A typical input currently looks like:

```
4D5307F1/
└── 00007000/
    ├── 305B6EA890970E515722
    └── 305B6EA890970E515722.data/
        ├── Data0000
        ├── Data0001
        ├── Data0002
        └── ...
```

Treat:

```
4D5307F1
```

as the Xbox title/content hierarchy,

```
00007000
```

as the Games-on-Demand content directory,

the extensionless file as the GoD package/header file,

and the matching `.data/DataXXXX` files as its external data segments.

Do NOT feed `Data0000` directly into ReXGlue.

Do NOT concatenate, alter, rename, or overwrite the original package files merely to make a tool accept them.

Preserve the original directory structure.

# Phase 0 — Inventory

Before modifying the project, inspect it.

At minimum run equivalents of:

```
git status
find . -maxdepth <reasonable-depth> -type f
file <candidate files>
du -sh <game-data-root>
```

Identify:

* repository root;
* existing flake;
* existing ReXGlue project;
* ReXGlue SDK location;
* ReXGlue revision;
* GoD package header;
* corresponding `.data` directory;
* existing extracted game root, if any;
* existing `default.xex`;
* title updates, if any;
* existing reverse-engineering notes;
* existing generated code;
* current build state.

Do not duplicate extraction if a verified extracted game already exists.

# Phase 1 — GoD Extraction

The first required artifact is an extracted game filesystem containing the title's executable and assets.

Expected result resembles:

```
game/
├── default.xex
├── ...
└── <game assets>
```

## Preferred approach: direct GoD extraction

Prefer a maintained Linux-capable tool that understands GoD input directly.

A suitable current candidate is XGDTool.

Before invoking it:

1. obtain/build it if necessary;
2. run its help output;
3. confirm the exact CLI accepted by the checked-out revision;
4. point it at the GoD package/header or package path as required;
5. request extracted-files output.

The expected conceptual operation is:

```
GoD package -> extracted Xbox game filesystem
```

Do not blindly copy an example command from these instructions if the checked-out tool's CLI differs.

Verify the resulting filesystem.

The extraction is successful only when a plausible entry executable such as:

```
default.xex
Default.xex
```

exists.

# GoD Extraction Fallback

If direct extraction fails for a tooling reason, use a two-stage conversion:

```
GoD
  -> Xbox/XDVDFS ISO
  -> extracted game directory
```

Suitable tool categories are:

```
god2iso
extract-xiso
```

Check each tool's current `--help` output before invocation.

Do not guess argument order.

After generating an ISO, first list its contents without modifying it when supported.

Verify that a `default.xex`-like executable exists.

Then extract the filesystem.

The intermediate ISO is a generated artifact and may be deleted after successful verified extraction if disk space matters.

Never delete the original GoD package.

# Extraction Verification

After extraction:

1. locate all `.xex` files;
2. identify the likely game entrypoint;
3. inspect sizes and file types;
4. record a checksum of the extracted entry XEX;
5. preserve the relative asset hierarchy.

Useful commands include:

```
find <game-root> -iname '*.xex' -o -iname '*.xexp'
file <game-root>/default.xex
sha256sum <game-root>/default.xex
```

Do not assume the largest XEX is the entrypoint.

Do not flatten the extracted game directory.

# Title Updates

Search the supplied Xbox content tree for title-update content as well.

Do not download missing updates.

If a supplied title update extracts to an XEX patch such as `default.xexp`, inspect the version of ReXGlue being used and determine whether its patch-file support is appropriate.

Keep:

* base executable;
* update patch;
* patched/generated executable, if one is produced;

as distinct artifacts.

Record exactly which executable/update combination is being recompiled.

# Phase 2 — Establish the ReXGlue Project

Once a verified entry XEX exists, inspect the installed ReXGlue CLI:

```
rexglue --help
rexglue init --help
rexglue codegen --help
```

Do this before creating configuration.

If no project exists, initialize one using the syntax supported by the installed SDK.

Use the extracted XEX as the entry executable.

Do not point ReXGlue at:

* the GoD header;
* `Data0000`;
* the reconstructed ISO.

The ReXGlue input is the extracted Xbox executable.

# Game Assets

Keep executable code and game assets logically separate.

A useful structure is:

```
assets-original/
    <original GoD package>

assets-extracted/
    default.xex
    ...

generated/
    <ReXGlue output>

src/
    <native overrides and runtime integration>

docs/re/
    <reverse-engineering notes>
```

Actual repository conventions take priority.

Do not rename asset files unless required.

Xbox titles can rely on exact paths and case conventions.

# Phase 3 — Baseline Codegen

Run ReXGlue codegen normally first.

Do not immediately use force/ignore options.

Capture complete diagnostics to a file where practical.

Example conceptual workflow:

```
rexglue codegen <config> 2>&1 | tee logs/codegen.log
```

Use the actual CLI for the installed version.

Classify every error.

Typical categories:

* undiscovered function;
* incorrect function boundary;
* disconnected function chunk;
* unresolved direct branch/call;
* indirect-call target missing;
* jump/switch table analysis failure;
* invalid instruction/data interpreted as code;
* missing import;
* CRT routine not mapped;
* unsupported PPC behavior;
* ReXGlue codegen bug.

Create a work queue from concrete diagnostics.

# Force Codegen

A force option, if supported, may be used diagnostically to generate enough C++ to inspect later code.

It is not a fix.

Any generated runtime fatal corresponding to an unresolved call remains an unresolved bug.

Do not declare codegen successful while reachable unresolved calls remain.

# Phase 4 — Build Baseline

Establish separate results for:

1. code generation;
2. CMake configuration;
3. compilation;
4. linking;
5. runtime initialization.

Do not conflate them.

Use the ReXGlue-compatible Clang toolchain.

Prefer Debug or RelWithDebInfo while bringing up the title.

Capture build output when failures are non-trivial.

# External Reverse-Engineering Tools

Codex is expected to obtain and run additional open-source tooling when that materially reduces uncertainty.

A tool not being preinstalled is not a blocker.

Use Nix as described above.

# Required Capability: Scriptable PPC Analysis

For autonomous work, maintain at least one noninteractive way to inspect PowerPC code.

Possible implementations include:

* ReXGlue's own decoder/analysis output;
* generated ReXGlue C++;
* a small analysis utility built against ReXGlue internals;
* LLVM PowerPC disassembly when supplied a usable code image;
* Capstone;
* Rizin;
* radare2;
* Ghidra headless with an appropriate loader/import path;
* another scriptable PPC disassembler.

Do NOT hard-depend on an external tool being able to load Xbox XEX directly.

If an external disassembler cannot parse XEX:

1. do not spend excessive time forcing it;
2. use ReXGlue's XEX loader/decoder where possible;
3. inspect generated code;
4. create a small purpose-built analysis utility if necessary.

The objective is PPC visibility, not allegiance to a particular decompiler.

# GUI Reverse-Engineering Tools

IDA Pro may be useful when already available to the user, particularly because ReXGlue may ship helper scripts for it.

However, autonomous progress must not depend on a GUI-only commercial tool.

Do not stop because IDA is unavailable.

Prefer scriptable tooling for the autonomous loop.

# Utility Tooling

Codex may freely obtain ordinary open-source analysis utilities.

Useful categories include:

* `ripgrep` for source/generated-code search;
* `file` for format identification;
* `xxd` or equivalent for byte inspection;
* `strings`;
* `objdump` / LLVM tools;
* Python for ad-hoc parsers and trace comparison;
* `jq` for structured diagnostic output;
* `gdb` or `lldb`;
* CMake/Ninja tooling.

Small one-off analysis scripts are encouraged when they make a hypothesis testable.

Place useful persistent scripts under something such as:

```
tools/
scripts/
```

instead of repeatedly issuing fragile shell pipelines.

# Xenia

A local Xenia source checkout is an important reference.

If not already available, Codex may clone the upstream Xenia repository.

Use Xenia source for:

* Xbox kernel API semantics;
* XAM behavior;
* object/handle behavior;
* filesystem behavior;
* threading and synchronization semantics;
* memory APIs;
* graphics API behavior;
* audio behavior;
* implementation details inherited by ReXGlue.

Do not copy substantial implementation blindly.

Understand the externally visible behavior required by the game and implement it at the correct ReXGlue/project layer.

Keep the Xenia revision recorded in notes when specific behavior was derived from it.

# Xenia Runtime Comparison

If a runnable Xenia build and suitable environment are available, it may be used as an oracle for behavior of the user's supplied game.

Useful comparisons include:

* boot logs;
* import calls;
* return codes;
* initialization order;
* filesystem accesses;
* game state transitions;
* memory values;
* rendering initialization.

Do not make autonomous progress depend on interactive Xenia operation.

Source-level comparison remains useful even when running Xenia is impractical.

# Phase 5 — Function Graph Repair

When ReXGlue reports an unresolved target, investigate it systematically.

For an address `TARGET`:

1. locate every diagnostic mentioning `TARGET`;
2. search generated output for `TARGET`;
3. inspect the caller;
4. inspect original PPC around the target;
5. determine whether target bytes are code or data;
6. determine likely function boundaries;
7. inspect neighboring functions;
8. inspect other callers;
9. add the smallest justified configuration correction;
10. regenerate;
11. verify that diagnostics improve rather than merely move.

Do not invent arbitrary sizes.

Prefer evidence such as:

* known next function start;
* PPC return pattern;
* unwind/prologue patterns;
* branch-target graph;
* caller behavior;
* symbol information;
* repeated ABI patterns.

# Function Naming

Addresses are canonical.

Unknown functions should remain address-based:

```
sub_82XXXXXX
```

Semantic names may be introduced when supported by evidence.

Every semantic name in reverse-engineering notes should retain its address.

Bad:

```
InitializeRenderer
```

when this is merely a guess.

Better:

```
sub_82123400
hypothesis: renderer initialization
confidence: low
```

Rename only when confidence becomes sufficient.

# Function Notes

For meaningful discoveries, maintain:

```
docs/re/functions.md
```

or per-subsystem files.

Record:

```
Address:
Name:
Start:
End/size:
Parent chunk:
Callers:
Callees:
Inputs:
Return:
Important registers:
Globals:
Side effects:
Hypothesis:
Evidence:
Confidence:
Open questions:
```

Do not keep important reverse-engineering knowledge only in terminal output or agent context.

# PowerPC Reasoning

Xbox 360 code is PowerPC.

Always reason in terms of guest ABI before host C++ types.

Inspect:

* GPR argument registers;
* floating/vector registers;
* return registers;
* LR;
* CTR;
* CR;
* stack frame;
* TOC/global addressing;
* indirect calls;
* branch linkage;
* sign extension;
* integer width;
* endianness;
* VMX/vector instructions;
* alignment;
* paired address-construction instructions;
* jump tables;
* vtables and function pointers.

Do not infer a high-level C++ signature from one use.

Inspect several callers when possible.

# Data Structure Recovery

Recover structures incrementally.

Prefer:

```
object + 0x30 = pointer-like field
object + 0x44 = 32-bit flag field
```

over prematurely declaring a complete class.

Validate fields from:

* constructors;
* destructors;
* repeated access patterns;
* serialization;
* setters/getters;
* call sites.

Keep unknown regions unknown.

# ReXCRT

When a PPC function is strongly identified as a supported runtime/CRT operation, prefer the ReXGlue native CRT mapping mechanism instead of recompiling redundant code.

Before mapping:

1. inspect the ReXGlue revision's supported mappings;
2. validate the function identity from PPC behavior and callers;
3. verify required function groups are complete;
4. regenerate and test.

Never label a function `memcpy`, allocator, string routine, etc. because it merely looks similar.

# Imports

For every missing Xbox import, determine whether it is:

* already implemented by ReXGlue;
* missing from the export table;
* implemented but unresolved incorrectly;
* genuinely unimplemented;
* a title-local wrapper rather than an import.

Search the pinned ReXGlue SDK first.

Then search Xenia.

Do not immediately return success from unknown imports.

# Implementing Missing Xbox APIs

If a missing kernel/XAM API must be implemented:

1. inspect Xenia's equivalent;
2. determine observable semantics;
3. identify what this game uses;
4. implement the smallest semantically correct subset;
5. preserve errors and side effects that callers rely upon;
6. document intentionally unsupported behavior;
7. test the call path.

If the implementation is generally correct for Xbox software, prefer the ReXGlue runtime layer.

If it is intentionally game-specific, keep it in the title project.

# Overrides

Use whole-function native overrides only when there is a reason to replace the entire original function.

Examples:

* recognized runtime library routine;
* platform abstraction;
* unsupported guest behavior with well-understood semantics;
* subsystem boundary;
* controlled diagnostic experiment.

Before overriding, determine:

* argument ABI;
* return ABI;
* guest-memory side effects;
* globals touched;
* callbacks;
* synchronization behavior.

Do not replace complex gameplay code merely because it is inconvenient to debug.

# Mid-Function Instrumentation

Use ReXGlue's supported mid-ASM hook mechanism when information is needed at a specific guest instruction.

Prefer observation first.

Good uses:

* log selected guest registers;
* inspect a pointer;
* capture a state transition;
* confirm branch conditions;
* trace indirect call targets;
* compare before/after values.

Do not modify registers or redirect flow until the diagnostic data justifies it.

# Logging Policy

Instrumentation should be targeted.

Every trace should answer a specific question.

Avoid logging every instruction for long periods unless implementing a bounded trace mechanism.

When logging guest state, include:

* guest PC/address;
* function address;
* relevant registers;
* relevant memory addresses;
* thread when applicable.

Remove or gate noisy traces after the issue is understood.

# Stubbing Policy

Stubbing is permitted only with evidence that omitted behavior is irrelevant or intentionally unsupported.

Potential examples:

* telemetry;
* obsolete online-service integration;
* optional dashboard UI;
* debug-only logging.

Before creating a stub, inspect:

* all known callers;
* expected return codes;
* output parameters;
* guest-memory writes;
* globals;
* synchronization;
* callbacks.

Document every non-trivial stub.

Do not build a chain of "return success" stubs merely to reach the title screen.

# Native Debugging

Use `gdb` or `lldb` for crashes in the generated/native program.

If unavailable:

```
nix run nixpkgs#gdb -- ...
```

or obtain the appropriate debugger through the project flake.

When a runtime crash occurs:

1. capture the native backtrace;
2. identify the generated ReXGlue function;
3. recover the corresponding guest address;
4. inspect relevant PPC state;
5. inspect guest memory;
6. inspect the PPC caller;
7. determine whether state was corrupted earlier.

The instruction that crashes is not necessarily the instruction that introduced the bug.

# Sanitizers

When compatible with the ReXGlue build, consider native diagnostics such as:

* AddressSanitizer;
* UndefinedBehaviorSanitizer.

Use them primarily to distinguish host/runtime bugs from guest-semantic bugs.

Do not leave diagnostic build flags permanently enabled without reason.

# Indirect Calls

Treat unresolved indirect calls as high priority.

For a bad guest target:

1. capture target address;
2. find where target value came from;
3. determine whether it is:

   * vtable;
   * callback;
   * import thunk;
   * jump table;
   * function-pointer table;
   * corrupted data;
4. verify that target corresponds to real guest code;
5. verify function graph coverage;
6. verify mapping table coverage.

Never redirect an unknown target to a nearby function simply because addresses are close.

# Switch Tables

If control-flow analysis fails around `bctr`/computed branches:

1. identify index register;
2. identify table base;
3. recover possible targets;
4. validate each target as reachable code;
5. use the ReXGlue revision's supported manual switch-table configuration if necessary.

Do not treat every indirect branch as a switch.

# Generated C++

Generated C++ is diagnostic output and build input.

Do not manually maintain it.

Never solve a permanent problem by editing:

```
generated/*.cpp
generated/*.h
```

when regeneration will overwrite the change.

Permanent changes belong in:

* codegen configuration;
* project overrides;
* hooks;
* runtime integration;
* ReXGlue SDK;
* explicit documented patching.

It is acceptable to temporarily annotate/copy generated fragments into notes for analysis.

# ReXGlue SDK Bugs

When behavior appears to be a ReXGlue defect:

1. isolate the guest address/instruction;
2. inspect generated C++;
3. inspect ReXGlue's decoder/code generator;
4. verify PowerPC semantics;
5. search upstream history/issues if Internet access is available;
6. construct the smallest reproduction possible;
7. fix the SDK only when evidence supports it.

Do not work around a generic compiler bug with a game-specific guest patch when a proper SDK fix is feasible.

Keep SDK fixes separable from title-specific changes.

# Do Not Modify the Guest Blindly

Binary patching is a last resort.

Do not:

* NOP arbitrary instructions;
* invert branches until execution continues;
* replace unknown calls with returns;
* patch constants merely because they prevent a crash.

If a binary patch is required:

* document original bytes;
* document replacement bytes;
* document address;
* document exact semantic reason;
* verify it against the original behavior.

# Autonomous Blocker Queue

Maintain:

```
docs/re/status.md
```

with at least:

```
Current milestone
Current blocker
Last successful guest address/state
Codegen errors
Missing imports
Runtime crashes
Intentional stubs
Active hypotheses
Fixed issues
Regressions
Next actions
```

When one blocker is solved, immediately select the next concrete blocker.

# Choosing the Next Task

Prioritize work in this order:

1. deterministic codegen failures;
2. compile/link failures;
3. deterministic startup crashes;
4. missing imports encountered during startup;
5. invalid indirect-call targets;
6. initialization deadlocks;
7. graphics/audio/input blockers preventing progress;
8. gameplay correctness;
9. non-critical missing behavior;
10. optimization.

Do not spend hours semantically naming functions while a deterministic startup failure remains unresolved.

# Hypothesis Discipline

Explicitly classify findings as:

```
CONFIRMED
STRONG INFERENCE
HYPOTHESIS
UNKNOWN
```

Do not silently promote guesses into facts.

For each meaningful hypothesis, try to devise an observation that could falsify it.

# Iteration Loop

Use this loop repeatedly:

```
reproduce blocker
    ↓
record exact failure
    ↓
map native failure to guest address
    ↓
inspect generated code
    ↓
inspect PPC / imports / data
    ↓
inspect callers and state
    ↓
form testable hypothesis
    ↓
add minimal instrumentation
    ↓
rerun
    ↓
confirm/refute
    ↓
implement smallest permanent fix
    ↓
regenerate if necessary
    ↓
rebuild
    ↓
rerun
    ↓
regression-check earlier milestone
    ↓
update docs/re/status.md
    ↓
select next blocker
```

# One Variable at a Time

Prefer one causal modification per diagnostic iteration.

Do not combine:

* a function-boundary change;
* two API stubs;
* a guest patch;
* and a runtime change

into one experiment.

If behavior improves, the cause must remain attributable.

# Preserve Useful Command Output

For difficult failures, store output under something such as:

```
logs/
    codegen.log
    build.log
    runtime.log
```

Do not commit enormous transient traces by default.

Persistent small diagnostic traces that explain a bug may be retained when useful.

# Project Tool Scripts

Automate repeated operations.

Good candidates:

```
scripts/extract-game
scripts/codegen
scripts/build
scripts/run
scripts/debug
scripts/check
scripts/update-status
```

A single reproducible command is preferable to remembering a sequence of shell commands.

Scripts should fail on errors.

# Extraction Script

Once the GoD extraction process has been proven, create a reproducible extraction helper if appropriate.

It should:

1. locate/accept the GoD package;
2. refuse to overwrite original input;
3. create a deterministic extracted directory;
4. invoke the selected tool;
5. verify `default.xex`;
6. print/check its hash.

Do not commit extracted copyrighted assets merely because the script exists.

# Hashing

Record hashes for important immutable inputs:

* GoD header;
* relevant Data chunks if practical;
* extracted `default.xex`;
* title-update patches.

This prevents debugging against accidentally changed input.

# Milestones

## M0 — Input recovered

* GoD package identified.
* Game filesystem extracted reproducibly.
* Entry XEX identified.
* Entry XEX hash recorded.

## M1 — ReXGlue analysis

* ReXGlue reads the XEX.
* Codegen begins.
* Deterministic analysis blockers are understood.

## M2 — Code generation

* Reachable function graph is sufficiently valid.
* Generated source is reproducible.
* Remaining forced/unresolved entries are explicitly documented.

## M3 — Compilation

* Native project configures.
* Generated code compiles.
* Executable links.

## M4 — Runtime startup

* ReXGlue initializes.
* Guest image loads.
* Guest entrypoint executes.

## M5 — Early title boot

* Threads initialize.
* Required imports resolve.
* Filesystem initialization proceeds.

## M6 — Visible output

* Presentation initializes.
* Stable visible frame/loading screen appears.

## M7 — Title screen

* Title screen operates.
* Input works sufficiently to navigate.
* No deterministic boot crash.

## M8 — Gameplay

* A game/session can be started.
* Core simulation executes.
* Representative scene remains stable.

## M9 — Playable

* representative gameplay works;
* graphics are functionally correct;
* audio is sufficiently correct;
* input works;
* save/load works if applicable;
* serious deterministic crashes are resolved;
* significant divergences are documented.

# Regression Testing

After a significant runtime/runtime-API fix, repeat the earliest affected milestone.

A fix that advances one path but breaks an earlier working path is a regression.

Track regressions explicitly.

# Optimization

Correctness first.

Do not optimize generated code or enable aggressive transformations while fundamental semantic bugs remain.

Only optimize once representative gameplay exists, unless performance prevents debugging.

# Git Discipline

Before editing:

```
git status
```

Never erase unrelated user changes.

Do not reset, clean, checkout-over, or otherwise discard unknown changes.

If commits are requested or already part of the workflow, keep them narrow.

Examples:

```
tooling: add reproducible GoD extraction

re: define function at 0x82123400

rexcrt: map verified memcpy implementation

runtime: implement required XamFoo behavior

hooks: trace scheduler state at 0x8220AB10

sdk: fix <PPC instruction> codegen semantics
```

Avoid:

```
fix game
```

Do not commit original proprietary game assets unless the repository explicitly already does so and the user expects it.

# Internet Research

Internet access may be used for:

* ReXGlue documentation;
* ReXGlue source/issues/commits;
* Xenia source/issues;
* PowerPC documentation;
* Xbox 360 technical documentation;
* open-source analysis tools;
* Nix packaging information.

Do not search for or obtain pirated game files.

When technical information conflicts with the checked-out SDK behavior, trust the checked-out source.

# Definition of a Useful Autonomous Session

A session does not need to reach gameplay to be productive.

Good outcomes include:

* converting the supplied GoD package reproducibly;
* fixing codegen;
* identifying several missing function boundaries;
* implementing a missing import;
* isolating an SDK bug;
* advancing boot to a new deterministic blocker.

Do not hide partial progress.

At the end of work, leave the repository in a state where another agent can immediately understand what was learned and what fails next.

# Progress Reports

For meaningful iterations, summarize:

```
Blocker:
Guest address:
Native location:
Evidence:
Classification:
Root cause:
Change:
Codegen:
Build:
Runtime:
Next blocker:
```

Do not produce verbose narration for trivial commands.

# Definition of Done

The recompilation is complete only when:

* GoD extraction is reproducible;
* original source package remains unmodified;
* executable/update identity is documented;
* ReXGlue codegen is reproducible;
* generated files require no manual fixes;
* project builds reproducibly;
* reachable function calls resolve correctly;
* required Xbox APIs have suitable implementations;
* intentional stubs are documented;
* representative gameplay is stable;
* input works;
* graphics work sufficiently;
* audio works sufficiently;
* save/load works if supported by the title;
* serious known divergences are documented.

Reaching a loading screen or title screen is a milestone, not completion.

# Core Behavioral Rule

Never ask:

```
"How can I make the game get farther?"
```

Ask:

```
"What does the original Xbox 360 program expect to happen here, and what evidence can establish that?"
```

Then make ReXGlue reproduce that behavior.

