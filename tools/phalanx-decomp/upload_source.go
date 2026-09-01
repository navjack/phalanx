package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"sort"
)

const (
	uploaderLengthPC = 0x1f8aa1
	uploaderTargetPC = 0x1f8aa6
	uploaderFirstPC  = 0x1f8a78
	uploaderLoopPC   = 0x1f8a81
)

type uploadSourceRead struct {
	frame   int
	pc      uint32
	address uint32
	value   byte
}

type uploadSourceBlock struct {
	frame, target, length int
	firstSource           uint32
}

func readUploadSource(path string) ([]uploadSourceRead, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	var result []uploadSourceRead
	s := bufio.NewScanner(f)
	for s.Scan() {
		var r uploadSourceRead
		var value uint
		if _, err := fmt.Sscanf(s.Text(), "%d %x %x %x", &r.frame, &r.pc, &r.address, &value); err != nil || value > 0xff {
			return nil, fmt.Errorf("parse %q", s.Text())
		}
		r.value = byte(value)
		result = append(result, r)
	}
	return result, s.Err()
}

// inferUploadSourceBlocks reads the actual indirect-long source reads from
// the uploader. Header reads give byte length and destination; the first data
// read gives the ROM source range. No static pointer-table guess is involved.
func inferUploadSourceBlocks(reads []uploadSourceRead) []uploadSourceBlock {
	var result []uploadSourceBlock
	for i := 0; i+3 < len(reads); i++ {
		if reads[i].pc != uploaderLengthPC || reads[i+1].pc != uploaderLengthPC ||
			reads[i+2].pc != uploaderTargetPC || reads[i+3].pc != uploaderTargetPC {
			continue
		}
		length := int(reads[i].value) | int(reads[i+1].value)<<8
		target := int(reads[i+2].value) | int(reads[i+3].value)<<8
		if length == 0 {
			continue
		}
		for j := i + 4; j < len(reads); j++ {
			if reads[j].pc != uploaderFirstPC && reads[j].pc != uploaderLoopPC {
				continue
			}
			result = append(result, uploadSourceBlock{reads[i].frame, target, length, reads[j].address})
			break
		}
	}
	return result
}

func uploadSource(args []string) error {
	fs := flag.NewFlagSet("upload-source", flag.ContinueOnError)
	path := fs.String("log", "", "APU upload-source log from sound-capture")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *path == "" {
		return fmt.Errorf("-log is required")
	}
	reads, err := readUploadSource(*path)
	if err != nil {
		return err
	}
	blocks := inferUploadSourceBlocks(reads)
	sort.SliceStable(blocks, func(i, j int) bool { return blocks[i].frame < blocks[j].frame })
	fmt.Println("frame rom_source aram_range bytes")
	for _, b := range blocks {
		fmt.Printf("%d %06X %04X-%04X %d\n", b.frame, b.firstSource, b.target,
			(b.target+b.length-1)&0xffff, b.length)
	}
	return nil
}
