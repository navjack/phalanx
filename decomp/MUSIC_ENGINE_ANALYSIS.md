# Phalanx USA music engine: verified composition path

This document describes only behavior grounded in clean-reset Sound Test
captures, live SPC execution/state traces, and byte-identical assembled
SPC700 source. It does not claim an independent player or sub-cycle-accurate
reproduction.

## From Sound Test number to a running song

The verified route is `Start → Down → Down → Start`. Sound Test IDs `00`–`12`
are captured from a fresh reset; `13` wraps to `00`. A capture records the
CPU/APU port handoff, uploaded ARAM, SPC execution, DSP writes, and native PCM.

After a preview selection, the resident SPC program reads active sequence
streams through the instruction at `$13CB`:

```text
MOV A,($14)+Y       ; F7 14
```

The `$14/$15` pointer belongs to the current voice stream. The verified Sound
01 streams begin at `$1792`, `$17A3`, `$17B3`, `$17C3`, and `$17D8`; these are
runtime reads, not speculative pointer scans.

## Event bytes

The scanner at `$13C7` distinguishes note/flow bytes from control bytes. For
control bytes it saves the old stream offset, indexes a width table at
`$11FC+control`, and adds that width to the stream offset. The dispatcher at
`$0B19` routes `E0+` bytes to the resolver at `$101C`, which selects a handler
return address from `$11DE/$11DF` and advances the per-voice stream state.

The promoted `$13ED` continuation shows that `$17` acts as a per-event delay
or countdown gate. When it expires, the engine can reload active stream pairs
from `$0230/$0231` or `$0240/$0241`; it then returns to `$13CB` to fetch the
next event byte. This is the first matching-source evidence for event timing
and stream-flow state, separate from control-byte parameter changes.

The following controls are observed in real preview streams:

| Byte | Handler | Verified effect |
| --- | --- | --- |
| `E0 xx` | `$1038` | Stores the instrument/program selector; subsequent DSP traffic writes SRCN and ADSR fields. |
| `E1 xx` | `$1091` | Stores a five-bit pan parameter that contributes to DSP voice L/R volume. |
| `E3 dd vv rr` | `$10B8` | Stores vibrato delay, depth, and rate. `$143B` consumes these fields and reaches the pitch writer. |
| `ED xx` | `$1141` | Stores the per-voice volume level used by the later mix path. |
| `EF ll hh nn` | `$1167` | Stores a stream target/count state and copies it into the active per-voice stream pair. |
| `E7 xx` | `$10F6` | Clears direct-page work word `$52/$53`. |
| `FA xx` | `$122D` | Stores the value into direct-page `$5F`. |

The names "instrument", "pan", "vibrato", and "volume" above are justified
by their traced downstream state and DSP effects. `E7` and `FA` retain only
the direct behavior proven so far.

E0's promoted continuation at `$1044` now makes the descriptor operation
direct: it computes `program × 6`, addresses the `$3E00` descriptor area, then
walks four descriptor-controlled DSP register writes through `$F2/$F3` before
storing the final descriptor pair in per-voice state.

E1's `$109F` continuation computes a complementary pan component, calls the
pan helper at `$1282`, and stores the resulting pair at `$0340/$0341+X`. The
common voice service later reads that pair before its mix-side helper call.

## Note tick, vibrato, and voice service

The matching `$1414` note-tick gate is the common continuation reached after
stream scanning or delay flow. It restores the voice DSP selector, updates two
per-voice countdown/state bytes, calls the note-side helper at `$1277`, and
then bypasses vibrato for inactive voices. This establishes the ordering:
stream event/delay processing happens before the per-voice pitch modulation
service.

That selector restoration is now source-complete at `$09D3`: it preserves the
pending DSP value while testing the voice selector mask, then either continues
into the `$09DB` DSP address/data write or branches to its return.

The full preceding `$0950` service is matching source too. It combines the
voice's work pair with the `$1577–$157A` selector tables and `$0220/$0221+X`
state, then selects the DSP register in `Y` and its pending value in `A` for
the `$09D3/$09DB` gate and write. This is the traced runtime path that turns
voice-state arithmetic into DSP-facing pitch/mix updates.

The compact `$09E2` helper now gives direct matching-source evidence for a
second stream cursor, `$40/$41`: it consumes two consecutive bytes and returns
them as an `A`/`Y` pair. Its caller-specific grammar remains intentionally
unclassified.

The following `$09F0` helper chooses the initial `$40/$41` value from the
`$15FE/$15FF` table and records the same selection in `$04/$08`, then marks its
mode in `$46`. It is observed in every captured Sound Test preview.

Before preview playback, the matching `$0A0F` routine initializes six paired
voice-state slots and calls the existing E1/pan path for each. It then seeds
the shared timer/control work bytes used by the periodic voice service.

The matching `$1277` helper is shared with the common voice service: it copies
the selected voice's `$0360/$0361+X` pair into direct-page `$10/$11` work
state before downstream pitch/mix processing.

The two call sites at `$1294` now have matching source: it first bypasses the
delta when bit 7 of `$12` is clear; otherwise it subtracts the `$10/$11`
target/work pair from the `$0E/$0F` current pair and returns the signed delta
in `YA`.

The fast service loops at `$08A5` and `$08BF` scan active voice slots and call
the shared service at `$148B`. The proven vibrato service at `$143B` consumes
the E3 fields in the `$02A0–$02C1` voice-state region. It maintains phase and
modulation state, calls `$14FC`, then jumps to `$0950`; state/DSP traces show
this path reaching the voice pitch registers.

E3's adjacent `$10CF` helper now has matching source too: it derives and
stores the `$02C0+X` modulation field that the `$143B` service later adds into
its pitch-side calculation.

This describes an engine-driven vibrato rather than pre-rendered sample
vibrato: the song bytecode supplies parameters and the SPC700 modifies pitch
over the service ticks.

## Sample and DSP evidence

At each DSP `KON`, the capture report resolves the voice `SRCN` through the
sample directory and records BRR source ranges, pitch, volume, ADSR, echo
events, and native PCM hashes. `preview.spc` is a portable music-state
reference; `preview.apu` additionally preserves live S-DSP state for an exact
native emulator resume regression. The latter is required because a standard
SPC file does not include every envelope/decode/echo-history detail necessary
for a sample-exact warm resume.

## Remaining work

The note-duration/note-on path, the helpers reached from the now-promoted
common voice service `$148B–$1503`, instrument descriptor tables, and the
complete sequence header grammar are not yet source-complete. They remain data
or analysis-only listings until execution and byte-identity evidence supports
promotion.
