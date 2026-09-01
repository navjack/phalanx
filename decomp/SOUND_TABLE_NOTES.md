# Sound command table boundary

The `$1F:896F` command path indexes `$1F:8AD3` with `A << 2` and reads three
successive bytes into direct-page `$10`, `$11`, and `$12`.  The next known
executable entry is `$1F:8B9F`, so `$1F:8AD3..$1F:8B9E` is a 204-byte table
candidate (51 four-byte records), not executable code.  Its records include
little-endian pointers into the sound/event data area and several zero
records.  Keep it classified as data until the command-ID fan-out is fully
mapped; the boundary itself is established by the indexed access and the
verified `$1F:8B9F` entry point.
