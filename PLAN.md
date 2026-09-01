# Phalanx USA Matching Decompilation

## Summary

Build an assembly-first, ROM-clean decompilation alongside the existing native runner. The authoritative result will be readable, commented 65816/SPC700 source that reconstructs a fresh 1 MiB ROM with SHA-256 `0663330bc061f4b768fa1806610878ef6e6cf546f36041ae087c8e55703693b8`.

The current SNESRecomp output remains a runtime oracle and analysis seed; it is not treated as matching-ROM source. Its 126 known entry points cover an upper-bound span of roughly 24.9 KiB across banks `$00`, `$01`, and `$1F`.

## Implementation

- Add a separate `decomp/` tree and pin official Asar v1.91 at commit `6a6dbdaed8cb773e583052ce4f66375bd896c3a1`. Asar supports creating new ROMs plus 65816 and SPC700 assembly, LoROM addressing, symbols, and explicit placement. [Asar repository](https://github.com/RPGHacker/asar) and [user manual](https://rpghacker.github.io/asar/asar_19/manual/).
- Add one project-owned Go tool with `build`, `verify`, and `report` commands. It will:
  - Reject missing, headered, beta, incorrectly sized, or incorrectly hashed base ROMs.
  - Extract non-code ranges into ignored build storage.
  - Audit a committed layout manifest for gaps, overlaps, and out-of-range segments.
  - Assemble into a newly created output—not a patched copy of the base ROM.
  - Report the first differing ROM offset and SNES address on failure.
- Model every ROM byte in the layout manifest as `65816_code`, `spc700_code`, `data`, or `unknown`, with either an assembly source or extracted-data backing. Progress reports separately track opaque executable bytes so data conversion cannot inflate decompilation progress.
- Bootstrap a byte-identical 32-bank scaffold using extracted ranges, then replace executable ranges with assembly incrementally. Once a range becomes assembly, no original code bytes may back it.
- Use explicit instruction/address widths, fixed origins, bank-boundary assertions, and routine-end assertions. Disable Asar’s automatic checksum rewrite and reproduce the original header/checksum bytes directly.

## Decompilation Order

1. Convert bank `$00` reset, NMI, IRQ, initialization, and main-loop orchestration first; establish hardware-register and WRAM naming conventions.
2. Convert bank `$01`, currently only two discovered entry points, to close the small cross-bank subsystem.
3. Convert bank `$1F` leaf-to-root using the call graph, prioritizing the presently unresolved call chains at `$00:8432`, `$00:889E`, `$1F:8B9F`, and `$1F:8E62`.
4. Revisit remaining isolated bank `$00` regions and perform static closure over every direct branch/call, vector, jump table, and function-pointer table.
5. Instrument the existing headless interpreter to produce deterministic executable/read coverage with M/X, DB, and direct-page state. Merge title-demo coverage with committed input movies covering gameplay, stages, bosses, death/continue, options, and ending paths.
6. Classify remaining bytes using both runtime evidence and explicit data-format analysis. The 65816 phase is complete only when no `65816_code` or `unknown` range remains opaque.
7. Trace the APU upload protocol, identify the embedded audio program and tables, then replace all `spc700_code` payloads with matching commented SPC700 assembly. Music/sample data may remain extracted assets.

Each routine will carry concise evidence-based documentation: entry M/X state, DB/direct-page assumptions, callers, clobbers, important WRAM/hardware effects, and control-flow purpose. Unknown routines retain address-based names until behavior is proven.

## Developer Interfaces

- One command builds and verifies the ROM from the locally supplied USA base ROM.
- The build emits an ignored `.sfc`, an Asar symbol map, a byte-difference report on failure, and a code/data coverage report.
- No ROM, extracted graphics, music, maps, or binary code blobs are committed.
- The existing macOS application and runtime APIs remain unchanged; the rebuilt ROM is additionally exercised through the existing oracle.

## Test and Acceptance Plan

- Require an exact 1,048,576-byte output, byte-for-byte `cmp`, the expected SHA-256, checksum `$6C3F`, complement `$93C0`, and complement-plus-checksum `$FFFF`.
- Run the rebuilt ROM through the existing 300-frame oracle; retain its current forced-blank, enabled-screen, and joypad checks.
- Test wrong-ROM rejection with the present beta ROM and test headered-input rejection explicitly.
- Fail on layout gaps, overlaps, stale extracted assets, implicit instruction widths, bank overflow, or assembly-backed ranges that differ from the reference.
- Require deterministic coverage output for identical ROM/input-movie pairs and reject conflicting M/X decoding or control-flow targets that land inside an instruction.
- Final 65816 acceptance: every main-CPU executable byte is assembly-backed and commented; remaining opaque ranges are proven non-code data.
- Final project acceptance: every embedded SPC700 executable byte is also assembly-backed, the ROM still matches exactly, and a repository audit finds no tracked or bundled ROM-derived binaries.

## Assumptions

- Assembly is the matching build source; optional C-like pseudocode is documentation only.
- The USA ROM is the sole matching target.
- Main 65816 completion precedes SPC700 completion.
- The current uncommitted native-runner work and pinned SNESRecomp checkout are preserved intact.
- ROM-backed exactness is a local/restricted test; public CI must not claim byte-identical validation unless it receives an authorized private ROM.
