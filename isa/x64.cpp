#include "isa/x64.hpp"
#include "linear/registers.hpp"

// x86-64 (System V AMD64 ABI) platform information
//
// Register ID layout (= vector index):
//   GPRs with 8-bit high sub-register (rax, rbx, rcx, rdx):
//     base+0 = 64-bit, base+1 = 32-bit, base+2 = 16-bit, base+3 = 8-bit low, base+4 = 8-bit high
//     stride = 5
//   GPRs without high sub-register (rsi, rdi, rbp, rsp, r8-r15):
//     base+0 = 64-bit, base+1 = 32-bit, base+2 = 16-bit, base+3 = 8-bit low
//     stride = 4
//   XMM registers:
//     one entry each (used for both float32 and float64)
//
// ID assignment:
//   rax family:  0-4     rbx family:  5-9     rcx family: 10-14    rdx family: 15-19
//   rsi family: 20-23    rdi family: 24-27    rbp family: 28-31    rsp family: 32-35
//   r8  family: 36-39    r9  family: 40-43    r10 family: 44-47    r11 family: 48-51
//   r12 family: 52-55    r13 family: 56-59    r14 family: 60-63    r15 family: 64-67
//   xmm0: 68   xmm1: 69   xmm2: 70   xmm3: 71   xmm4: 72   xmm5: 73   xmm6: 74   xmm7: 75
//   xmm8: 76   xmm9: 77   xmm10: 78  xmm11: 79  xmm12: 80  xmm13: 81  xmm14: 82  xmm15: 83

