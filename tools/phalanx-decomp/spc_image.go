package main

import (
	"bytes"
	"flag"
	"fmt"
	"os"
)

const (
	spcDriverROMPC     = 0x0f3242 // USA-ROM $1E:B242
	spcDriverARAM      = 0x0800
	spcDriverByteCount = 3567
)

func extractSPCDriverImage(rom, aram []byte) ([]byte, error) {
	if len(rom) < spcDriverROMPC+spcDriverByteCount {
		return nil, fmt.Errorf("ROM too short for SPC driver payload")
	}
	if len(aram) != 0 && len(aram) < spcDriverARAM+spcDriverByteCount {
		return nil, fmt.Errorf("ARAM image too short for SPC driver payload")
	}
	payload := append([]byte(nil), rom[spcDriverROMPC:spcDriverROMPC+spcDriverByteCount]...)
	if len(aram) != 0 && !bytes.Equal(payload, aram[spcDriverARAM:spcDriverARAM+spcDriverByteCount]) {
		return nil, fmt.Errorf("ROM $1E:B242 payload differs from ARAM $0800 driver image")
	}
	return payload, nil
}

func spcImage(args []string) error {
	fs := flag.NewFlagSet("spc-image", flag.ContinueOnError)
	romPath := fs.String("rom", "ROMS/Phalanx (USA).sfc", "verified USA ROM")
	aramPath := fs.String("aram", "", "optional captured 64 KiB ARAM image to compare")
	outPath := fs.String("out", "", "optional ignored driver payload output")
	if err := fs.Parse(args); err != nil {
		return err
	}
	rom, _, err := readROM(*romPath, 1024*1024)
	if err != nil {
		return err
	}
	var aram []byte
	if *aramPath != "" {
		aram, err = os.ReadFile(*aramPath)
		if err != nil {
			return err
		}
	}
	payload, err := extractSPCDriverImage(rom, aram)
	if err != nil {
		return err
	}
	if *outPath != "" {
		if err := os.WriteFile(*outPath, payload, 0600); err != nil {
			return err
		}
	}
	if len(aram) != 0 {
		fmt.Printf("USA-ROM $1E:B242-$1E:C030 exactly matches ARAM $0800-$15EE (%d bytes)\n", len(payload))
	} else {
		fmt.Printf("extracted USA-ROM $1E:B242-$1E:C030 (%d bytes)\n", len(payload))
	}
	return nil
}
