package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

const expectedSHA256 = "0663330bc061f4b768fa1806610878ef6e6cf546f36041ae087c8e55703693b8"

type Layout struct {
	FormatVersion int       `json:"format_version"`
	Mapping       string    `json:"mapping"`
	ROMSize       int       `json:"rom_size"`
	ROMSHA256     string    `json:"rom_sha256"`
	Segments      []Segment `json:"segments"`
}

type Segment struct {
	Name         string `json:"name"`
	PCStart      int    `json:"pc_start"`
	PCEnd        int    `json:"pc_end"`
	Kind         string `json:"kind"`
	Source       string `json:"source"`
	SPCARAMStart *int   `json:"spc_aram_start,omitempty"`
}

type span struct {
	start int
	end   int
	name  string
}

func usage() {
	fmt.Fprintln(os.Stderr, "usage: phalanx-decomp <report|extract|build|verify|audio|upload-source|spc-image|sequence|sequence-trace|sequence-events|sound-capture|sound-report|sound-verify|spc-flow> [options]")
	fmt.Fprintln(os.Stderr, "  report  audit layout.json and print byte ownership")
	fmt.Fprintln(os.Stderr, "  extract  verify the base ROM and extract opaque ranges")
	fmt.Fprintln(os.Stderr, "  build   assemble a fresh ROM from the layout and verify it")
	fmt.Fprintln(os.Stderr, "  verify  compare a rebuilt ROM against the USA reference")
	fmt.Fprintln(os.Stderr, "  audio   reconstruct SPC ARAM from PHALANX_ORACLE_APU_LOG output")
	fmt.Fprintln(os.Stderr, "  upload-source map live SPC ARAM uploads to their ROM source ranges")
	fmt.Fprintln(os.Stderr, "  spc-image verify/extract the ROM-resident SPC700 driver payload")
	fmt.Fprintln(os.Stderr, "  sequence scan little-endian SPC sequence-header pointers")
	fmt.Fprintln(os.Stderr, "  sequence-trace identify live SPC sequence-stream entry reads")
	fmt.Fprintln(os.Stderr, "  sequence-events infer live control-byte arities and dispatcher targets")
	fmt.Fprintln(os.Stderr, "  sound-capture run one clean-reset Sound Test capture")
	fmt.Fprintln(os.Stderr, "  sound-report summarize sound-capture manifests")
	fmt.Fprintln(os.Stderr, "  sound-verify make two clean-reset captures and compare evidence")
	fmt.Fprintln(os.Stderr, "  spc-flow summarize actual SPC700 preview control flow")
}

func main() {
	if len(os.Args) < 2 {
		usage()
		os.Exit(2)
	}
	var err error
	switch os.Args[1] {
	case "report":
		err = report(os.Args[2:])
	case "extract":
		err = extract(os.Args[2:])
	case "build":
		err = build(os.Args[2:])
	case "verify":
		err = verify(os.Args[2:])
	case "audio":
		err = audio(os.Args[2:])
	case "upload-source":
		err = uploadSource(os.Args[2:])
	case "spc-image":
		err = spcImage(os.Args[2:])
	case "sequence":
		err = sequence(os.Args[2:])
	case "sequence-trace":
		err = sequenceTrace(os.Args[2:])
	case "sequence-events":
		err = sequenceEvents(os.Args[2:])
	case "sound-capture":
		err = soundCapture(os.Args[2:])
	case "sound-report":
		err = soundReport(os.Args[2:])
	case "sound-verify":
		err = soundVerify(os.Args[2:])
	case "spc-flow":
		err = spcFlowCommand(os.Args[2:])
	default:
		usage()
		err = errors.New("unknown command")
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, "phalanx-decomp:", err)
		os.Exit(1)
	}
}

func loadLayout(path string) (Layout, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return Layout{}, err
	}
	var l Layout
	if err := json.Unmarshal(b, &l); err != nil {
		return Layout{}, fmt.Errorf("parse %s: %w", path, err)
	}
	if l.FormatVersion != 1 {
		return Layout{}, fmt.Errorf("unsupported layout format %d", l.FormatVersion)
	}
	if l.Mapping != "lorom" {
		return Layout{}, fmt.Errorf("unsupported mapping %q", l.Mapping)
	}
	if l.ROMSize != 1024*1024 {
		return Layout{}, fmt.Errorf("layout ROM size is %d, want 1048576", l.ROMSize)
	}
	if l.ROMSHA256 != expectedSHA256 {
		return Layout{}, fmt.Errorf("layout hash is %s, want %s", l.ROMSHA256, expectedSHA256)
	}
	if err := validateSegments(l); err != nil {
		return Layout{}, err
	}
	return l, nil
}

