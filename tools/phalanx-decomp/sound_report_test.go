package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestReadDSPEvidenceResolvesKONSourcesThroughDirectory(t *testing.T) {
	dir := t.TempDir()
	logPath := filepath.Join(dir, "dsp.log")
	regsPath := filepath.Join(dir, "dsp-registers.bin")
	aramPath := filepath.Join(dir, "aram.bin")
	// Voice 0 gets SRCN $02 and is started by KON bit 0. Voice 1's SRCN is
	// intentionally not keyed on, proving the report does not list merely
	// configured-but-unused sources.
	log := "# index sample type addr value aux producer\n0 0 1 04 02 0 1\n1 1 1 14 03 0 1\n2 2 1 4C 01 0 1\n"
	if err := os.WriteFile(logPath, []byte(log), 0600); err != nil {
		t.Fatal(err)
	}
	regs := make([]byte, 0x80)
	regs[0x5d] = 0x20
	if err := os.WriteFile(regsPath, regs, 0600); err != nil {
		t.Fatal(err)
	}
	aram := make([]byte, 0x10000)
	aram[0x2008], aram[0x2009] = 0x34, 0x12
	aram[0x200a], aram[0x200b] = 0x78, 0x56
	if err := os.WriteFile(aramPath, aram, 0600); err != nil {
		t.Fatal(err)
	}
	evidence, sources, err := readDSPEvidence(logPath, regsPath, aramPath)
	if err != nil {
		t.Fatal(err)
	}
	if evidence.regWrites != 3 || !evidence.konValues[1] {
		t.Fatalf("evidence = %#v", evidence)
	}
	if sources != "02:$1234/$5678" {
		t.Fatalf("sources = %q", sources)
	}
	if !strings.Contains(formatDSP(evidence), "kon=01") {
		t.Fatalf("signature = %q", formatDSP(evidence))
	}
}
