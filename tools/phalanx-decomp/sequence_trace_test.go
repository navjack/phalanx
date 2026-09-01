package main

import "testing"

func TestFirstSequenceStreamsStopsAtServiceGap(t *testing.T) {
	reads := []sequenceRead{
		{100, sequenceReaderPostPC, 0x1792, 0xb0},
		{240, sequenceReaderPostPC, 0x17a3, 0xad},
		{1300, sequenceReaderPostPC, 0x1793, 0xb1},
	}
	streams := firstSequenceStreams(reads, sequenceReaderPostPC, 1024)
	if got := formatSequenceStreams(streams); got != "1792,17A3" {
		t.Fatalf("streams = %s", got)
	}
}
