package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

type dspEvidence struct {
	regWrites int
	konValues map[byte]bool
	sources   map[byte]bool
}

const audioTraceEventDSPRegisterWrite = 1

func readDSPEvidence(logPath, regsPath, aramPath string) (dspEvidence, string, error) {
	f, err := os.Open(logPath)
	if err != nil {
		return dspEvidence{}, "", err
	}
	defer f.Close()
	evidence := dspEvidence{konValues: make(map[byte]bool), sources: make(map[byte]bool)}
	var voiceSrc [8]byte
	s := bufio.NewScanner(f)
	for s.Scan() {
		var index, sample, typ, addr, value, aux, producer int
		if n, _ := fmt.Sscanf(s.Text(), "%d %d %d %x %x %d %d", &index, &sample, &typ, &addr, &value, &aux, &producer); n != 7 {
			continue
		}
		if typ != audioTraceEventDSPRegisterWrite {
			continue
		}
		evidence.regWrites++
		if addr <= 0x74 && addr&0x0f == 0x04 {
			voiceSrc[addr>>4] = byte(value)
		}
		if addr == 0x4c && value != 0 {
			evidence.konValues[byte(value)] = true
			for voice := 0; voice < 8; voice++ {
				if value&(1<<voice) != 0 {
					evidence.sources[voiceSrc[voice]] = true
				}
			}
		}
	}
	if err := s.Err(); err != nil {
		return dspEvidence{}, "", err
	}
	regs, err := os.ReadFile(regsPath)
	if err != nil {
		return dspEvidence{}, "", err
	}
	aram, err := os.ReadFile(aramPath)
	if err != nil {
		return dspEvidence{}, "", err
	}
	if len(regs) != 0x80 || len(aram) != 0x10000 {
		return dspEvidence{}, "", fmt.Errorf("invalid DSP/ARAM artifact size")
	}
	// Include sources already assigned at the end of the playback window. The
	// KON-derived set remains the claim of actual voice starts.
	for voice := 0; voice < 8; voice++ {
		if regs[voice*0x10+4] != 0 {
			evidence.sources[regs[voice*0x10+4]] = true
		}
	}
	dir := int(regs[0x5d]) << 8
	sources := make([]int, 0, len(evidence.sources))
	for src := range evidence.sources {
		sources = append(sources, int(src))
	}
	sort.Ints(sources)
	parts := make([]string, 0, len(sources))
	for _, src := range sources {
		entry := (dir + src*4) & 0xffff
		start := int(aram[entry]) | int(aram[(entry+1)&0xffff])<<8
		loop := int(aram[(entry+2)&0xffff]) | int(aram[(entry+3)&0xffff])<<8
		parts = append(parts, fmt.Sprintf("%02X:$%04X/$%04X", src, start, loop))
	}
	return evidence, strings.Join(parts, ","), nil
}

func formatDSP(e dspEvidence) string {
	if e.regWrites == 0 {
		return "none"
	}
	values := make([]int, 0, len(e.konValues))
	for v := range e.konValues {
		values = append(values, int(v))
	}
	sort.Ints(values)
	parts := make([]string, len(values))
	for i, v := range values {
		parts[i] = fmt.Sprintf("%02X", v)
	}
	return "regs=" + strconv.Itoa(e.regWrites) + " kon=" + strings.Join(parts, ",")
}

func latestUploadSession(uploads []captureUpload) []captureUpload {
	if len(uploads) == 0 {
		return nil
	}
	start := len(uploads) - 1
	// Separate upload generations by a visibly idle frame gap. This is a
	// presentation rule only; the raw manifest preserves every transfer.
	for start > 0 && uploads[start].Frame-uploads[start-1].Frame <= 30 {
		start--
	}
	return uploads[start:]
}

func formatCommand(commands []captureCommand) string {
	if len(commands) == 0 {
		return "none"
	}
	c := commands[len(commands)-1]
	if c.Port0PC != 0 || c.Port1PC != 0 {
		return fmt.Sprintf("%02X/%02X@%d[%06X/%06X]", c.Port0, c.Port1, c.Frame, c.Port0PC, c.Port1PC)
	}
	return fmt.Sprintf("%02X/%02X@%d", c.Port0, c.Port1, c.Frame)
}

func formatUploads(uploads []captureUpload) string {
	if len(uploads) == 0 {
		return "none"
	}
	parts := make([]string, len(uploads))
	for i, u := range uploads {
		parts[i] = fmt.Sprintf("%04X+%d@%d", u.Address, u.Length, u.Frame)
	}
	return strings.Join(parts, ",")
}

func loadCaptureEvidence(dir string, row *soundCaptureManifest) error {
	// Older captures predate the structured manifest. Re-analyze the raw APU
	// log so the report remains based on the original observed traffic.
	if len(row.NonBulkPairs) != 0 || len(row.ARAMUploads) != 0 {
		return nil
	}
	writes, err := readAPUWrites(filepath.Join(dir, "apu.log"))
	if err != nil {
		return err
	}
	commands, uploads := summarizeAudio(analyzeAPUWrites(writes))
	row.NonBulkPairs, row.ARAMUploads = commands, uploads
	return nil
}

func soundReport(args []string) error {
	fs := flag.NewFlagSet("sound-report", flag.ContinueOnError)
	root := fs.String("root", "build-decomp/sound-captures", "capture output root")
	if err := fs.Parse(args); err != nil {
		return err
	}
	entries, err := os.ReadDir(*root)
	if err != nil {
		return err
	}
	var rows []soundCaptureManifest
	for _, entry := range entries {
		if !entry.IsDir() {
			continue
		}
		dir := filepath.Join(*root, entry.Name())
		path := filepath.Join(dir, "manifest.json")
		b, err := os.ReadFile(path)
		if err != nil {
			continue
		}
		var row soundCaptureManifest
		if err := json.Unmarshal(b, &row); err != nil {
			return fmt.Errorf("parse %s: %w", path, err)
		}
		if err := loadCaptureEvidence(dir, &row); err != nil {
			return fmt.Errorf("analyze %s: %w", dir, err)
		}
		rows = append(rows, row)
	}
	sort.Slice(rows, func(i, j int) bool { return rows[i].SoundID < rows[j].SoundID })
	fmt.Println("id exit observed_port_pair last_upload_session sequence_streams sample_brr_sources dsp_signature pcm_sha256")
	for _, row := range rows {
		dir := filepath.Join(*root, fmt.Sprintf("%02d", row.SoundID))
		streams := "unproven"
		if reads, err := readSequenceReads(filepath.Join(dir, "spc-data.log")); err == nil {
			streams = formatSequenceStreams(firstSequenceStreams(reads, sequenceReaderPostPC, 1024))
		}
		dsp, sources, err := readDSPEvidence(filepath.Join(dir, "dsp.log"), filepath.Join(dir, "dsp-registers.bin"), filepath.Join(dir, "aram.bin"))
		if err != nil {
			// Historical capture directories have no live DSP artifact. Do not
			// fabricate a DSP signature from the standalone trace.
			sources = "unavailable"
			dsp = dspEvidence{}
		}
		fmt.Printf("%02d %d %s %s %s %s %s %s\n", row.SoundID, row.OracleExit,
			formatCommand(row.NonBulkPairs), formatUploads(latestUploadSession(row.ARAMUploads)),
			streams, sources, formatDSP(dsp), row.WAVSHA256)
	}
	return nil
}
