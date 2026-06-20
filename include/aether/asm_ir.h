#ifndef AETHER_ASM_IR_H
#define AETHER_ASM_IR_H

#include <stdint.h>
#include <stddef.h>

/* ================================================================
 * Aether Multi-Target Assembler — Intermediate Representation
 *
 * Parses NASM-syntax assembly into an architecture-neutral IR,
 * then backends emit target-specific assembly text.
 * ================================================================ */

/* Maximum sizes */
#define ASM_MAX_OPERANDS 6
#define ASM_MAX_INSTRUCTIONS 4096
#define ASM_MAX_LABEL_LEN 256
#define ASM_MAX_MNEMONIC_LEN 32

/* --- Register enum (architecture-neutral) --- */
typedef enum {
    /* General purpose — x86_64 naming */
    ASM_REG_RAX, ASM_REG_RBX, ASM_REG_RCX, ASM_REG_RDX,
    ASM_REG_RSI, ASM_REG_RDI, ASM_REG_RBP, ASM_REG_RSP,
    ASM_REG_R8,  ASM_REG_R9,  ASM_REG_R10, ASM_REG_R11,
    ASM_REG_R12, ASM_REG_R13, ASM_REG_R14, ASM_REG_R15,

    /* 32-bit sub-registers */
    ASM_REG_EAX, ASM_REG_EBX, ASM_REG_ECX, ASM_REG_EDX,
    ASM_REG_ESI, ASM_REG_EDI, ASM_REG_EBP, ASM_REG_ESP,
    ASM_REG_R8D, ASM_REG_R9D, ASM_REG_R10D, ASM_REG_R11D,
    ASM_REG_R12D, ASM_REG_R13D, ASM_REG_R14D, ASM_REG_R15D,

    /* 16-bit sub-registers */
    ASM_REG_AX, ASM_REG_BX, ASM_REG_CX, ASM_REG_DX,
    ASM_REG_SI, ASM_REG_DI, ASM_REG_BP, ASM_REG_SP,
    ASM_REG_R8W, ASM_REG_R9W, ASM_REG_R10W, ASM_REG_R11W,
    ASM_REG_R12W, ASM_REG_R13W, ASM_REG_R14W, ASM_REG_R15W,

    /* 8-bit sub-registers */
    ASM_REG_AL, ASM_REG_BL, ASM_REG_CL, ASM_REG_DL,
    ASM_REG_AH, ASM_REG_BH, ASM_REG_CH, ASM_REG_DH,
    ASM_REG_SIL, ASM_REG_DIL, ASM_REG_BPL, ASM_REG_SPL,
    ASM_REG_R8B, ASM_REG_R9B, ASM_REG_R10B, ASM_REG_R11B,
    ASM_REG_R12B, ASM_REG_R13B, ASM_REG_R14B, ASM_REG_R15B,

    /* SIMD registers */
    ASM_REG_XMM0,  ASM_REG_XMM1,  ASM_REG_XMM2,  ASM_REG_XMM3,
    ASM_REG_XMM4,  ASM_REG_XMM5,  ASM_REG_XMM6,  ASM_REG_XMM7,
    ASM_REG_XMM8,  ASM_REG_XMM9,  ASM_REG_XMM10, ASM_REG_XMM11,
    ASM_REG_XMM12, ASM_REG_XMM13, ASM_REG_XMM14, ASM_REG_XMM15,

    /* Segment registers */
    ASM_REG_CS, ASM_REG_DS, ASM_REG_ES, ASM_REG_FS, ASM_REG_GS, ASM_REG_SS,

    /* ARM64 special registers */
    ASM_REG_X0,  ASM_REG_X1,  ASM_REG_X2,  ASM_REG_X3,
    ASM_REG_X4,  ASM_REG_X5,  ASM_REG_X6,  ASM_REG_X7,
    ASM_REG_X8,  ASM_REG_X9,  ASM_REG_X10, ASM_REG_X11,
    ASM_REG_X12, ASM_REG_X13, ASM_REG_X14, ASM_REG_X15,
    ASM_REG_X16, ASM_REG_X17, ASM_REG_X18, ASM_REG_X19,
    ASM_REG_X20, ASM_REG_X21, ASM_REG_X22, ASM_REG_X23,
    ASM_REG_X24, ASM_REG_X25, ASM_REG_X26, ASM_REG_X27,
    ASM_REG_X28, ASM_REG_X29, ASM_REG_X30, ASM_REG_XZR,

    /* RISC-V registers */
    ASM_REG_RV_ZERO, ASM_REG_RV_RA,  ASM_REG_RV_SP,  ASM_REG_RV_GP,
    ASM_REG_RV_TP,   ASM_REG_RV_T0,  ASM_REG_RV_T1,  ASM_REG_RV_T2,
    ASM_REG_RV_S0,   ASM_REG_RV_S1,  ASM_REG_RV_S2,  ASM_REG_RV_S3,
    ASM_REG_RV_S4,   ASM_REG_RV_S5,  ASM_REG_RV_S6,  ASM_REG_RV_S7,
    ASM_REG_RV_S8,   ASM_REG_RV_S9,  ASM_REG_RV_S10, ASM_REG_RV_S11,
    ASM_REG_RV_A0,   ASM_REG_RV_A1,  ASM_REG_RV_A2,  ASM_REG_RV_A3,
    ASM_REG_RV_A4,   ASM_REG_RV_A5,  ASM_REG_RV_A6,  ASM_REG_RV_A7,
    ASM_REG_RV_T3,   ASM_REG_RV_T4,  ASM_REG_RV_T5,  ASM_REG_RV_T6,

    ASM_REG_COUNT
} AsmRegister;

