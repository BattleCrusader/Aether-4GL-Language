package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
)

type Linker struct {
	text   []byte
	data   []byte
	rodata []byte
	bss    []byte
	labels map[string]int
}

func NewLinker() *Linker {
	return &Linker{
		labels: make(map[string]int),
	}
}

func (l *Linker) Link(text, data, rodata, bss []byte, codegenLabels map[string]int, outputPath string) error {
	l.text = text
	l.data = data
	l.rodata = rodata
	l.bss = bss

	for k, v := range codegenLabels {
		l.labels[k] = v
	}

	// Create temp directory for intermediate files
	tmpDir, err := os.MkdirTemp("", "aether-link-*")
	if err != nil {
		return fmt.Errorf("failed to create temp dir: %w", err)
	}
	defer os.RemoveAll(tmpDir)

	// Step 1: Write generated machine code to a temp .bin file
	// Concatenate text + rodata + data sections into one binary blob.
	binPath := filepath.Join(tmpDir, "aether_code.bin")
	code := make([]byte, 0, len(l.text)+len(l.rodata)+len(l.data))
	code = append(code, l.text...)
	code = append(code, l.rodata...)
	code = append(code, l.data...)
	if err := os.WriteFile(binPath, code, 0644); err != nil {
		return fmt.Errorf("failed to write code binary: %w", err)
	}

	// Step 2: Create an assembly wrapper with .incbin
	asmPath := filepath.Join(tmpDir, "aether.s")
	asmContent := fmt.Sprintf(`.text
.globl _main
_main:
.incbin "%s"
`, binPath)
	if err := os.WriteFile(asmPath, []byte(asmContent), 0644); err != nil {
		return fmt.Errorf("failed to write assembly wrapper: %w", err)
	}

	// Step 4: Assemble with `as`
	objPath := filepath.Join(tmpDir, "aether.o")
	cmdAs := exec.Command("as", "-arch", "x86_64", "-o", objPath, asmPath)
	cmdAs.Stderr = os.Stderr
	if err := cmdAs.Run(); err != nil {
		return fmt.Errorf("assembly failed: %w", err)
	}

	// Step 5: Link with `ld`
	sdkPathBytes, err := exec.Command("xcrun", "--show-sdk-path").Output()
	if err != nil {
		return fmt.Errorf("failed to get SDK path: %w", err)
	}
	sdkPath := string(sdkPathBytes)
	if len(sdkPath) > 0 && sdkPath[len(sdkPath)-1] == '\n' {
		sdkPath = sdkPath[:len(sdkPath)-1]
	}

	cmdLd := exec.Command("ld",
		"-o", outputPath,
		objPath,
		"-lSystem",
		"-syslibroot", sdkPath,
		"-arch", "x86_64",
		"-segprot", "__TEXT", "rwx", "rwx",
	)
	cmdLd.Stderr = os.Stderr
	if err := cmdLd.Run(); err != nil {
		return fmt.Errorf("linking failed: %w", err)
	}

	return nil
}