func validateSegments(l Layout) error {
	if len(l.Segments) == 0 {
		return errors.New("layout has no segments")
	}
	spans := make([]span, 0, len(l.Segments))
	validKinds := map[string]bool{
		"65816_code": true, "spc700_code": true, "data": true, "unknown": true,
	}
	names := make(map[string]struct{}, len(l.Segments))
	for _, s := range l.Segments {
		if s.Name == "" || s.Source == "" {
			return errors.New("every segment needs a name and source")
		}
		if _, exists := names[s.Name]; exists {
			return fmt.Errorf("duplicate segment name %q", s.Name)
		}
		names[s.Name] = struct{}{}
		if !validKinds[s.Kind] {
			return fmt.Errorf("segment %s has invalid kind %q", s.Name, s.Kind)
		}
		if s.PCStart < 0 || s.PCEnd <= s.PCStart || s.PCEnd > l.ROMSize {
			return fmt.Errorf("segment %s range [%#x,%#x) is outside the ROM", s.Name, s.PCStart, s.PCEnd)
		}
		if s.Kind == "65816_code" || s.Kind == "spc700_code" {
			if s.Source == "base_rom" {
				return fmt.Errorf("executable segment %s cannot use base_rom", s.Name)
			}
		}
		if s.Kind == "spc700_code" && (s.SPCARAMStart == nil || *s.SPCARAMStart < 0 || *s.SPCARAMStart > 0xffff) {
			return fmt.Errorf("SPC700 segment %s needs spc_aram_start in 0..ffff", s.Name)
		}
		if s.Kind != "spc700_code" && s.SPCARAMStart != nil {
			return fmt.Errorf("non-SPC segment %s has spc_aram_start", s.Name)
		}
		spans = append(spans, span{s.PCStart, s.PCEnd, s.Name})
	}
	sort.Slice(spans, func(i, j int) bool { return spans[i].start < spans[j].start })
	if spans[0].start != 0 {
		return fmt.Errorf("layout gap before %#x", spans[0].start)
	}
	end := 0
	for _, s := range spans {
		if s.start < end {
			return fmt.Errorf("layout overlap at %#x near %s", s.start, s.name)
		}
		if s.start > end {
			return fmt.Errorf("layout gap [%#x,%#x)", end, s.start)
		}
		end = s.end
	}
	if end != l.ROMSize {
		return fmt.Errorf("layout ends at %#x, want %#x", end, l.ROMSize)
	}
	return nil
}

func readROM(path string, expectedSize int) ([]byte, string, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return nil, "", err
	}
	if len(b) != expectedSize {
		return nil, "", fmt.Errorf("%s is %d bytes, want %d (headered or wrong ROM)", path, len(b), expectedSize)
	}
	sum := sha256.Sum256(b)
	hash := hex.EncodeToString(sum[:])
	if hash != expectedSHA256 {
		return nil, hash, fmt.Errorf("unsupported ROM hash %s", hash)
	}
	return b, hash, nil
}

func layoutPath(fs *flag.FlagSet) *string {
	return fs.String("layout", "decomp/layout.json", "layout manifest")
}

func report(args []string) error {
	fs := flag.NewFlagSet("report", flag.ContinueOnError)
	layout := layoutPath(fs)
	if err := fs.Parse(args); err != nil {
		return err
	}
	l, err := loadLayout(*layout)
	if err != nil {
		return err
	}
	counts := map[string]int{}
	opaque := 0
	for _, s := range l.Segments {
		bytes := s.PCEnd - s.PCStart
		counts[s.Kind] += bytes
		if s.Source == "base_rom" {
			opaque += bytes
		}
	}
	fmt.Printf("layout: %d segments, %d bytes, hash %s\n", len(l.Segments), l.ROMSize, l.ROMSHA256)
	for _, kind := range []string{"65816_code", "spc700_code", "data", "unknown"} {
		fmt.Printf("  %-11s %8d bytes\n", kind, counts[kind])
	}
	fmt.Printf("  opaque from base ROM: %d bytes\n", opaque)
	return nil
}

