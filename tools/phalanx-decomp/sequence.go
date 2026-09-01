package main

import (
	"flag"
	"fmt"
	"os"
)

// sequence prints the self-relative little-endian pointers used by the
// observed SPC sequence headers. It is intentionally a reader only: no
// guessed event decoding is performed here.
func sequence(args []string) error {
	fs := flag.NewFlagSet("sequence", flag.ContinueOnError)
	aramPath := fs.String("aram", "", "64 KiB ARAM image")
	start := fs.Int("start", 0x1700, "header start address")
	count := fs.Int("count", 32, "number of 16-bit entries")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *aramPath == "" {
		return fmt.Errorf("-aram is required")
	}
	b, err := os.ReadFile(*aramPath)
	if err != nil {
		return err
	}
	if len(b) != 0x10000 {
		return fmt.Errorf("ARAM is %d bytes, want 65536", len(b))
	}
	if *start < 0 || *start+*count*2 > len(b) || *count < 1 {
		return fmt.Errorf("header range is outside ARAM")
	}
	for i := 0; i < *count; i++ {
		off := *start + i*2
		ptr := int(b[off]) | int(b[off+1])<<8
		if ptr == 0 {
			fmt.Printf("%02d: $0000\n", i)
			continue
		}
		class := "external"
		if ptr >= *start && ptr < *start+0x1000 {
			class = "local"
		}
		fmt.Printf("%02d: $%04X %s\n", i, ptr, class)
	}
	return nil
}
