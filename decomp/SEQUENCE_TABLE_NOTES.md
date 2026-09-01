# SPC sequence table reconnaissance

The reconstructed 10,000-frame ARAM image provides a stronger classification
for the `$1700` and `$2000` uploads.  Both begin with the same shaped table:

* little-endian pointers into the containing region;
* a zero separator after each group of entries; and
* compact event bytes immediately after the table.

Representative entries are:

```text
$1700: $1724, $1734, $1744, $1754, $1764, ... $1AE7, ...
$2000: $2012, $2022, $2032, $2042, $2052, ... $22D8, ...
```

The first event blocks begin at `$1780` and `$2080`; both contain repeated
`E0/E1/ED/E3` control fields followed by compact note/value bytes.  The
parallel layout, pointer alignment, and self-referential destinations make
these sequence/header tables, not executable SPC700 code and not raw BRR
streams.  This is now an evidence-backed data classification target for the
manifest; the driver at `$0800–$15EE` remains the executable candidate.

The event bodies are not byte-identical copies: a direct comparison of the
first 128 bytes at `$1780` and `$2080` diverges after the shared header/control
prefix, while retaining the same recurring `E0/E1/ED/E3` opcode families.
That is consistent with separate track variants using one common sequence
language. It is not sufficient by itself to assign a song number; command and
DSP traces are still required for that mapping.

The repeatable scanner is:

```text
go run ./tools/phalanx-decomp sequence -aram /tmp/aram10000.bin -start 0x1700
```

It prints each 16-bit entry and marks pointers local to the sequence region;
it does not interpret event bytes.

## Live decoder evidence

The Sound Test capture now records every SPC read in `$1600–$3FFF`, tied to
the driver instruction that performed it. The stream-byte load is verified at
`$13CB` (`MOV A,[$14]+Y`); its post-operand trace PC is `$13CD`. This makes
the first service burst a reliable per-preview stream-entry mapping rather
than a pointer-table guess:

```text
Sound 01: $1792, $17A3, $17B3, $17C3, $17D8
Sound 02: $1768, $17EC, $1C6D
Sound 10: $2600
Sound 11: $2090, $2101, $2153
```

At `$13DF`, the driver compares the fetched byte with `$E0`; values below
that boundary follow the note/data path, while `$E0` and higher dispatch
sequence controls. This directly validates the recurring `E0/E1/ED/E3`
families as bytecode controls.

Use the reproducible reader instead of a raw pointer scan when assigning a
preview to a sequence stream:

```text
go run ./tools/phalanx-decomp sequence-trace \
  -log build-decomp/sound-captures/01/spc-data.log
```

## Control dispatcher and proven arities

The stream scanner only advances over controls. The actual effect dispatcher
is `$0B19`, which receives the control byte in A and calls `$101C` to resolve
an effect handler. Live register traces across all preview IDs prove:

| Byte | Operands | Handler | Evidence-backed action |
|---|---:|---:|---|
| `E0` | 1 | `$1038` | Selects an instrument descriptor and writes voice SRCN/ADSR fields. |
| `E1` | 1 | `$1091` | Sets a masked 5-bit per-voice pan value. |
| `E3` | 3 | `$10B8` | Configures per-voice vibrato: delay, depth, and rate. |
| `ED` | 1 | `$1141` | Sets a per-voice volume level. |
| `EF` | 3 | `$1167` | Reads target low/high plus count and rebases the stream. |

For example, an observed `$EF,$FB,$1B,$01` event at `$183E` causes the next
sequence read at `$1BFB`, proving the 16-bit target plus count form.
No musical-format labels in this table are inferred solely from common SPC
driver conventions; each has a captured downstream state and DSP path.

## Handler-to-DSP data-flow evidence

The diagnostic capture also writes `spc-state.log`. It records every access
to voice state, the driver DSP-dispatch tables, and `$F2/$F3`, with the
executing SPC instruction. In Sound 01, a live `E0 $06` at `$1038` writes
`$0211=$06`, then `$107B/$107E` write DSP `$04=$06`, `$05=$FF`, `$06=$E7`,
and `$07=$B8`: the selected descriptor is therefore the voice's source and
ADSR program, not merely an abstract program number.

The same trace proves the stereo-volume data flow. `E1` writes `$0351` and
`$0331`; `ED` writes `$0301`. Routine `$1526` consumes `$0301` to produce the
voice level at `$0321`; `$1370` combines `$0321` with `$0351` and the paired
table to write DSP `$00/$01` (voice 0 left/right volume). The equivalent
per-voice accesses produce `$10/$11`, `$20/$21`, and so on. This is direct
runtime evidence for the pan and volume labels above.

`E3` closes through the pitch path. Its handler writes the first field to
`$02B0+X`, the second to `$02A1+X`, and the third to `$02C1+X` plus the
direct-page rate register `$B1+X`. The `$143B–$1482` tick code waits until
the `$B0+X` delay counter reaches `$02B0+X`, reloads the rate from `$02C1+X`,
accumulates the depth from `$02A1+X` into `$02A0+X`, and then reaches `$09DE`.
The captured writes are DSP `$02/$03`, `$12/$13`, `$22/$23`, and `$32/$33`:
the four active voices' pitch low/high registers. This proves the
delay/depth/rate vibrato contract.

The repeatable arity analysis is:

```text
go run ./tools/phalanx-decomp sequence-events \
  -data build-decomp/sound-captures/01/spc-data.log \
  -exec build-decomp/sound-captures/01/spc-exec.log
```

It emits both the scanner advance and the runtime-resolved handler, so the
table above is reproducible from the captured state rather than maintained as
a manual claim. The same Sound 01 trace also resolves `E7 -> $10F6` and
`FA -> $122D`. Both take one byte through the resolver's operand reader.
`E7` clears direct-page `$00/$01`; `FA` stores its operand in direct-page
`$0E`. Their later consumers are not yet closed, so they deliberately remain
outside the musical semantic table.