func extract(args []string) error {
	fs := flag.NewFlagSet("extract", flag.ContinueOnError)
	layout := layoutPath(fs)
	romPath := fs.String("rom", "ROMS/Phalanx (USA).sfc", "verified USA ROM")
	outDir := fs.String("out", "build-decomp/assets", "ignored extraction directory")
	if err := fs.Parse(args); err != nil {
		return err
	}
	l, err := loadLayout(*layout)
	if err != nil {
		return err
	}
	rom, hash, err := readROM(*romPath, l.ROMSize)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(*outDir, 0o755); err != nil {
		return err
	}
	for _, s := range l.Segments {
		if s.Source != "base_rom" {
			continue
		}
		path := filepath.Join(*outDir, s.Name+".bin")
		if err := os.WriteFile(path, rom[s.PCStart:s.PCEnd], 0o644); err != nil {
			return fmt.Errorf("write %s: %w", path, err)
		}
	}
	fmt.Printf("extracted %d segments from verified ROM %s\n", len(l.Segments), hash)
	return nil
}

func build(args []string) error {
	fs := flag.NewFlagSet("build", flag.ContinueOnError)
	layoutPathValue := layoutPath(fs)
	romPath := fs.String("rom", "ROMS/Phalanx (USA).sfc", "verified USA ROM")
	asarPath := fs.String("asar", "", "path to the pinned Asar executable")
	workDir := fs.String("work", "build-decomp", "ignored build directory")
	outPath := fs.String("out", "", "rebuilt ROM path (defaults inside --work)")
	if err := fs.Parse(args); err != nil {
		return err
	}
	if *asarPath == "" {
		return errors.New("build requires --asar")
	}
	l, err := loadLayout(*layoutPathValue)
	if err != nil {
		return err
	}
	rom, hash, err := readROM(*romPath, l.ROMSize)
	if err != nil {
		return err
	}
	if err := os.MkdirAll(*workDir, 0o755); err != nil {
		return err
	}
	assetsDir := filepath.Join(*workDir, "assets")
	if err := os.MkdirAll(assetsDir, 0o755); err != nil {
		return err
	}
	assetPaths := make(map[string]string)
	for _, s := range l.Segments {
		if s.Source != "base_rom" {
			continue
		}
		path := filepath.Join(assetsDir, s.Name+".bin")
		if err := os.WriteFile(path, rom[s.PCStart:s.PCEnd], 0o644); err != nil {
			return fmt.Errorf("write %s: %w", path, err)
		}
		assetPaths[s.Name] = path
	}
	asmPath := filepath.Join(*workDir, "layout.asm")
	layoutAbs, err := filepath.Abs(*layoutPathValue)
	if err != nil {
		return err
	}
	asm, err := generateAssembly(l, assetPaths, filepath.Dir(filepath.Dir(layoutAbs)))
	if err != nil {
		return err
	}
	if err := os.WriteFile(asmPath, []byte(asm), 0o644); err != nil {
		return err
	}
	if *outPath == "" {
		*outPath = filepath.Join(*workDir, "Phalanx (USA).sfc")
	}
	if err := os.MkdirAll(filepath.Dir(*outPath), 0o755); err != nil {
		return err
	}
	// Asar patches an existing file in place. Truncate this generated target
	// first so omitted bytes can never survive from an earlier build.
	file, err := os.Create(*outPath)
	if err != nil {
		return err
	}
	if err := file.Close(); err != nil {
		return err
	}
	symbolsPath := filepath.Join(*workDir, "Phalanx (USA).sym")
	cmd := execCommand(*asarPath,
		"--symbols=wla", "--symbols-path="+symbolsPath,
		"--fix-checksum=off", asmPath, *outPath)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("asar: %w", err)
	}
	if err := verifyOutput(l, *romPath, *outPath); err != nil {
		return err
	}
	fmt.Printf("built verified ROM from %d layout segments (base %s)\n", len(l.Segments), hash)
	return nil
}

// execCommand is a variable so the build path stays easy to exercise without
// making the layout verifier depend on a shell.
var execCommand = func(name string, args ...string) *exec.Cmd {
	return exec.Command(name, args...)
}

