# Matching ROM source

This directory is the source-of-truth tree for reconstructing the USA ROM.
The base ROM is an external build input. It is never copied into the
repository, and extracted ranges belong under the ignored build directory.

`layout.json` assigns every output byte to an address range. During the
bootstrap phase the entire image is `unknown` and is extracted from the local
base ROM. As ranges become real assembly, their segment is changed to a
code-kind and points at an assembly source. The verifier rejects overlaps,
gaps, malformed ranges, and a wrong base-ROM hash before any extraction.

From the repository root, `bash tools/build-rom.sh` builds Asar when needed,
assembles a fresh output, compares every byte with the verified USA reference,
and runs the existing headless boot oracle when it is available. The output,
symbols, extracted assets, and generated assembly all stay under the ignored
`build-decomp/` directory.
