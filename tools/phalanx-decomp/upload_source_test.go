package main

import "testing"

func TestInferUploadSourceBlocksUsesLiveHeaderAndFirstPayloadRead(t *testing.T) {
	reads := []uploadSourceRead{
		{frame: 4, pc: uploaderLengthPC, address: 0x1e8000, value: 0x04},
		{frame: 4, pc: uploaderLengthPC, address: 0x1e8001, value: 0x00},
		{frame: 4, pc: uploaderTargetPC, address: 0x1e8002, value: 0x00},
		{frame: 4, pc: uploaderTargetPC, address: 0x1e8003, value: 0x08},
		{frame: 4, pc: uploaderFirstPC, address: 0x1e8004, value: 0x20},
	}
	blocks := inferUploadSourceBlocks(reads)
	if len(blocks) != 1 {
		t.Fatalf("blocks = %d", len(blocks))
	}
	if got := blocks[0]; got.target != 0x0800 || got.length != 4 || got.firstSource != 0x1e8004 {
		t.Fatalf("block = %#v", got)
	}
}
