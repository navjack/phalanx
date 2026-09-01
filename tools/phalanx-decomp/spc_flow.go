package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"sort"
)

type spcEdge struct {
	from, to uint16
	opcode   byte
	count    uint64
}

type spcFlow struct {
	instructions uint64
	pcs          map[uint16]uint64
	edges        map[uint32]spcEdge
}

// readSPCFlow consumes the exact pre-opcode PC/opcode stream recorded during
// the real preview. Consecutive records are therefore observed control-flow
// edges, including calls, returns, and dynamic jumps.
func readSPCFlow(path string) (spcFlow, error) {
	f, err := os.Open(path)
	if err != nil {
		return spcFlow{}, err
	}
	defer f.Close()
	flow := spcFlow{pcs: make(map[uint16]uint64), edges: make(map[uint32]spcEdge)}
	s := bufio.NewScanner(f)
	var previousPC uint16
	var previousOpcode byte
	havePrevious := false
	for s.Scan() {
		line := s.Text()
		if len(line) == 0 || line[0] == '#' {
			continue
		}
		var sequence uint64
		var pc, opcode uint
		if n, _ := fmt.Sscanf(line, "%d %x %x", &sequence, &pc, &opcode); n != 3 || pc > 0xffff || opcode > 0xff {
			return spcFlow{}, fmt.Errorf("parse %q", line)
		}
		currentPC, currentOpcode := uint16(pc), byte(opcode)
		flow.instructions++
		flow.pcs[currentPC]++
		if havePrevious {
			key := uint32(previousPC)<<16 | uint32(currentPC)
			edge := flow.edges[key]
			edge.from, edge.to, edge.opcode = previousPC, currentPC, previousOpcode
			edge.count++
			flow.edges[key] = edge
		}
		previousPC, previousOpcode, havePrevious = currentPC, currentOpcode, true
	}
	if err := s.Err(); err != nil {
		return spcFlow{}, err
	}
	return flow, nil
}

func sortedSPCEdges(edges map[uint32]spcEdge) []spcEdge {
	result := make([]spcEdge, 0, len(edges))
	for _, edge := range edges {
		result = append(result, edge)
	}
	sort.Slice(result, func(i, j int) bool {
		if result[i].count != result[j].count {
			return result[i].count > result[j].count
		}
		if result[i].from != result[j].from {
			return result[i].from < result[j].from
		}
		return result[i].to < result[j].to
	})
	return result
}

func spcFlowCommand(args []string) error {
	fs := flag.NewFlagSet("spc-flow", flag.ContinueOnError)
	trace := fs.String("trace", "build-decomp/sound-captures/01/spc-exec.log", "live SPC execution trace")
	top := fs.Int("top", 80, "number of control-flow edges to print; 0 prints all")
	if err := fs.Parse(args); err != nil {
		return err
	}
	flow, err := readSPCFlow(*trace)
	if err != nil {
		return err
	}
	edges := sortedSPCEdges(flow.edges)
	limit := len(edges)
	if *top > 0 && *top < limit {
		limit = *top
	}
	fmt.Printf("instructions %d executed_pcs %d observed_edges %d\n", flow.instructions, len(flow.pcs), len(edges))
	for _, edge := range edges[:limit] {
		fmt.Printf("%04X %02X -> %04X %d\n", edge.from, edge.opcode, edge.to, edge.count)
	}
	return nil
}
