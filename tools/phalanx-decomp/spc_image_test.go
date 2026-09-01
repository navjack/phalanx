package main

import "testing"

func TestExtractSPCDriverImageRequiresMatchingARAM(t *testing.T) {
	rom := make([]byte, spcDriverROMPC+spcDriverByteCount)
	aram := make([]byte, spcDriverARAM+spcDriverByteCount)
	for i := 0; i < spcDriverByteCount; i++ {
		rom[spcDriverROMPC+i] = byte(i)
		aram[spcDriverARAM+i] = byte(i)
	}
	payload, err := extractSPCDriverImage(rom, aram)
	if err != nil || len(payload) != spcDriverByteCount {
		t.Fatalf("payload=%d err=%v", len(payload), err)
	}
	aram[spcDriverARAM+3] ^= 1
	if _, err := extractSPCDriverImage(rom, aram); err == nil {
		t.Fatal("different ARAM payload accepted")
	}
}
