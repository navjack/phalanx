package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

type soundCaptureManifest struct {
	SoundID                 int              `json:"sound_id"`
	InputScript             string           `json:"input_script"`
	OracleExit              int              `json:"oracle_exit"`
	APUSHA256               string           `json:"apu_sha256"`
	APUSourceHash           string           `json:"apu_upload_source_sha256"`
	ARAMSHA256              string           `json:"aram_sha256"`
	WAVSHA256               string           `json:"wav_sha256"`
	SPCSnapshotHash         string           `json:"spc_snapshot_sha256"`
	SPCSnapshotWAVSHA256    string           `json:"spc_snapshot_wav_sha256"`
	APUSnapshotHash         string           `json:"apu_snapshot_sha256"`
	APUSnapshotReplaySHA256 string           `json:"apu_snapshot_replay_wav_sha256"`
	DSPLogSHA256            string           `json:"dsp_log_sha256"`
	DSPRegsSHA256           string           `json:"dsp_registers_sha256"`
	SPCPCHash               string           `json:"spc_pc_coverage_sha256"`
	SPCExecHash             string           `json:"spc_execution_trace_sha256"`
	SPCPortHash             string           `json:"spc_port_trace_sha256"`
	SPCDataHash             string           `json:"spc_data_trace_sha256"`
	SPCStateHash            string           `json:"spc_state_trace_sha256"`
	TraceSHA256             string           `json:"spc_trace_sha256"`
	NonBulkPairs            []captureCommand `json:"non_bulk_port_pairs"`
	ARAMUploads             []captureUpload  `json:"aram_uploads"`
}

// These fields record observed wire traffic only. Driver semantics such as a
// sequence address are added only after an SPC700 control-flow trace proves
// them; this keeps the capture manifest evidence-first.
type captureCommand struct {
	Frame   int    `json:"frame"`
	Port0   int    `json:"port0"`
	Port1   int    `json:"port1"`
	Port0PC uint32 `json:"port0_cpu_pc"`
	Port1PC uint32 `json:"port1_cpu_pc"`
}

type captureUpload struct {
	Frame   int `json:"frame"`
	Address int `json:"address"`
	Length  int `json:"length"`
}

func summarizeAudio(analysis audioAnalysis) ([]captureCommand, []captureUpload) {
	commands := make([]captureCommand, len(analysis.commands))
	for i, c := range analysis.commands {
		commands[i] = captureCommand{c.frame, c.port0, c.port1, c.port0PC, c.port1PC}
	}
	uploads := make([]captureUpload, len(analysis.chunks))
	for i, c := range analysis.chunks {
		uploads[i] = captureUpload{c.frame, c.address, c.length}
	}
	return commands, uploads
}

func fileSHA256(path string) (string, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	h := sha256.Sum256(b)
	return hex.EncodeToString(h[:]), nil
}

// verifyAPUSnapshotResume proves that the private full-state snapshot resumes
// at the same sample boundary as the native running capture.  Standard .spc
// files intentionally do not make this promise.
func verifyAPUSnapshotResume(referencePath, replayPath string, samples int) error {
	reference, err := os.ReadFile(referencePath)
	if err != nil {
		return err
	}
	replay, err := os.ReadFile(replayPath)
	if err != nil {
		return err
	}
	const wavHeader = 44
	want := wavHeader + samples*4 // 16-bit stereo native S-DSP PCM
	if len(reference) < want || len(replay) != want {
		return fmt.Errorf("unexpected snapshot WAV sizes: reference=%d replay=%d want-at-least=%d", len(reference), len(replay), want)
	}
	if !bytes.Equal(reference[wavHeader:want], replay[wavHeader:]) {
		return fmt.Errorf("full APU snapshot replay PCM differs from native post-snapshot capture")
	}
	return nil
}

