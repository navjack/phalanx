package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"sort"
)

type spcExecutionState struct {
	pc     uint16
	opcode byte
	a      byte
	y      byte
}

func readSPCExecutionStates(path string) ([]spcExecutionState, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	var result []spcExecutionState
	s := bufio.NewScanner(f)
	for s.Scan() {
		line := s.Text()
		if len(line) == 0 || line[0] == '#' {
			continue
		}
		var index uint64
		var pc, opcode, a, x, y, sp, flags uint
		n, _ := fmt.Sscanf(line, "%d %x %x %x %x %x %x %x", &index, &pc, &opcode, &a, &x, &y, &sp, &flags)
		if n < 3 || pc > 0xffff || opcode > 0xff {
			return nil, fmt.Errorf("parse %q", line)
		}
		if index != uint64(len(result)) {
			return nil, fmt.Errorf("execution trace has retained offset at %d", index)
		}
		state := spcExecutionState{pc: uint16(pc), opcode: byte(opcode)}
		if n >= 4 {
			state.a = byte(a)
		}
		if n >= 6 {
			state.y = byte(y)
		}
		result = append(result, state)
	}
	if err := s.Err(); err != nil {
		return nil, err
	}
	return result, nil
}

type sequenceCommandAdvance struct {
	value    byte
	advances map[byte]uint64
}

func inferSequenceControlHandlers(states []spcExecutionState) map[byte]map[uint16]uint64 {
	result := make(map[byte]map[uint16]uint64)
	for i, state := range states {
		if state.pc != 0x0b19 || state.opcode != 0x68 || state.a < 0xe0 {
			continue
		}
		for j := i + 1; j+1 < len(states) && j < i+256; j++ {
			if states[j].pc != 0x1037 || states[j].opcode != 0x6f {
				continue
			}
			handler := states[j+1].pc
			if result[state.a] == nil {
				result[state.a] = make(map[uint16]uint64)
			}
			result[state.a][handler]++
			break
		}
	}
	return result
}

func inferSequenceCommandAdvances(reads []sequenceRead, states []spcExecutionState) map[byte]sequenceCommandAdvance {
	result := make(map[byte]sequenceCommandAdvance)
	for _, read := range reads {
		if read.pc != sequenceReaderPostPC || read.value < 0xe0 || read.instruction >= uint64(len(states)) {
			continue
		}
		current := states[read.instruction]
		if current.pc != 0x13cb || current.opcode != 0xf7 {
			continue
		}
		for i := read.instruction + 1; i < uint64(len(states)) && i < read.instruction+64; i++ {
			next := states[i]
			if next.pc != 0x13cb || next.opcode != 0xf7 {
				continue
			}
			a := result[read.value]
			if a.advances == nil {
				a = sequenceCommandAdvance{value: read.value, advances: make(map[byte]uint64)}
			}
			a.advances[byte(uint16(next.y-current.y))]++
			result[read.value] = a
			break
		}
	}
	return result
}

func sequenceEvents(args []string) error {
	fs := flag.NewFlagSet("sequence-events", flag.ContinueOnError)
	dataPath := fs.String("data", "", "SPC data-read log from sound-capture")
	execPath := fs.String("exec", "", "SPC execution trace from sound-capture")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *dataPath == "" || *execPath == "" {
		return fmt.Errorf("-data and -exec are required")
	}
	reads, err := readSequenceReads(*dataPath)
	if err != nil {
		return err
	}
	states, err := readSPCExecutionStates(*execPath)
	if err != nil {
		return err
	}
	advances := inferSequenceCommandAdvances(reads, states)
	keys := make([]int, 0, len(advances))
	for value := range advances {
		keys = append(keys, int(value))
	}
	sort.Ints(keys)
	for _, value := range keys {
		a := advances[byte(value)]
		advanceKeys := make([]int, 0, len(a.advances))
		for advance := range a.advances {
			advanceKeys = append(advanceKeys, int(advance))
		}
		sort.Ints(advanceKeys)
		for _, advance := range advanceKeys {
			// A small forward offset is a local opcode+operand skip. Zero or a
			// wrap-sized delta means the stream rebased/jumped between reads.
			if advance == 0 || advance > 16 {
				fmt.Printf("%02X advance=nonlinear operands=unproven observations=%d\n", a.value, a.advances[byte(advance)])
			} else {
				fmt.Printf("%02X advance=%d operands=%d observations=%d\n", a.value, advance, advance-1, a.advances[byte(advance)])
			}
		}
	}
	handlers := inferSequenceControlHandlers(states)
	handlerKeys := make([]int, 0, len(handlers))
	for value := range handlers {
		handlerKeys = append(handlerKeys, int(value))
	}
	sort.Ints(handlerKeys)
	for _, value := range handlerKeys {
		entries := make([]int, 0, len(handlers[byte(value)]))
		for handler := range handlers[byte(value)] {
			entries = append(entries, int(handler))
		}
		sort.Ints(entries)
		for _, handler := range entries {
			fmt.Printf("%02X handler=%04X observations=%d\n", value, handler, handlers[byte(value)][uint16(handler)])
		}
	}
	return nil
}
