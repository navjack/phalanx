package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestReadSPCFlowPreservesObservedEdges(t *testing.T) {
	path := filepath.Join(t.TempDir(), "spc-exec.log")
	data := "# instruction_count 4 retained_from 0\n0 0800 20\n1 0801 CD\n2 0810 3F\n3 0800 20\n"
	if err := os.WriteFile(path, []byte(data), 0600); err != nil {
		t.Fatal(err)
	}
	flow, err := readSPCFlow(path)
	if err != nil {
		t.Fatal(err)
	}
	if flow.instructions != 4 || len(flow.pcs) != 3 || len(flow.edges) != 3 {
		t.Fatalf("flow = %#v", flow)
	}
	edge := flow.edges[uint32(0x0810)<<16|0x0800]
	if edge.opcode != 0x3f || edge.count != 1 {
		t.Fatalf("return edge = %#v", edge)
	}
}
