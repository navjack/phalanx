package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"
)

// The $13CB F7 instruction is the verified stream-byte load; its trace record
// carries the post-operand PC $13CD. It is deliberately a trace identity, not
// a speculative bytecode interpretation.
const sequenceReaderPostPC = 0x13cd

type sequenceRead struct {
	instruction uint64
	pc          uint16
	address     uint16
	value       byte
}

func readSequenceReads(path string) ([]sequenceRead, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	var reads []sequenceRead
	s := bufio.NewScanner(f)
	for s.Scan() {
		line := s.Text()
		if len(line) == 0 || line[0] == '#' {
			continue
		}
		var record uint64
		var instruction uint64
		var pc, address, value uint
		if n, _ := fmt.Sscanf(line, "%d %d %x %x %x", &record, &instruction, &pc, &address, &value); n != 5 || pc > 0xffff || address > 0xffff || value > 0xff {
			return nil, fmt.Errorf("parse %q", line)
		}
		reads = append(reads, sequenceRead{instruction, uint16(pc), uint16(address), byte(value)})
	}
	if err := s.Err(); err != nil {
		return nil, err
	}
	return reads, nil
}

// firstSequenceStreams returns the first decoder-read group after selection.
// The driver visits active voices in one short service burst; an instruction
// gap terminates that group. Its addresses are observed stream entry bytes.
func firstSequenceStreams(reads []sequenceRead, readerPC uint16, gap uint64) []sequenceRead {
	var result []sequenceRead
	var previous uint64
	for _, read := range reads {
		if read.pc != readerPC {
			continue
		}
		if len(result) != 0 && read.instruction-previous > gap {
			break
		}
		result = append(result, read)
		previous = read.instruction
	}
	return result
}

func formatSequenceStreams(reads []sequenceRead) string {
	if len(reads) == 0 {
		return "unproven"
	}
	parts := make([]string, len(reads))
	for i, read := range reads {
		parts[i] = fmt.Sprintf("%04X", read.address)
	}
	return strings.Join(parts, ",")
}

func parseHex16(s string) (uint16, error) {
	v, err := strconv.ParseUint(strings.TrimPrefix(strings.TrimPrefix(s, "0x"), "0X"), 16, 16)
	return uint16(v), err
}

func sequenceTrace(args []string) error {
	fs := flag.NewFlagSet("sequence-trace", flag.ContinueOnError)
	path := fs.String("log", "", "SPC data-read log from sound-capture")
	reader := fs.String("reader", "13cd", "post-operand PC of sequence-byte reader")
	gap := fs.Uint64("gap", 1024, "instruction gap separating voice-service bursts")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *path == "" {
		return fmt.Errorf("-log is required")
	}
	pc, err := parseHex16(*reader)
	if err != nil {
		return err
	}
	reads, err := readSequenceReads(*path)
	if err != nil {
		return err
	}
	streams := firstSequenceStreams(reads, pc, *gap)
	fmt.Printf("reader_post_pc %04X first_stream_group %s\n", pc, formatSequenceStreams(streams))
	for _, stream := range streams {
		fmt.Printf("%04X %02X instruction %d\n", stream.address, stream.value, stream.instruction)
	}
	return nil
}