/* --- Size specifier --- */
typedef enum {
    ASM_SIZE_NONE,   /* no size specified */
    ASM_SIZE_BYTE,   /* 8-bit  */
    ASM_SIZE_WORD,   /* 16-bit */
    ASM_SIZE_DWORD,  /* 32-bit */
    ASM_SIZE_QWORD,  /* 64-bit */
    ASM_SIZE_OWORD,  /* 128-bit (SSE) */
    ASM_SIZE_TWORD,  /* 80-bit (x87) */
} AsmSizeSpec;

/* --- Addressing mode --- */
typedef enum {
    ASM_ADDR_NONE,           /* not a memory operand */
    ASM_ADDR_DIRECT,         /* [label] or [abs_addr] */
    ASM_ADDR_BASE,           /* [base] */
    ASM_ADDR_BASE_DISP,      /* [base + disp] */
    ASM_ADDR_BASE_INDEX,     /* [base + index*scale] */
    ASM_ADDR_BASE_INDEX_DISP,/* [base + index*scale + disp] */
    ASM_ADDR_RIP_REL,        /* [rel label] */
} AsmAddrMode;

/* --- Memory operand --- */
typedef struct {
    AsmAddrMode mode;
    AsmRegister base;         /* base register (ASM_REG_COUNT = none) */
    AsmRegister index;        /* index register (ASM_REG_COUNT = none) */
    int scale;                /* 1, 2, 4, 8 */
    int64_t displacement;     /* signed displacement */
    char label[ASM_MAX_LABEL_LEN]; /* label name for direct/rip-relative */
    AsmSizeSpec size;         /* size override (e.g., qword [rax]) */
} AsmMem;

/* --- Operand type --- */
typedef enum {
    ASM_OP_REGISTER,
    ASM_OP_IMMEDIATE,
    ASM_OP_MEMORY,
    ASM_OP_LABEL,       /* label reference (jump target, etc.) */
    ASM_OP_EXPR,        /* expression (e.g., label + 5) */
} AsmOperandType;

/* --- Operand --- */
typedef struct {
    AsmOperandType type;
    AsmRegister reg;
    int64_t immediate;
    AsmMem mem;
    char label[ASM_MAX_LABEL_LEN];
    char expr[ASM_MAX_LABEL_LEN]; /* raw expression text */
} AsmOperand;

/* --- Instruction --- */
typedef struct {
    char mnemonic[ASM_MAX_MNEMONIC_LEN];
    int operand_count;
    AsmOperand operands[ASM_MAX_OPERANDS];
    /* Metadata */
    int has_size_spec;       /* e.g., qword in "mov qword [rax], 0" */
    AsmSizeSpec size_spec;
    int is_lock_prefix;
    int is_rep_prefix;
} AsmInstruction;

/* --- Directive type --- */
typedef enum {
    ASM_DIR_NONE,
    ASM_DIR_SECTION,     /* section .text */
    ASM_DIR_GLOBAL,      /* global sym */
    ASM_DIR_EXTERN,      /* extern sym */
    ASM_DIR_ALIGN,       /* align N */
    ASM_DIR_TIMES,       /* times N ... */
    ASM_DIR_DB,          /* db ... */
    ASM_DIR_DW,          /* dw ... */
    ASM_DIR_DD,          /* dd ... */
    ASM_DIR_DQ,          /* dq ... */
    ASM_DIR_RESB,        /* resb N */
    ASM_DIR_RESW,        /* resw N */
    ASM_DIR_RESD,        /* resd N */
    ASM_DIR_RESQ,        /* resq N */
    ASM_DIR_ORG,         /* org N */
    ASM_DIR_BITS,        /* bits N */
    ASM_DIR_INCBIN,      /* incbin "file" */
    ASM_DIR_EQU,         /* sym equ value */
    ASM_DIR_STRUC,       /* struc / endstruc */
    ASM_DIR_MACRO,       /* %macro / %endmacro */
} AsmDirectiveType;

