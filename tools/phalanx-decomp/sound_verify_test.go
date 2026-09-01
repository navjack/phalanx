package main

import "testing"

func TestSameCaptureEvidenceChecksAllExactArtifacts(t *testing.T) {
	a := soundCaptureManifest{OracleExit: 0, APUSHA256: "a", APUSourceHash: "source", ARAMSHA256: "b", WAVSHA256: "c", SPCSnapshotHash: "snapshot", SPCSnapshotWAVSHA256: "snapshot-wav", APUSnapshotHash: "apu-state", APUSnapshotReplaySHA256: "apu-resume", DSPLogSHA256: "dsp", DSPRegsSHA256: "regs", SPCPCHash: "pc", SPCExecHash: "exec", SPCPortHash: "port", SPCDataHash: "data", SPCStateHash: "state", TraceSHA256: "d"}
	if err := sameCaptureEvidence(a, a); err != nil {
		t.Fatal(err)
	}
	b := a
	b.WAVSHA256 = "changed"
	if err := sameCaptureEvidence(a, b); err == nil {
		t.Fatal("WAV mismatch was accepted")
	}
	b = a
	b.SPCStateHash = "changed"
	if err := sameCaptureEvidence(a, b); err == nil {
		t.Fatal("SPC state-trace mismatch was accepted")
	}
	b = a
	b.APUSnapshotReplaySHA256 = "changed"
	if err := sameCaptureEvidence(a, b); err == nil {
		t.Fatal("APU snapshot replay mismatch was accepted")
	}
}
