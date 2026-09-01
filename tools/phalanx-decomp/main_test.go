package main

import (
	"path/filepath"
	"strings"
	"testing"
)

func repoPath(parts ...string) string {
	all := append([]string{"..", ".."}, parts...)
	return filepath.Join(all...)
}

func TestSPC700SegmentRequiresARAMAddressAndUsesSPCAssemblerMode(t *testing.T) {
	missing := Layout{Segments: []Segment{{Name: "spc", PCStart: 0, PCEnd: 3, Kind: "spc700_code", Source: "driver.asm"}}}
	if err := validateSegments(missing); err == nil {
		t.Fatal("SPC700 segment without an ARAM address was accepted")
	}
	aram := 0x0800
	l := Layout{Segments: []Segment{{Name: "spc", PCStart: 0, PCEnd: 3, Kind: "spc700_code", Source: "driver.asm", SPCARAMStart: &aram}}}
	asm, err := generateAssembly(l, nil, "/source")
	if err != nil {
		t.Fatal(err)
	}
	for _, want := range []string{"arch spc700", "base $0800", "incsrc \"/source/driver.asm\"", "assert pc() == $0803", "arch 65816"} {
		if !strings.Contains(asm, want) {
			t.Fatalf("generated SPC700 assembly missing %q:\n%s", want, asm)
		}
	}
}

func TestReferenceLayoutAndROM(t *testing.T) {
	l, err := loadLayout(repoPath("decomp", "layout.json"))
	if err != nil {
		t.Fatal(err)
	}
	rom, hash, err := readROM(repoPath("ROMS", "Phalanx (USA).sfc"), l.ROMSize)
	if err != nil {
		t.Fatal(err)
	}
	if hash != expectedSHA256 {
		t.Fatalf("reference hash = %s", hash)
	}
	if err := validateChecksum(rom); err != nil {
		t.Fatal(err)
	}
	if l.Segments[0].Kind != "65816_code" || l.Segments[0].PCEnd != 195 {
		t.Fatalf("unexpected reset segment: %+v", l.Segments[0])
	}
}

func TestLoROMAddressMapping(t *testing.T) {
	cases := map[int]int{
		0x000000: 0x008000,
		0x007FFF: 0x00FFFF,
		0x008000: 0x018000,
		0x0F8000: 0x1F8000,
	}
	for pc, want := range cases {
		if got := pcToSNES(pc); got != want {
			t.Errorf("pcToSNES(%#x) = %#x, want %#x", pc, got, want)
		}
	}
}

func TestExecutableSegmentsCannotBorrowROM(t *testing.T) {
	l := Layout{
		FormatVersion: 1,
		Mapping:       "lorom",
		ROMSize:       4,
		ROMSHA256:     expectedSHA256,
		Segments: []Segment{
			{Name: "bad", PCStart: 0, PCEnd: 4, Kind: "65816_code", Source: "base_rom"},
		},
	}
	if err := validateSegments(l); err == nil {
		t.Fatal("base_rom executable segment was accepted")
	}
}