// soundCapture runs one clean-reset Sound Test selection. Generated artifacts
// stay in the caller-provided ignored output directory.
func soundCapture(args []string) error {
	fs := flag.NewFlagSet("sound-capture", flag.ContinueOnError)
	oraclePath := fs.String("oracle", "build-macos/phalanx_oracle", "oracle executable")
	romPath := fs.String("rom", "ROMS/Phalanx (USA).sfc", "USA ROM")
	outRoot := fs.String("out", "build-decomp/sound-captures", "ignored output root")
	soundID := fs.Int("id", 0, "Sound Test number (0-255)")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *soundID < 0 || *soundID > 255 {
		return fmt.Errorf("-id must be in 0..255")
	}
	dir := filepath.Join(*outRoot, fmt.Sprintf("%02d", *soundID))
	if err := os.MkdirAll(dir, 0700); err != nil {
		return err
	}
	// The verified route enters SOUND TEST at 00. One debounced Right press
	// advances each subsequent number; the preview attempt follows after a
	// settled frame window.
	parts := []string{"1800-1805:Start", "2200:Down", "2250:Down", "2350:Start"}
	const rightInterval = 250
	for i := 0; i < *soundID; i++ {
		parts = append(parts, fmt.Sprintf("%d:Right", 2450+i*rightInterval))
	}
	preview := 2600 + *soundID*rightInterval
	parts = append(parts, fmt.Sprintf("%d:A", preview))
	script := strings.Join(parts, ",")
	apuPath := filepath.Join(dir, "apu.log")
	apuSourcePath := filepath.Join(dir, "apu-source.log")
	diagPath := filepath.Join(dir, "frames.log")
	wramPath := filepath.Join(dir, "wram.bin")
	aramPath := filepath.Join(dir, "aram.bin")
	wavPath := filepath.Join(dir, "audio.wav")
	spcSnapshotPath := filepath.Join(dir, "preview.spc")
	spcSnapshotWAVPath := filepath.Join(dir, "preview.wav")
	apuSnapshotPath := filepath.Join(dir, "preview.apu")
	apuSnapshotReplayPath := filepath.Join(dir, "preview-resume.wav")
	dspLogPath := filepath.Join(dir, "dsp.log")
	dspRegsPath := filepath.Join(dir, "dsp-registers.bin")
	spcPCPath := filepath.Join(dir, "spc-pc.log")
	spcExecPath := filepath.Join(dir, "spc-exec.log")
	spcPortPath := filepath.Join(dir, "spc-ports.log")
	spcDataPath := filepath.Join(dir, "spc-data.log")
	spcStatePath := filepath.Join(dir, "spc-state.log")
	tracePath := filepath.Join(dir, "spc.trace")
	cmd := exec.Command(*oraclePath, *romPath)
	cmd.Env = append(os.Environ(),
		"PHALANX_ORACLE_FRAMES="+fmt.Sprint(preview+500),
		"PHALANX_ORACLE_PHASE_SCRIPT=title_start=1200-1205",
		"PHALANX_ORACLE_INPUT_SCRIPT="+script,
		"PHALANX_ORACLE_APU_LOG=1",
		"PHALANX_ORACLE_APU_SOURCE_LOG="+apuSourcePath,
		"PHALANX_ORACLE_DIAGNOSTIC_LOG="+diagPath,
		"PHALANX_ORACLE_WRAM_DUMP_FRAME="+fmt.Sprint(preview+400),
		"PHALANX_ORACLE_WRAM_DUMP_PATH="+wramPath,
		"PHALANX_ORACLE_AUDIO_TRACE_CLEAR_FRAME="+fmt.Sprint(preview),
		"PHALANX_ORACLE_SPC_SNAPSHOT="+spcSnapshotPath,
		"PHALANX_ORACLE_SPC_SNAPSHOT_FRAME="+fmt.Sprint(preview+1),
		"PHALANX_ORACLE_SPC_SNAPSHOT_WAV="+spcSnapshotWAVPath,
		"PHALANX_ORACLE_APU_SNAPSHOT="+apuSnapshotPath,
		"PHALANX_ORACLE_DSP_LOG="+dspLogPath,
		"PHALANX_ORACLE_DSP_REGISTERS="+dspRegsPath,
		"PHALANX_ORACLE_SPC_PC_LOG="+spcPCPath,
		"PHALANX_ORACLE_SPC_EXEC_LOG="+spcExecPath,
		"PHALANX_ORACLE_SPC_PORT_LOG="+spcPortPath,
		"PHALANX_ORACLE_SPC_DATA_LOG="+spcDataPath,
		"PHALANX_ORACLE_SPC_STATE_LOG="+spcStatePath,
		"PHALANX_ORACLE_PCM_WAV="+wavPath)
	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	err := cmd.Run()
	_ = os.WriteFile(filepath.Join(dir, "oracle.out"), stdout.Bytes(), 0600)
	var exitCode int
	if err != nil {
		if exitErr, ok := err.(*exec.ExitError); ok {
			exitCode = exitErr.ExitCode()
			if writeErr := os.WriteFile(apuPath, stderr.Bytes(), 0600); writeErr != nil {
				return writeErr
			}
		} else {
			return err
		}
	} else if err := os.WriteFile(apuPath, stderr.Bytes(), 0600); err != nil {
		return err
	}
	writes, err := readAPUWrites(apuPath)
	if err != nil {
		return err
	}
	analysis := analyzeAPUWrites(writes)
	if len(analysis.chunks) == 0 {
		return fmt.Errorf("capture contains no bulk APU upload; Sound Test entry is not proven")
	}
	if len(analysis.commands) == 0 {
		return fmt.Errorf("capture contains no non-bulk APU port traffic; Sound Test preview is not proven")
	}
	if err := os.WriteFile(aramPath, analysis.aram, 0600); err != nil {
		return err
	}
	apuHash, err := fileSHA256(apuPath)
	if err != nil {
		return err
	}
	apuSourceHash, err := fileSHA256(apuSourcePath)
	if err != nil {
		return err
	}
	aramHash, err := fileSHA256(aramPath)
	if err != nil {
		return err
	}
	wavHash, err := fileSHA256(wavPath)
	if err != nil {
		return err
	}
	spcSnapshotHash, err := fileSHA256(spcSnapshotPath)
	if err != nil {
		return err
	}
	spcSnapshotWAVHash, err := fileSHA256(spcSnapshotWAVPath)
	if err != nil {
		return err
	}
	const snapshotReplaySamples = 4096
	replayCmd := exec.Command(*oraclePath, *romPath)
	replayCmd.Env = append(os.Environ(),
		"PHALANX_ORACLE_APU_SNAPSHOT_REPLAY="+apuSnapshotPath,
		"PHALANX_ORACLE_APU_SNAPSHOT_REPLAY_WAV="+apuSnapshotReplayPath,
		"PHALANX_ORACLE_APU_SNAPSHOT_REPLAY_SAMPLES="+fmt.Sprint(snapshotReplaySamples))
	if output, err := replayCmd.CombinedOutput(); err != nil {
		return fmt.Errorf("APU snapshot replay: %w: %s", err, output)
	}
	if err := verifyAPUSnapshotResume(spcSnapshotWAVPath, apuSnapshotReplayPath, snapshotReplaySamples); err != nil {
		return err
	}
	apuSnapshotHash, err := fileSHA256(apuSnapshotPath)
	if err != nil {
		return err
	}
	apuSnapshotReplayHash, err := fileSHA256(apuSnapshotReplayPath)
	if err != nil {
		return err
	}
	dspLogHash, err := fileSHA256(dspLogPath)
	if err != nil {
		return err
	}
	dspRegsHash, err := fileSHA256(dspRegsPath)
	if err != nil {
		return err
	}
	spcPCHash, err := fileSHA256(spcPCPath)
	if err != nil {
		return err
	}
	spcExecHash, err := fileSHA256(spcExecPath)
	if err != nil {
		return err
	}
	spcPortHash, err := fileSHA256(spcPortPath)
	if err != nil {
		return err
	}
	spcDataHash, err := fileSHA256(spcDataPath)
	if err != nil {
		return err
	}
	spcStateHash, err := fileSHA256(spcStatePath)
	if err != nil {
		return err
	}
	traceCmd := exec.Command(*oraclePath, *romPath)
	traceCmd.Env = append(os.Environ(),
		"PHALANX_SPC_TRACE_ARAM="+aramPath,
		"PHALANX_SPC_TRACE_STEPS=20000")
	trace, err := traceCmd.Output()
	if err != nil {
		return fmt.Errorf("SPC trace: %w", err)
	}
	if err := os.WriteFile(tracePath, trace, 0600); err != nil {
		return err
	}
	traceHash, err := fileSHA256(tracePath)
	if err != nil {
		return err
	}
	commands, uploads := summarizeAudio(analysis)
	m := soundCaptureManifest{
		SoundID: *soundID, InputScript: script, OracleExit: exitCode,
		APUSHA256: apuHash, APUSourceHash: apuSourceHash, ARAMSHA256: aramHash, WAVSHA256: wavHash, SPCSnapshotHash: spcSnapshotHash, SPCSnapshotWAVSHA256: spcSnapshotWAVHash, APUSnapshotHash: apuSnapshotHash, APUSnapshotReplaySHA256: apuSnapshotReplayHash,
		DSPLogSHA256: dspLogHash, DSPRegsSHA256: dspRegsHash, SPCPCHash: spcPCHash,
		SPCExecHash: spcExecHash, SPCPortHash: spcPortHash, SPCDataHash: spcDataHash, SPCStateHash: spcStateHash,
		TraceSHA256:  traceHash,
		NonBulkPairs: commands, ARAMUploads: uploads,
	}
	b, err := json.MarshalIndent(m, "", "  ")
	if err != nil {
		return err
	}
	return os.WriteFile(filepath.Join(dir, "manifest.json"), append(b, '\n'), 0600)
}
