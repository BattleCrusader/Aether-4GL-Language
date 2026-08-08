package main

import "os"

//go:noinline
func appendWriteFileCode(text []byte) ([]byte, int) {
	// Read from file at runtime — the optimizer cannot reorder across
	// a real I/O call with observable side effects.
	code, err := os.ReadFile("cmd/bootstrap/writefile.bin")
	if err != nil {
		panic("failed to read writefile.bin: " + err.Error())
	}
	text = append(text, make([]byte, len(code))...)
	n := len(text) - len(code)
	copy(text[n:], code)
	return text, n
}
