package main

import (
	"encoding/binary"
	"os"
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

	// Calculate section offsets
	textOffset := uint64(0x1000)
	rodataOffset := textOffset + uint64(align(len(l.text), 0x1000))
	dataOffset := rodataOffset + uint64(align(len(l.rodata), 0x1000))

	// Calculate file offsets
	textFileOffset := uint64(0x1000)
	rodataFileOffset := textFileOffset + uint64(len(l.text))
	dataFileOffset := rodataFileOffset + uint64(len(l.rodata))

	// Build ELF64 binary
	headerSize := uint64(64)
	phdrSize := uint64(56)
	phdrs := uint64(3)

	fileSize := headerSize + phdrs*phdrSize + uint64(len(l.text)) + uint64(len(l.rodata)) + uint64(len(l.data))
	buf := make([]byte, fileSize)
	offset := uint64(0)

	// ELF Header
	copy(buf[offset:], []byte{0x7f, 'E', 'L', 'F'})
	buf[offset+4] = 2
	buf[offset+5] = 1
	buf[offset+6] = 1
	buf[offset+7] = 0
	offset += 16

	binary.LittleEndian.PutUint16(buf[offset:], 2) // ET_EXEC
	offset += 2
	binary.LittleEndian.PutUint16(buf[offset:], 0x3E) // x86_64
	offset += 2
	binary.LittleEndian.PutUint32(buf[offset:], 1) // version
	offset += 4
	binary.LittleEndian.PutUint64(buf[offset:], textOffset) // entry
	offset += 8
	binary.LittleEndian.PutUint64(buf[offset:], headerSize+phdrs*phdrSize) // phoff
	offset += 8
	binary.LittleEndian.PutUint64(buf[offset:], 0) // shoff
	offset += 8
	binary.LittleEndian.PutUint32(buf[offset:], 0) // flags
	offset += 4
	binary.LittleEndian.PutUint16(buf[offset:], uint16(headerSize)) // ehsize
	offset += 2
	binary.LittleEndian.PutUint16(buf[offset:], uint16(phdrSize)) // phentsize
	offset += 2
	binary.LittleEndian.PutUint16(buf[offset:], uint16(phdrs)) // phnum
	offset += 2
	binary.LittleEndian.PutUint16(buf[offset:], 0) // shentsize
	offset += 2
	binary.LittleEndian.PutUint16(buf[offset:], 0) // shnum
	offset += 2
	binary.LittleEndian.PutUint16(buf[offset:], 0) // shstrndx
	offset += 2

	// Program Header 1: .text (LOAD, RX)
	phdr := buf[offset:]
	binary.LittleEndian.PutUint32(phdr[0:], 1) // PT_LOAD
	binary.LittleEndian.PutUint32(phdr[4:], 5) // PF_R | PF_X
	binary.LittleEndian.PutUint64(phdr[8:], 0) // offset
	binary.LittleEndian.PutUint64(phdr[16:], textOffset) // vaddr
	binary.LittleEndian.PutUint64(phdr[24:], textOffset) // paddr
	binary.LittleEndian.PutUint64(phdr[32:], textFileOffset+uint64(len(l.text))) // filesz
	binary.LittleEndian.PutUint64(phdr[40:], textFileOffset+uint64(len(l.text))) // memsz
	binary.LittleEndian.PutUint64(phdr[48:], 0x1000) // align
	offset += 56

	// Program Header 2: .rodata (LOAD, R)
	phdr = buf[offset:]
	binary.LittleEndian.PutUint32(phdr[0:], 1) // PT_LOAD
	binary.LittleEndian.PutUint32(phdr[4:], 4) // PF_R
	binary.LittleEndian.PutUint64(phdr[8:], rodataFileOffset) // offset
	binary.LittleEndian.PutUint64(phdr[16:], rodataOffset) // vaddr
	binary.LittleEndian.PutUint64(phdr[24:], rodataOffset) // paddr
	binary.LittleEndian.PutUint64(phdr[32:], uint64(len(l.rodata))) // filesz
	binary.LittleEndian.PutUint64(phdr[40:], uint64(len(l.rodata))) // memsz
	binary.LittleEndian.PutUint64(phdr[48:], 0x1000) // align
	offset += 56

	// Program Header 3: .data + .bss (LOAD, RW)
	phdr = buf[offset:]
	binary.LittleEndian.PutUint32(phdr[0:], 1) // PT_LOAD
	binary.LittleEndian.PutUint32(phdr[4:], 6) // PF_R | PF_W
	binary.LittleEndian.PutUint64(phdr[8:], dataFileOffset) // offset
	binary.LittleEndian.PutUint64(phdr[16:], dataOffset) // vaddr
	binary.LittleEndian.PutUint64(phdr[24:], dataOffset) // paddr
	binary.LittleEndian.PutUint64(phdr[32:], uint64(len(l.data))) // filesz
	binary.LittleEndian.PutUint64(phdr[40:], uint64(len(l.data))+uint64(len(l.bss))) // memsz
	binary.LittleEndian.PutUint64(phdr[48:], 0x1000) // align
	offset += 56

	// Write .text
	copy(buf[offset:], l.text)
	offset += uint64(len(l.text))

	// Write .rodata
	copy(buf[offset:], l.rodata)
	offset += uint64(len(l.rodata))

	// Write .data
	copy(buf[offset:], l.data)

	return os.WriteFile(outputPath, buf, 0755)
}

func align(size, alignment int) int {
	return (size + alignment - 1) & ^(alignment - 1)
}
