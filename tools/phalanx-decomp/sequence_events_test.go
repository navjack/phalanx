package main

import "testing"

func TestInferSequenceCommandAdvances(t *testing.T) {
	states := make([]spcExecutionState, 8)
	states[1] = spcExecutionState{pc: 0x13cb, opcode: 0xf7, y: 4}
	states[4] = spcExecutionState{pc: 0x13cb, opcode: 0xf7, y: 8}
	reads := []sequenceRead{{instruction: 1, pc: sequenceReaderPostPC, address: 0x1704, value: 0xe3}}
	advances := inferSequenceCommandAdvances(reads, states)
	if got := advances[0xe3].advances[4]; got != 1 {
		t.Fatalf("advance count = %d", got)
	}
}

func TestInferSequenceControlHandlers(t *testing.T) {
	states := []spcExecutionState{
		{pc: 0x0b19, opcode: 0x68, a: 0xe3},
		{pc: 0x101c, opcode: 0x1c},
		{pc: 0x1037, opcode: 0x6f},
		{pc: 0x10b8, opcode: 0xd5},
	}
	if got := inferSequenceControlHandlers(states)[0xe3][0x10b8]; got != 1 {
		t.Fatalf("handler observation count = %d", got)
	}
}
