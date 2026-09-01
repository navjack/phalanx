# Sound-test preview capture

The in-game sound-test menu provides a deterministic, user-visible way to
select the level-preview music.  During the observed session the display
showed these sound numbers (the menu can advance past entries when a key is
held, so this is an observation log rather than a claim that the IDs are
contiguous):

```text
01  03  04  06  09  10  12  10  06  00
```

The screen identifies itself as `SOUND TEST`, displays `IN AREA 269`, and
shows `PUSH START BUTTON TO EXIT`.  This establishes that the previews are
selected by an explicit sound number and can be exercised without relying on
an arbitrary gameplay route.

## Next capture contract

For each displayed number, hold no navigation key and press the preview
control once.  Capture the CPU-to-APU port log from reset through selection,
then retain the corresponding reconstructed ARAM image and DSP event trace.
The resulting table will link:

* sound-test number;
* 65816 command bytes and `$1F:8AD3` record;
* SPC sequence/header address;
* BRR source number and loop metadata; and
* cycle/timestamped DSP writes and rendered PCM.

Until that capture is made, the observed numbers are menu evidence only; they
are not being used as a guessed song-table mapping.

## Verified menu route

The local build was reset and observed through the title flow.  After the
title prompt, the main menu opens with `EXECUTE MISSION` selected.  The sound
test entry is reached with this button sequence:

```text
Start       open the main menu
Down        select SYSTEM CONFIGURATION
Down        select SOUND TEST
Start       enter SOUND TEST
```

Once inside, the displayed number begins at `00`; in the headless oracle a
single debounced `Right` changes it, and `A` triggers the preview. This is the
route the oracle's frame-script encodes.

In the live observation, the reset disclaimer/credit sequence lasted roughly
8–10 seconds before the title screen accepted `Start` (approximately
480–600 NTSC frames).  This is only a search window for oracle calibration,
not yet a committed timing claim.

The headless oracle runs the same reset sequence on a different wall-clock
schedule. A frame dump proves that a `Start` pulse near frame 1,200 reaches
the title/main-menu flow; the earlier 480–600 estimate must therefore not be
used as an oracle timing constant.

## Proven oracle route

The headless oracle now reaches the real Sound Test deterministically:

```text
1200-1205: Start    title/pilot flow
1800-1805: Start    open the main menu
2200: Down          select SYSTEM CONFIGURATION
2250: Down          select SOUND TEST
2350: Start         enter SOUND TEST (SOUND NUMBER 00)
2450: Right         change 00 to 01
```

Frame dumps prove the final `SOUND TEST` screen and the `00 → 01` change.
Two clean-reset captures of sound number `00` with a preview attempt at frame
2600 produced byte-identical APU logs, reconstructed ARAM
(`27fb4c3a838b6fc9875632569e41e24d998b3bb8d586efdb4e54e3481b584fb9`),
and WRAM dumps. The next capture pass can therefore enumerate numbers with
debounced `Right` presses from this fixed route.

The first per-ID comparison now proves the capture path is selecting distinct
audio state: Sound `01` has a different APU-log hash
(`78cfa3b26f101b5b6e985f07ca9a8de3915b9c50ac395e3d07e5e6a1c0aa1078`),
different reconstructed ARAM
(`e0819d0927358c8a5ce0f0f64f65a2a2a53e544789634551d3ef720295fbf686`),
and a final non-bulk command `$01,$00` at frame 2675. Its preview upload adds
`$1700–$1DFC`, whereas the Sound `00` capture takes the corresponding zero
command path. This is the first ground-truth Sound Test number-to-SPC event
mapping.

Direction presses require a 250-frame debounce in the headless menu. A
50-frame sweep visibly advanced thirteen intended presses only to Sound `02`;
three presses separated by 250 frames visibly reach Sound `03`. Capture
scripts therefore use 250-frame spacing and prior 02–12 artifacts must be
regenerated before being treated as their nominal IDs.

With that spacing, thirteen `Right` presses produce a frame dump of `SOUND
NUMBER 00`. The sound-test selector therefore wraps after `12`; `00–12` is
the proven complete preview range. The generated wraparound artifact was
moved out of the capture report rather than being retained as a fictional
Sound `13` entry.

Two first oracle probes used one-frame button events at 60-frame intervals:

```text
600:Start,660:Down,720:Down,780:Start,840:A
540:Start,600:Down,660:Down,720:Start,780:A
```

Neither produced a new sound command in the APU inventory beyond the normal
title/demo traffic.  This narrows the remaining calibration problem: the
route itself is known, but the title prompt's frame acceptance and the sound
preview button/hold duration still need to be measured rather than inferred.

The oracle script now also accepts held ranges such as
`500-560:Start` (in addition to one-frame entries).  A held-button probe over
the estimated title window likewise produced only baseline traffic, so the
title timing remains an empirical calibration target rather than a hard-coded
assumption.

