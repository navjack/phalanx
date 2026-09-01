# SPC700 upload reconnaissance

This is an analysis note, not a ROM-derived asset. The gated
`PHALANX_ORACLE_APU_LOG=1` mode in `tests/oracle.c` records the CPU writes to
the four APU ports. The first bulk transfer has the standard SPC IPL shape:
the CPU selects an SPC RAM address with ports 2/3, sends the `$CC` handshake
on port 0, and then sends an incrementing port-0 counter with one data byte on
port 1 for each destination byte.

The first clean upload observed during the 300-frame oracle run is:

| SPC RAM range | Bytes | Interpretation |
| --- | ---: | --- |
| `$0800–$15EE` | 3,567 | SPC driver/program candidate; entry bytes begin `20 CD CF BD E8 00` |
| `$3E00–$3E95` | 150 | small table/data block |
| `$3F00–$3F17` | 24 | small table/data block |
| `$BF80–$F0FF` | 12,672 | BRR/sample or large audio-data block |
| `$3C00–$3C6F` | 112 | table/data block |
| `$4000–$BF7F` | 32,640 | large audio-data block |
| `$1600–$16FD` | 254 | sequence/table block |
| `$1700–$1EA0` | 1,953 | sequence/table block |
| `$2C00–$3B8C` | 3,981 | sequence/table block |

## ROM-source provenance

The gated uploader read log resolves the ROM source of each ARAM transfer from
the actual `$1F:8A64` indirect-long reads. The stable resident driver is not
an orphaned ARAM artifact: its 3,567 payload bytes are copied from USA-ROM
`$1E:B242–$1E:C030` into ARAM `$0800–$15EE`.

Thirty-nine trace-proven regions (1,182 bytes total) are now `spc700_code`
layout segments backed by `spc700_init.asm`, `spc700_dsp_dispatch.asm`, and
`spc700_timer_poll.asm`, plus `spc700_voice_scan_a.asm` and
`spc700_voice_scan_b.asm`, plus the live sequence handlers
`spc700_sequence_e0.asm`, `spc700_sequence_e1.asm`,
`spc700_sequence_e3.asm`, and `spc700_sequence_ed.asm`. The pinned Asar
build also promotes `spc700_sequence_ef.asm`, `spc700_control_e7.asm`, and
`spc700_control_fa.asm`, and `spc700_vibrato_service.asm`, with their
corresponding ARAM bases, and now `spc700_sequence_scanner.asm`. It places all
resulting bytes at their ROM upload locations; the normal exact-ROM comparison
proves both address spaces remain correct. The set covers low-ARAM
initialization, the live DSP-dispatch entry gate, register dispatch and
state/flag handoff, timer-0 service entry and its slow-service gate, both
active-voice scans, the traced instrument/pan/vibrato/volume/stream-rebase
handlers, the 80-byte vibrato service that carries E3 state to pitch
generation, the actual stream-byte scanner/control-width lookup, and now the
handler-return/stream-width resolver at `$101C`, plus the live Sound Test
port-handshake primer at `$159C`, handshake, pointer-update, and DSP-port-helper
path at `$15A1–$15EE`.
The matching slow periodic service path at `$0880` also now proves the
timer-derived `POP Y; MUL YA; CLRC` phase scaling and its global/voice call
order. The shared DSP write helper at `$09DB`, its live `$09D3` selector gate,
and the preceding 131-byte `$0950` per-voice DSP arithmetic/register-selection
service are now matching source as well. The trace-proven `$09E2` stream-word
reader, `$09F0` stream-pointer/mode setup helper, and `$0A0F` per-voice state
initializer are matching source too.
The 71-byte front half of the common voice service at `$148B` is matching
source too, including its proven pitch/volume state handoffs, and its
50-byte `$14D2–$1503` tail now completes that routine's matching source, and
the subsequent 42-byte `$1504` helper is matching source as well. The
39-byte `$1414` note-tick gate is matching source too: it carries normal
sequence flow into per-voice timing, DSP-selector setup, and the
vibrato/inactive-voice decision. Every
promoted SPC700 segment now carries an assembler ARAM-end assertion, preventing
an overlong source from being overwritten by a following data segment. The E0
instrument-descriptor pointer setup at `$1044` now has matching source as well.
Its `$105D` descriptor-to-DSP loop is matching source too, including the four
`$F2/$F3` register writes and per-voice descriptor pair store. The remaining
2,385 bytes
stay `data` sourced from the verified ROM until their instruction boundaries
and semantics are independently proven.

`spc700_driver_working.asm` remains the broader analysis listing. It is not a
build input and must not be treated as proof that the unpromoted bytes are
source-complete.

```text
go run ./tools/phalanx-decomp upload-source \
  -log build-decomp/sound-captures/01/apu-source.log

go run ./tools/phalanx-decomp spc-image \
  -aram build-decomp/sound-captures/01/aram.bin \
  -out build-decomp/spc700-driver.bin
```

The same report records all observed source/target sessions. In Sound 01 it
also proves ROM `$1D:8078` feeds the large ARAM sample block `$4000–$BF7F`,
while later `$1E:C0xx`, `$1E:D1xx`, and `$1A:Fxxx` ranges feed the Sound Test
tables and selected sequence data. These are trace-derived mappings; they do
not classify sequence bytes as executable driver code.

The later ranges are loaded after the driver and are interleaved with normal
sound-command traffic, so the extractor must retain upload-session boundaries
and must not classify every port-0/port-1 pair as a bulk transfer. The next
step is to reconstruct one 64 KiB ARAM image from these sessions, disassemble
the `$0800` program with SPC700 instruction boundaries, and prove which of the
remaining ranges are code, sequence data, BRR samples, or DSP tables.

