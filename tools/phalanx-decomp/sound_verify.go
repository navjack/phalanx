package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"path/filepath"
)

func readCaptureManifest(path string) (soundCaptureManifest, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return soundCaptureManifest{}, err
	}
	var manifest soundCaptureManifest
	if err := json.Unmarshal(b, &manifest); err != nil {
		return soundCaptureManifest{}, err
	}
	return manifest, nil
}

func sameCaptureEvidence(a, b soundCaptureManifest) error {
	if a.OracleExit != 0 || b.OracleExit != 0 {
		return fmt.Errorf("oracle exits are %d and %d", a.OracleExit, b.OracleExit)
	}
	if a.APUSHA256 != b.APUSHA256 {
		return fmt.Errorf("APU log hash differs: %s != %s", a.APUSHA256, b.APUSHA256)
	}
	if a.APUSourceHash != b.APUSourceHash {
		return fmt.Errorf("APU upload-source log hash differs: %s != %s", a.APUSourceHash, b.APUSourceHash)
	}
	if a.ARAMSHA256 != b.ARAMSHA256 {
		return fmt.Errorf("ARAM hash differs: %s != %s", a.ARAMSHA256, b.ARAMSHA256)
	}
	if a.WAVSHA256 != b.WAVSHA256 {
		return fmt.Errorf("PCM/WAV hash differs: %s != %s", a.WAVSHA256, b.WAVSHA256)
	}
	if a.SPCSnapshotHash != b.SPCSnapshotHash {
		return fmt.Errorf("SPC snapshot hash differs: %s != %s", a.SPCSnapshotHash, b.SPCSnapshotHash)
	}
	if a.SPCSnapshotWAVSHA256 != b.SPCSnapshotWAVSHA256 {
		return fmt.Errorf("post-snapshot WAV hash differs: %s != %s", a.SPCSnapshotWAVSHA256, b.SPCSnapshotWAVSHA256)
	}
	if a.APUSnapshotHash != b.APUSnapshotHash {
		return fmt.Errorf("full APU snapshot hash differs: %s != %s", a.APUSnapshotHash, b.APUSnapshotHash)
	}
	if a.APUSnapshotReplaySHA256 != b.APUSnapshotReplaySHA256 {
		return fmt.Errorf("full APU snapshot replay WAV hash differs: %s != %s", a.APUSnapshotReplaySHA256, b.APUSnapshotReplaySHA256)
	}
	if a.DSPLogSHA256 != b.DSPLogSHA256 {
		return fmt.Errorf("DSP event-log hash differs: %s != %s", a.DSPLogSHA256, b.DSPLogSHA256)
	}
	if a.DSPRegsSHA256 != b.DSPRegsSHA256 {
		return fmt.Errorf("DSP register-state hash differs: %s != %s", a.DSPRegsSHA256, b.DSPRegsSHA256)
	}
	if a.SPCPCHash != b.SPCPCHash {
		return fmt.Errorf("SPC PC-coverage hash differs: %s != %s", a.SPCPCHash, b.SPCPCHash)
	}
	if a.SPCExecHash != b.SPCExecHash {
		return fmt.Errorf("SPC execution-trace hash differs: %s != %s", a.SPCExecHash, b.SPCExecHash)
	}
	if a.SPCPortHash != b.SPCPortHash {
		return fmt.Errorf("SPC port-trace hash differs: %s != %s", a.SPCPortHash, b.SPCPortHash)
	}
	if a.SPCDataHash != b.SPCDataHash {
		return fmt.Errorf("SPC data-trace hash differs: %s != %s", a.SPCDataHash, b.SPCDataHash)
	}
	if a.SPCStateHash != b.SPCStateHash {
		return fmt.Errorf("SPC state-trace hash differs: %s != %s", a.SPCStateHash, b.SPCStateHash)
	}
	if a.TraceSHA256 != b.TraceSHA256 {
		return fmt.Errorf("SPC trace hash differs: %s != %s", a.TraceSHA256, b.TraceSHA256)
	}
	return nil
}

// soundVerify makes two entirely fresh captures so determinism is tested
// against clean reset state, not against a prior capture artifact.
func soundVerify(args []string) error {
	fs := flag.NewFlagSet("sound-verify", flag.ContinueOnError)
	oraclePath := fs.String("oracle", "build-macos/phalanx_oracle", "oracle executable")
	romPath := fs.String("rom", "ROMS/Phalanx (USA).sfc", "USA ROM")
	soundID := fs.Int("id", 0, "Sound Test number (0-255)")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *soundID < 0 || *soundID > 255 {
		return fmt.Errorf("-id must be in 0..255")
	}
	root, err := os.MkdirTemp("", "phalanx-sound-verify-")
	if err != nil {
		return err
	}
	defer os.RemoveAll(root)
	for _, name := range []string{"first", "second"} {
		if err := soundCapture([]string{"-oracle", *oraclePath, "-rom", *romPath, "-out", filepath.Join(root, name), "-id", fmt.Sprint(*soundID)}); err != nil {
			return fmt.Errorf("%s capture: %w", name, err)
		}
	}
	first, err := readCaptureManifest(filepath.Join(root, "first", fmt.Sprintf("%02d", *soundID), "manifest.json"))
	if err != nil {
		return err
	}
	second, err := readCaptureManifest(filepath.Join(root, "second", fmt.Sprintf("%02d", *soundID), "manifest.json"))
	if err != nil {
		return err
	}
	if err := sameCaptureEvidence(first, second); err != nil {
		return fmt.Errorf("sound %02d is not deterministic: %w", *soundID, err)
	}
	fmt.Printf("sound %02d: clean-reset APU, ARAM, DSP, portable SPC and full APU-state snapshots, resumed PCM, PC/port/data/state/execution traces, and WAV hashes match\n", *soundID)
	return nil
}