michaelcc::platform_info michaelcc::isa::x64::platform_info{
    .pointer_size   = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64,
    .short_size     = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16,
    .int_size       = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32,
    .long_size      = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32,
    .long_long_size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64,
    .float_size     = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32,
    .double_size    = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64,
    .max_alignment  = 16,

    .return_register_int8_id   = 3,   // al
    .return_register_int16_id  = 2,   // ax
    .return_register_int32_id  = 1,   // eax
    .return_register_int64_id  = 0,   // rax
    .return_register_float32_id = 68, // xmm0
    .return_register_float64_id = 68, // xmm0

    .registers = {
        // ===== rax family (ids 0-4) =====
        { .id = 0,  .name = "rax", .description = "RAX",
          .mutually_exclusive_registers = {1, 2, 3, 4},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 1,  .name = "eax", .description = "EAX",
          .mutually_exclusive_registers = {0, 2, 3, 4},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 2,  .name = "ax",  .description = "AX",
          .mutually_exclusive_registers = {0, 1, 3, 4},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 3,  .name = "al",  .description = "AL",
          .mutually_exclusive_registers = {0, 1, 2},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 4,  .name = "ah",  .description = "AH",
          .mutually_exclusive_registers = {0, 1, 2},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== rbx family (ids 5-9) =====
        { .id = 5,  .name = "rbx", .description = "RBX",
          .mutually_exclusive_registers = {6, 7, 8, 9},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 6,  .name = "ebx", .description = "EBX",
          .mutually_exclusive_registers = {5, 7, 8, 9},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 7,  .name = "bx",  .description = "BX",
          .mutually_exclusive_registers = {5, 6, 8, 9},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 8,  .name = "bl",  .description = "BL",
          .mutually_exclusive_registers = {5, 6, 7},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 9,  .name = "bh",  .description = "BH",
          .mutually_exclusive_registers = {5, 6, 7},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== rcx family (ids 10-14) =====
        { .id = 10, .name = "rcx", .description = "RCX",
          .mutually_exclusive_registers = {11, 12, 13, 14},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 11, .name = "ecx", .description = "ECX",
          .mutually_exclusive_registers = {10, 12, 13, 14},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 12, .name = "cx",  .description = "CX",
          .mutually_exclusive_registers = {10, 11, 13, 14},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 13, .name = "cl",  .description = "CL",
          .mutually_exclusive_registers = {10, 11, 12},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 14, .name = "ch",  .description = "CH",
          .mutually_exclusive_registers = {10, 11, 12},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== rdx family (ids 15-19) =====
        { .id = 15, .name = "rdx", .description = "RDX",
          .mutually_exclusive_registers = {16, 17, 18, 19},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 16, .name = "edx", .description = "EDX",
          .mutually_exclusive_registers = {15, 17, 18, 19},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 17, .name = "dx",  .description = "DX",
          .mutually_exclusive_registers = {15, 16, 18, 19},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 18, .name = "dl",  .description = "DL",
          .mutually_exclusive_registers = {15, 16, 17},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 19, .name = "dh",  .description = "DH",
          .mutually_exclusive_registers = {15, 16, 17},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== rsi family (ids 20-23) =====
        { .id = 20, .name = "rsi", .description = "RSI",
          .mutually_exclusive_registers = {21, 22, 23},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 21, .name = "esi", .description = "ESI",
          .mutually_exclusive_registers = {20, 22, 23},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 22, .name = "si",  .description = "SI",
          .mutually_exclusive_registers = {20, 21, 23},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 23, .name = "sil", .description = "SIL",
          .mutually_exclusive_registers = {20, 21, 22},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== rdi family (ids 24-27) =====
        { .id = 24, .name = "rdi", .description = "RDI",
          .mutually_exclusive_registers = {25, 26, 27},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 25, .name = "edi", .description = "EDI",
          .mutually_exclusive_registers = {24, 26, 27},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 26, .name = "di",  .description = "DI",
          .mutually_exclusive_registers = {24, 25, 27},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 27, .name = "dil", .description = "DIL",
          .mutually_exclusive_registers = {24, 25, 26},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== rbp family (ids 28-31) =====
        { .id = 28, .name = "rbp", .description = "RBP",
          .mutually_exclusive_registers = {29, 30, 31},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 29, .name = "ebp", .description = "EBP",
          .mutually_exclusive_registers = {28, 30, 31},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 30, .name = "bp",  .description = "BP",
          .mutually_exclusive_registers = {28, 29, 31},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 31, .name = "bpl", .description = "BPL",
          .mutually_exclusive_registers = {28, 29, 30},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== rsp family (ids 32-35) =====
        { .id = 32, .name = "rsp", .description = "RSP",
          .mutually_exclusive_registers = {33, 34, 35},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 33, .name = "esp", .description = "ESP",
          .mutually_exclusive_registers = {32, 34, 35},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 34, .name = "sp",  .description = "SP",
          .mutually_exclusive_registers = {32, 33, 35},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 35, .name = "spl", .description = "SPL",
          .mutually_exclusive_registers = {32, 33, 34},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== r8 family (ids 36-39) =====
        { .id = 36, .name = "r8",   .description = "R8",
          .mutually_exclusive_registers = {37, 38, 39},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 37, .name = "r8d",  .description = "R8D",
          .mutually_exclusive_registers = {36, 38, 39},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 38, .name = "r8w",  .description = "R8W",
          .mutually_exclusive_registers = {36, 37, 39},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 39, .name = "r8b",  .description = "R8B",
          .mutually_exclusive_registers = {36, 37, 38},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== r9 family (ids 40-43) =====
        { .id = 40, .name = "r9",   .description = "R9",
          .mutually_exclusive_registers = {41, 42, 43},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 41, .name = "r9d",  .description = "R9D",
          .mutually_exclusive_registers = {40, 42, 43},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 42, .name = "r9w",  .description = "R9W",
          .mutually_exclusive_registers = {40, 41, 43},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 43, .name = "r9b",  .description = "R9B",
          .mutually_exclusive_registers = {40, 41, 42},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== r10 family (ids 44-47) =====
        { .id = 44, .name = "r10",  .description = "R10",
          .mutually_exclusive_registers = {45, 46, 47},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 45, .name = "r10d", .description = "R10D",
          .mutually_exclusive_registers = {44, 46, 47},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 46, .name = "r10w", .description = "R10W",
          .mutually_exclusive_registers = {44, 45, 47},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 47, .name = "r10b", .description = "R10B",
          .mutually_exclusive_registers = {44, 45, 46},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== r11 family (ids 48-51) =====
        { .id = 48, .name = "r11",  .description = "R11",
          .mutually_exclusive_registers = {49, 50, 51},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 49, .name = "r11d", .description = "R11D",
          .mutually_exclusive_registers = {48, 50, 51},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 50, .name = "r11w", .description = "R11W",
          .mutually_exclusive_registers = {48, 49, 51},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 51, .name = "r11b", .description = "R11B",
          .mutually_exclusive_registers = {48, 49, 50},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== r12 family (ids 52-55) =====
        { .id = 52, .name = "r12",  .description = "R12",
          .mutually_exclusive_registers = {53, 54, 55},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 53, .name = "r12d", .description = "R12D",
          .mutually_exclusive_registers = {52, 54, 55},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 54, .name = "r12w", .description = "R12W",
          .mutually_exclusive_registers = {52, 53, 55},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 55, .name = "r12b", .description = "R12B",
          .mutually_exclusive_registers = {52, 53, 54},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== r13 family (ids 56-59) =====
        { .id = 56, .name = "r13",  .description = "R13",
          .mutually_exclusive_registers = {57, 58, 59},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 57, .name = "r13d", .description = "R13D",
          .mutually_exclusive_registers = {56, 58, 59},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 58, .name = "r13w", .description = "R13W",
          .mutually_exclusive_registers = {56, 57, 59},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 59, .name = "r13b", .description = "R13B",
          .mutually_exclusive_registers = {56, 57, 58},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== r14 family (ids 60-63) =====
        { .id = 60, .name = "r14",  .description = "R14",
          .mutually_exclusive_registers = {61, 62, 63},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 61, .name = "r14d", .description = "R14D",
          .mutually_exclusive_registers = {60, 62, 63},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 62, .name = "r14w", .description = "R14W",
          .mutually_exclusive_registers = {60, 61, 63},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 63, .name = "r14b", .description = "R14B",
          .mutually_exclusive_registers = {60, 61, 62},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== r15 family (ids 64-67) =====
        { .id = 64, .name = "r15",  .description = "R15",
          .mutually_exclusive_registers = {65, 66, 67},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 65, .name = "r15d", .description = "R15D",
          .mutually_exclusive_registers = {64, 66, 67},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT32, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 66, .name = "r15w", .description = "R15W",
          .mutually_exclusive_registers = {64, 65, 67},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT16, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },
        { .id = 67, .name = "r15b", .description = "R15B",
          .mutually_exclusive_registers = {64, 65, 66},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_BYTE,   .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_INTEGER },

        // ===== XMM registers (ids 68-83) =====
        { .id = 68, .name = "xmm0",  .description = "XMM0",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 69, .name = "xmm1",  .description = "XMM1",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 70, .name = "xmm2",  .description = "XMM2",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 71, .name = "xmm3",  .description = "XMM3",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 72, .name = "xmm4",  .description = "XMM4",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 73, .name = "xmm5",  .description = "XMM5",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 74, .name = "xmm6",  .description = "XMM6",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 75, .name = "xmm7",  .description = "XMM7",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 76, .name = "xmm8",  .description = "XMM8",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 77, .name = "xmm9",  .description = "XMM9",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 78, .name = "xmm10", .description = "XMM10",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 79, .name = "xmm11", .description = "XMM11",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 80, .name = "xmm12", .description = "XMM12",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 81, .name = "xmm13", .description = "XMM13",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 82, .name = "xmm14", .description = "XMM14",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
        { .id = 83, .name = "xmm15", .description = "XMM15",
          .mutually_exclusive_registers = {},
          .size = michaelcc::linear::word_size::MICHAELCC_WORD_SIZE_UINT64, .reg_class = michaelcc::linear::register_class::MICHAELCC_REGISTER_CLASS_FLOATING_POINT },
    }
};
