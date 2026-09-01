package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"strconv"
)

type apuWrite struct {
	frame, port, value int
	cpuPC              uint32
}
type audioChunk struct{ address, length, frame int }

// soundCommand is a non-bulk CPU-to-SPC port 0/1 pair. It is deliberately
// named for the bus protocol, rather than calling it a song command: the
// meaning belongs to the traced SPC700 driver and has not yet been proven.
type soundCommand struct {
	frame, port0, port1 int
	port0PC, port1PC    uint32
}

type audioAnalysis struct {
	aram     []byte
	chunks   []audioChunk
	commands []soundCommand
}

func readAPUWrites(path string) ([]apuWrite, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	var writes []apuWrite
	s := bufio.NewScanner(f)
	for s.Scan() {
		var w apuWrite
		var value, source string
		n, _ := fmt.Sscanf(s.Text(), "%d %d %s %s", &w.frame, &w.port, &value, &source)
		if n < 3 {
			// Oracle status and diagnostic output may share stderr with the
			// gated APU log. Numeric-looking rows must still parse strictly.
			if len(s.Text()) == 0 || (s.Text()[0] < '0' || s.Text()[0] > '9') {
				continue
			}
			return nil, fmt.Errorf("parse %q: %w", s.Text(), err)
		}
		if w.port < 0 || w.port > 3 {
			return nil, fmt.Errorf("invalid APU port %d", w.port)
		}
		v, err := strconv.ParseUint(value, 16, 8)
		if err != nil {
			return nil, fmt.Errorf("parse value %q: %w", value, err)
		}
		w.value = int(v)
		if n == 4 {
			pc, err := strconv.ParseUint(source, 16, 24)
			if err != nil {
				return nil, fmt.Errorf("parse source PC %q: %w", source, err)
			}
			w.cpuPC = uint32(pc)
		}
		writes = append(writes, w)
	}
	if err := s.Err(); err != nil {
		return nil, err
	}
	return writes, nil
}

func isAddressPair(writes []apuWrite, i int) bool {
	return i+1 < len(writes) && writes[i].port == 2 && writes[i+1].port == 3
}

func isDataPair(writes []apuWrite, i int) bool {
	return i+1 < len(writes) && writes[i].port == 0 && writes[i+1].port == 1
}

// analyzeAPUWrites reconstructs exactly the payloads carried by the observed
// IPL-style transfer sessions. A session with 16 or fewer pairs remains short
// bus traffic: it cannot be silently mistaken for an ARAM upload.
func analyzeAPUWrites(writes []apuWrite) audioAnalysis {
	result := audioAnalysis{aram: make([]byte, 0x10000)}
	bulk := make([]bool, len(writes))
	for i := 0; i < len(writes); {
		if !isAddressPair(writes, i) {
			i++
			continue
		}
		j, n := i+2, 0
		for j < len(writes) && !isAddressPair(writes, j) {
			if isDataPair(writes, j) {
				n++
				j += 2
			} else {
				j++
			}
		}
		if n > 16 {
			for k := i + 2; k < j; k++ {
				bulk[k] = true
			}
		}
		i = j
	}
	for i := 0; i+1 < len(writes); i++ {
		if isDataPair(writes, i) && !bulk[i] && !bulk[i+1] {
			result.commands = append(result.commands, soundCommand{
				writes[i].frame, writes[i].value, writes[i+1].value,
				writes[i].cpuPC, writes[i+1].cpuPC,
			})
		}
	}
	for i := 0; i < len(writes); {
		if !isAddressPair(writes, i) {
			i++
			continue
		}
		address := writes[i].value | writes[i+1].value<<8
		j := i + 2
		values := make([]byte, 0, 16)
		for j < len(writes) && !isAddressPair(writes, j) {
			if isDataPair(writes, j) {
				values = append(values, byte(writes[j+1].value))
				j += 2
			} else {
				j++
			}
		}
		if len(values) > 16 {
			for k, value := range values {
				result.aram[(address+k)&0xffff] = value
			}
			result.chunks = append(result.chunks, audioChunk{address, len(values), writes[i].frame})
		}
		i = j
	}
	return result
}

// audio reconstructs only bytes transferred by the SPC IPL-style bulk
// protocol. Short port writes are reported separately and are never treated
// as ARAM data.
func audio(args []string) error {
	fs := flag.NewFlagSet("audio", flag.ContinueOnError)
	logPath := fs.String("log", "", "APU log produced by PHALANX_ORACLE_APU_LOG=1")
	aramPath := fs.String("aram", "", "optional ignored 64 KiB ARAM output path")
	commands := fs.Bool("commands", false, "list non-bulk CPU-to-SPC port-pair traffic")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *logPath == "" {
		return fmt.Errorf("-log is required")
	}
	writes, err := readAPUWrites(*logPath)
	if err != nil {
		return err
	}
	analysis := analyzeAPUWrites(writes)
	if *commands {
		for _, c := range analysis.commands {
			fmt.Printf("port-pair frame %d: %02X %02X\n", c.frame, c.port0, c.port1)
		}
	}
	for _, c := range analysis.chunks {
		fmt.Printf("$%04X-$%04X %d bytes frame %d\n", c.address, (c.address+c.length-1)&0xffff, c.length, c.frame)
	}
	if *aramPath != "" {
		if err := os.WriteFile(*aramPath, analysis.aram, 0600); err != nil {
			return err
		}
		fmt.Printf("wrote 65536-byte ARAM image to %s\n", *aramPath)
	}
	return nil
}