The opt-in named-phase form is `phase=start-end`, for example:

```text
title_start=540-560,menu_down_1=650-655,menu_down_2=700-705,sound_enter=750-755
```

`PHALANX_ORACLE_DIAGNOSTIC_LOG=/path/file` additionally emits one JSON-like
text record per frame with the scripted input and PPU presentation markers.

## Current capture evidence

`go run ./tools/phalanx-decomp sound-capture -id N` creates a self-contained,
ignored capture directory. In addition to the raw APU log, ARAM image, PCM
WAV, frame diagnostics, and standalone SPC trace, it records the **live**
native-S-DSP register event stream, final DSP register image, and SPC opcode
coverage for the 500-frame preview window. It also retains an ordered
`spc-exec.log`: each record is the real fetched SPC PC and opcode immediately
before execution, so adjacent records are observed control-flow edges. Its
manifest preserves every
observed non-bulk CPU-to-SPC port-0/1 pair and every reconstructed ARAM upload
range. `sound-report` derives the same wire evidence from older raw logs, so
that portion of the table does not depend on a stale manifest.

The companion `spc-ports.log` records every live SPC read of `$F4`–`$F7` with
its execution-trace instruction index. It links the 65816 write sites to the
SPC-side handshake/dispatch code without guessing from timing alone.

`spc-state.log` is an opt-in state-bus trace for the driver's modulation
working state at `$00A0–$00DF`, the per-voice state window `$0200–$03FF`, DSP
dispatch tables at `$1500–$15FF`, and DSP ports `$F2/$F3`. Every record
includes the instruction index, post-operand SPC PC, access direction,
address, and value. This supports a causal handler state-write →
DSP-register-write reconstruction without changing normal emulation.

Each current capture also contains `preview.spc`: a standard 66,048-byte SPC
snapshot taken one frame after the deterministic preview press. It contains
the native SPC registers, 64 KiB ARAM, and DSP register image at that exact
captured state. It is a portable regression/reference artifact for an
independent SPC/DSP player. `preview.wav` is the native PCM window beginning
immediately after that snapshot, so a separate player has an exact,
alignment-free post-snapshot target. A standard SPC file does **not** encode
all live S-DSP internals (envelopes, BRR decode history, echo/FIR history, and
sample-clock phase), so it alone cannot promise sample-exact warm resumption.

`preview.apu` is the companion local-only canonical state image. It records
the APU saveload walk plus the pending CPU-to-APU port scheduler, including
the live DSP state deliberately absent from `preview.spc`. The capture command
restarts the oracle from `preview.apu`, writes `preview-resume.wav`, and
requires its first 4,096 native stereo samples to equal the corresponding
prefix of `preview.wav` byte-for-byte. This proves exact continuation for the
captured emulator build; it is still not a claim that an independent player
matches that reference.

`spc-data.log` is the corresponding read trace for the uploaded
`$1600–$3FFF` data window. Its `$13CD` reader identity is the post-operand PC
of the driver's `MOV A,[dp]+Y` stream-byte load at `$13CB`. The first compact
group of those reads is now the report's `sequence_streams` field; e.g. Sound
`01` starts active streams at `$1792,$17A3,$17B3,$17C3,$17D8`. This is actual
driver access evidence, distinct from simply scanning potential pointers.

For the corrected 250-frame Right debounce, the final preview path has been
captured for every proven displayed ID `00` through `12`. The latest upload
generation distinguishes the entries: for example, `01` loads 1,789 bytes at
`$1700`, `02` loads 1,733 bytes there, while `10` uses a 1,714-byte upload at
`$2540`. These are observed transfer ranges, not yet claimed sequence
addresses or sample identities.

`sound-verify -id N` performs two new clean-reset captures in temporary,
discarded output and compares the full APU, ARAM, live-DSP, SPC-PC-coverage,
standalone-SPC-trace, and PCM/WAV hashes, including the state-bus trace when
present, the controlled `preview.spc`, and the full-state `preview.apu`
snapshot plus its exact-resume PCM. The richer artifact set
has now been regenerated for every discovered ID `00`–`12`; clean-reset
snapshot/PCM determinism is continuously checked per ID as those captures are
verified. This
establishes deterministic capture input/output evidence; it does **not**
establish sub-cycle reproduction.

The live report now resolves each DSP `SRCN` observed at `KON` through the
captured DSP directory and ARAM image, producing actual BRR start/loop
pointers. It also reports the exact DSP register-write count and observed
`KON` masks. `sequence_addr` remains explicitly `unproven`: that fact needs a
control-flow reconstruction of the real SPC700 driver, rather than inference
from transfer addresses or a standalone injected trace.
