package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestAnalyzeAPUWritesSeparatesCommandsAndUploads(t *testing.T) {
	writes := []apuWrite{
		{frame: 1, port: 0, value: 0x7e}, {frame: 1, port: 1, value: 0x01},
		{frame: 2, port: 2, value: 0xfe}, {frame: 2, port: 3, value: 0xff},
	}
	for i := 0; i < 17; i++ {
		writes = append(writes, apuWrite{frame: 3, port: 0, value: i}, apuWrite{frame: 3, port: 1, value: 0x80 + i})
	}
	// A subsequent address pair terminates the transfer session.
	writes = append(writes, apuWrite{frame: 4, port: 2, value: 0}, apuWrite{frame: 4, port: 3, value: 0})
	analysis := analyzeAPUWrites(writes)
	if len(analysis.commands) != 1 || analysis.commands[0] != (soundCommand{frame: 1, port0: 0x7e, port1: 0x01}) {
		t.Fatalf("commands = %#v", analysis.commands)
	}
	if len(analysis.chunks) != 1 || analysis.chunks[0] != (audioChunk{0xfffe, 17, 2}) {
		t.Fatalf("chunks = %#v", analysis.chunks)
	}
	if analysis.aram[0xfffe] != 0x80 || analysis.aram[0xffff] != 0x81 || analysis.aram[0] != 0x82 {
		t.Fatalf("wrapped ARAM payload is wrong: %02x %02x %02x", analysis.aram[0xfffe], analysis.aram[0xffff], analysis.aram[0])
	}
}

func TestReadAPUWritesRejectsMalformedNumericRows(t *testing.T) {
	path := filepath.Join(t.TempDir(), "apu.log")
	if err := os.WriteFile(path, []byte("oracle status\n1 0 zz\n"), 0600); err != nil {
		t.Fatal(err)
	}
	_, err := readAPUWrites(path)
	if err == nil || !strings.Contains(err.Error(), "parse value") {
		t.Fatalf("readAPUWrites error = %v", err)
	}
}

func TestReadAPUWritesAllowsOracleStatus(t *testing.T) {
	path := filepath.Join(t.TempDir(), "apu.log")
	if err := os.WriteFile(path, []byte("oracle boot complete\n12 3 af 1F88A2\n"), 0600); err != nil {
		t.Fatal(err)
	}
	writes, err := readAPUWrites(path)
	if err != nil || len(writes) != 1 || writes[0] != (apuWrite{frame: 12, port: 3, value: 0xaf, cpuPC: 0x1f88a2}) {
		t.Fatalf("writes = %#v, err = %v", writes, err)
	}
}