## Offline execution trace

The oracle has a separate, non-default trace mode for the reconstructed image:

```text
PHALANX_SPC_TRACE_ARAM=/tmp/phalanx-aram.bin \
  build-macos/phalanx_oracle "ROMS/Phalanx (USA).sfc"
```

It copies the 64 KiB image into the real SPC core, starts at `$0800`, and
prints the pre-instruction PC/opcode for 20,000 instructions. The trace
confirms the initialization loop at `$0807–$080A`, reaches the setup calls at
`$09DB` and `$11E7`, and then waits in the command/service loop at `$0869` and
`$086C` when no CPU port command is supplied. The trace advances the real APU
timers between SPC instructions, so it reaches the periodic service path at
`$0880–$0898` and then the handler table around `$1201–$1226`. This is the
first runtime proof of the `$0800` prologue’s control flow; the service path
will be driven with captured port traffic next.

The trace mode also injects the first captured non-bulk handoff (`$A4` on
port 0, with the captured companion port values) at instruction 6,000. That
causes execution to leave the idle wait and reach handler code at `$08A5`,
`$08BF`, and `$08D5`, providing a concrete first command-dispatch path to
disassemble. The injection is limited to the opt-in trace mode and does not
change normal oracle behavior.

The same mode reports changes to the SPC DSP register mirror. The `$A4`
handoff currently produces initialization writes at `$6C`, `$7D`, `$6D`,
`$0C`, `$1C`, and `$5D`, followed much later by `$5C` changes. It does not
yet establish a sustained musical voice; the next captures must replay the
subsequent command sequence and look for `$4C` (KON), per-voice pitch/volume,
and source-number writes.

The latest 20,000-instruction run makes the later behavior precise: at
instruction 16,926 the driver writes DSP `$5C` with `$3F`, then clears it at
16,941. The command therefore reaches the DSP/voice-state path, but this
short title capture still does not select a sustained song.

The trace length is now configurable with `PHALANX_SPC_TRACE_STEPS`. A
100,000-instruction run reaches real voice activity: DSP `$4C` (KON) changes
to `$28` at instruction 35,408 and clears at 35,582. It also shows repeated
source/echo state changes in `$39`, `$59`, and `$5C`. This is the first direct
proof that the reconstructed driver can be driven past initialization into a
timed voice event; the next step is correlating those writes with sequence
bytes and BRR source numbers.

The first KON window is now correlated to nearby DSP writes: `$54=$0E`,
`$56=$E0`, `$53=$08`, `$30/$31=$0F`, and `$50/$51=$2B` precede KON `$4C=$28`.
Immediately after KON, `$38/$58` move from `$40` to `$7F`. This is the first
concrete voice/echo parameter signature to match against sequence commands
and BRR directory entries.

The oracle now accepts deterministic one-frame input scripts through
`PHALANX_ORACLE_INPUT_SCRIPT`, using entries like
`2:Start,40:Down+A,80:Right`. This is the mechanism for driving the sound
menu preview list from reset while retaining the normal input path.

A 10,000-frame exploratory run reaches a later demo transition around frame
9,800 (the normal oracle intentionally reports a presentation-state failure
there). The filtered APU log adds pairs `$00,$00` at frames 9800/9822/9833/9843
and `$01,$00` at 9853, followed by new uploads at `$2000–$2BF1`. The audio
parser now ignores interleaved oracle status lines, so these longer diagnostic
captures remain machine-readable without weakening strict numeric log parsing.

The late upload is split into `$1700–$1DFC`, `$2000–$253D`, and
`$2540–$2BF1`. Because `$0800–$15EE` is the stable driver image and these
ranges arrive only at the demo transition, they are strong candidates for
sequence/state tables or song-specific assets rather than driver code. They
are now recorded as the next extraction targets for BRR/header and sequence
format scanning.

Allowing the normal oracle to run for 3,000 frames (`PHALANX_ORACLE_FRAMES`)
produces additional short CPU-to-SPC pairs that the 300-frame capture misses:
frame 427 carries `$02,$03`, and frame 950 carries `$01,$15`. These are the
first nonzero short event values in the live title/demo path and are the
initial command IDs to correlate with the `$1F:8AD3` table and SPC sequence
state.

Reconstructing ARAM from that 3,000-frame log preserves the same driver image
and reproduces the two-voice KON signature. The longer image also shows DSP
`$24/$26` changes immediately before the voice setup, providing candidate
pitch/source fields to trace when the command pairs are replayed individually.

Interpreting the DSP layout, `$30/$31` are voice 3 left/right volume, and
KON mask `$28` starts voices 3 and 5 simultaneously (bits 3 and 5). Thus the
first observed event is a two-voice onset, not a single-channel click; the
corresponding sequence event must program both voice state blocks.

The first 0x70 bytes of both `$1700` and `$2000` are visibly little-endian
pointer tables: entries such as `$1724`, `$1734`, `$20C8`, `$214C`, and
`$22D8` point deeper into the same ARAM regions, followed by compact event
values. These ranges are therefore sequence/header tables with offsets into
song data, not raw BRR sample streams.

## Reproducible command inventory

The parser currently extracts this command inventory from the 3,000-frame
capture (zero-valued pairs are retained because they are part of the live
protocol):

```text
frame  54: 00 00    frame  82: 00 00
frame 143: 00 00    frame 232: 00 00
frame 427: 02 03    frame 950: 01 15
```

The 10,000-frame exploratory capture adds `00 00` at frames 9800, 9822,
9833, and 9843, followed by `01 00` at frame 9853.  These are diagnostic
observations, not yet a sound-test mapping: the next capture must start from a
clean reset and use one explicit menu selection per preview number.