/* --- Directive --- */
typedef struct {
    AsmDirectiveType type;
    char name[ASM_MAX_LABEL_LEN];   /* section name, symbol name, etc. */
    int64_t value;                   /* alignment, count, etc. */
    int64_t value2;                  /* second value (e.g., times count) */
    AsmOperand data_operands[ASM_MAX_OPERANDS]; /* for db/dw/dd/dq */
    int data_count;
} AsmDirective;

/* --- Label --- */
typedef struct {
    char name[ASM_MAX_LABEL_LEN];
    int is_global;       /* was declared with global */
    int is_local;        /* starts with '.' */
} AsmLabel;

/* --- Block-level element type --- */
typedef enum {
    ASM_ELEM_INSTRUCTION,
    ASM_ELEM_DIRECTIVE,
    ASM_ELEM_LABEL,
    ASM_ELEM_COMMENT,
} AsmElementType;

/* --- Block element (instruction, directive, or label) --- */
typedef struct {
    AsmElementType type;
    AsmInstruction instr;
    AsmDirective directive;
    AsmLabel label;
    char comment[256];
} AsmElement;

/* --- Complete assembly block --- */
typedef struct {
    AsmElement elements[ASM_MAX_INSTRUCTIONS];
    int count;
    char source_file[256];   /* original .ae file */
    int source_line;         /* line number in .ae file */
} AsmBlock;

/* --- Target architecture --- */
typedef enum {
    ASM_ARCH_X86_64,
    ASM_ARCH_ARM64,
    ASM_ARCH_RISCV64,
} AsmArch;

/* --- Backend interface: emit AsmBlock as target assembly text --- */
typedef struct {
    AsmArch arch;
    char *(*emit)(const AsmBlock *block, size_t *out_len);
    void (*destroy)(char *output);
} AsmBackend;

/* --- Register translation table --- */
typedef struct {
    AsmRegister asm_reg;       /* architecture-neutral register */
    const char *x86_64_name;   /* "rax", "rbx", etc. */
    const char *arm64_name;    /* "x0", "x1", etc. */
    const char *riscv64_name;  /* "a0", "s0", etc. */
    int is_callee_saved;       /* callee-saved in standard calling conv */
    int width_bits;            /* 8, 16, 32, 64, 128 */
} AsmRegEntry;

/* --- Global register table (defined in asm_ir.c) --- */
extern const AsmRegEntry asm_reg_table[ASM_REG_COUNT];

/* --- Helper functions --- */
const char *asm_reg_name(AsmRegister reg, AsmArch arch);
AsmRegister asm_reg_from_name(const char *name);
int asm_reg_width(AsmRegister reg);
int asm_reg_is_callee_saved(AsmRegister reg);

/* --- Block construction --- */
void asm_block_init(AsmBlock *block);
int asm_block_add_instr(AsmBlock *block, const AsmInstruction *instr);
int asm_block_add_directive(AsmBlock *block, const AsmDirective *dir);
int asm_block_add_label(AsmBlock *block, const char *name, int is_global);
int asm_block_add_comment(AsmBlock *block, const char *text);

/* --- Instruction construction --- */
void asm_instr_init(AsmInstruction *instr, const char *mnemonic);
void asm_instr_add_operand(AsmInstruction *instr, const AsmOperand *op);

/* --- Operand construction --- */
void asm_op_reg(AsmOperand *op, AsmRegister reg);
void asm_op_imm(AsmOperand *op, int64_t value);
void asm_op_label(AsmOperand *op, const char *label);
void asm_op_mem(AsmOperand *op, const AsmMem *mem);
void asm_op_expr(AsmOperand *op, const char *expr);

/* --- Memory operand construction --- */
void asm_mem_init(AsmMem *mem);
void asm_mem_base(AsmMem *mem, AsmRegister base);
void asm_mem_base_disp(AsmMem *mem, AsmRegister base, int64_t disp);
void asm_mem_base_index(AsmMem *mem, AsmRegister base, AsmRegister index, int scale);
void asm_mem_full(AsmMem *mem, AsmRegister base, AsmRegister index, int scale, int64_t disp);
void asm_mem_rip_rel(AsmMem *mem, const char *label);

/* --- Backend creation --- */
AsmBackend *asm_backend_create(AsmArch arch);
void asm_backend_free(AsmBackend *backend);

#endif /* AETHER_ASM_IR_H */
