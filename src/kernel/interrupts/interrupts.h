#pragma once

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint64_t CR3;
    uint64_t R15;
    uint64_t R14;
    uint64_t R13;
    uint64_t R12;
    uint64_t R11;
    uint64_t R10;
    uint64_t R9;
    uint64_t R8;
    uint64_t RBP;
    uint64_t RDI;
    uint64_t RSI;
    uint64_t RDX;
    uint64_t RCX;
    uint64_t RBX;
    uint64_t RAX;

    uint64_t Vector;
    uint64_t ErrorCode;

    uint64_t InstructionPointer;
    uint64_t CodeSegment;
    uint64_t Flags;
    uint64_t StackPointer;
    uint64_t StackSegment;
} InterruptStackFrame;

enum 
{
    IRQ_PIT = 0,
    IRQ_KEYBOARD = 1,
    IRQ_CASCADE = 2,
    IRQ_COM2 = 3,
    IRQ_COM1 = 4,
    IRQ_LPT2 = 5,
    IRQ_FLOPPY = 6,
    IRQ_LPT1 = 7,
    IRQ_CMOS = 8,
    IRQ_FREE1 = 9,
    IRQ_FREE2 = 10,
    IRQ_FREE3 = 11,
    IRQ_PS2_MOUSE = 12,
    IRQ_FPU = 13,
    IRQ_PRIMARY_ATA_HARD_DISK = 14,
    IRQ_SECONDARY_ATA_HARD_DISK = 15
};

void interrupt_handler(InterruptStackFrame* stackFrame);

void irq_handler(InterruptStackFrame* stackFrame);

void exception_handler(InterruptStackFrame* stackFrame);