func generateAssembly(l Layout, assets map[string]string, sourceRoot string) (string, error) {
	var b strings.Builder
	b.WriteString("asar 1.91\narch 65816\nlorom\n")
	for _, s := range l.Segments {
		for start := s.PCStart; start < s.PCEnd; {
			bankEnd := ((start / 0x8000) + 1) * 0x8000
			end := s.PCEnd
			if end > bankEnd {
				end = bankEnd
			}
			b.WriteString(fmt.Sprintf("org $%06X\n", pcToSNES(start)))
			if s.Source == "base_rom" {
				asset, ok := assets[s.Name]
				if !ok {
					return "", fmt.Errorf("missing extracted asset for %s", s.Name)
				}
				b.WriteString(fmt.Sprintf("incbin %s:$%X..$%X\n", strconv.Quote(asset), start-s.PCStart, end-s.PCStart))
			} else {
				if start != s.PCStart || end != s.PCEnd {
					return "", fmt.Errorf("assembly segment %s crosses a LoROM bank boundary", s.Name)
				}
				source := s.Source
				if !filepath.IsAbs(source) {
					source = filepath.Join(sourceRoot, source)
				}
				if s.Kind == "spc700_code" {
					// SPC700 uses its own 16-bit ARAM address space. `base` gives
					// branches and labels that address space while the outer `org`
					// continues to select the ROM upload payload byte location.
					b.WriteString("arch spc700\n")
					b.WriteString(fmt.Sprintf("base $%04X\n", *s.SPCARAMStart))
					b.WriteString(fmt.Sprintf("incsrc %s\n", strconv.Quote(source)))
					// SPC `base` makes pc() the ARAM address.  Assert the exact
					// endpoint here, before returning to the ROM architecture, so
					// an overlong source cannot be hidden by the next segment.
					aramEnd := *s.SPCARAMStart + s.PCEnd - s.PCStart
					b.WriteString(fmt.Sprintf("assert pc() == $%04X\n", aramEnd))
					b.WriteString("arch 65816\n")
				} else {
					b.WriteString(fmt.Sprintf("incsrc %s\n", strconv.Quote(source)))
				}
			}
			start = end
		}
	}
	return b.String(), nil
}

func pcToSNES(pc int) int {
	return ((pc/0x8000)&0xff)<<16 | (0x8000 + pc%0x8000)
}

func verifyOutput(l Layout, referencePath, outputPath string) error {
	ref, _, err := readROM(referencePath, l.ROMSize)
	if err != nil {
		return err
	}
	out, err := os.ReadFile(outputPath)
	if err != nil {
		return err
	}
	if len(out) != l.ROMSize {
		return fmt.Errorf("rebuilt ROM is %d bytes, want %d", len(out), l.ROMSize)
	}
	for offset := range out {
		if out[offset] != ref[offset] {
			return fmt.Errorf("first mismatch at PC %#x (SNES %02X:%04X): got %02X want %02X", offset, offset/0x8000, 0x8000+offset%0x8000, out[offset], ref[offset])
		}
	}
	return validateChecksum(out)
}

func validateChecksum(out []byte) error {
	var sum uint32
	for _, b := range out {
		sum += uint32(b)
	}
	checksum := uint16(out[0x7fde]) | uint16(out[0x7fdf])<<8
	complement := uint16(out[0x7fdc]) | uint16(out[0x7fdd])<<8
	if uint16(sum) != checksum || complement+checksum != 0xffff {
		return fmt.Errorf("invalid SNES checksum: sum=%04X header=%04X complement=%04X", uint16(sum), checksum, complement)
	}
	return nil
}

func verify(args []string) error {
	fs := flag.NewFlagSet("verify", flag.ContinueOnError)
	layout := layoutPath(fs)
	romPath := fs.String("rom", "ROMS/Phalanx (USA).sfc", "reference USA ROM")
	outPath := fs.String("out", "build-decomp/Phalanx (USA).sfc", "rebuilt ROM")
	if err := fs.Parse(args); err != nil {
		return err
	}
	l, err := loadLayout(*layout)
	if err != nil {
		return err
	}
	_, _, err = readROM(*romPath, l.ROMSize)
	if err != nil {
		return err
	}
	out, err := os.ReadFile(*outPath)
	if err != nil {
		return err
	}
	if len(out) != l.ROMSize {
		return fmt.Errorf("rebuilt ROM is %d bytes, want %d", len(out), l.ROMSize)
	}
	ref, err := os.ReadFile(*romPath)
	if err != nil {
		return err
	}
	for offset := range out {
		if out[offset] != ref[offset] {
			return fmt.Errorf("first mismatch at PC %#x (SNES %02X:%04X): got %02X want %02X", offset, offset/0x8000, 0x8000+offset%0x8000, out[offset], ref[offset])
		}
	}
	if len(out) < 0x7fe0 {
		return errors.New("rebuilt ROM is missing the SNES header")
	}
	var sum uint32
	for _, b := range out {
		sum += uint32(b)
	}
	checksum := uint16(out[0x7fde]) | uint16(out[0x7fdf])<<8
	complement := uint16(out[0x7fdc]) | uint16(out[0x7fdd])<<8
	if uint16(sum) != checksum || complement+checksum != 0xffff {
		return fmt.Errorf("invalid SNES checksum: sum=%04X header=%04X complement=%04X", uint16(sum), checksum, complement)
	}
	fmt.Printf("verified byte-identical ROM: %d bytes, checksum %04X/%04X\n", len(out), checksum, complement)
	return nil
}
