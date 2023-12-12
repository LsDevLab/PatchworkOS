#include "gdt/gdt.h"
#include "tty/tty.h"
#include "idt/idt.h"
#include "utils/utils.h"
#include "file_system/file_system.h"
#include "page_allocator/page_allocator.h"
#include "multitasking/multitasking.h"
#include "program_loader/program_loader.h"
#include "heap/heap.h"

#include "kernel/kernel.h"

#include "../common.h"

#include "debug/debug.h"

#include "string/string.h"

void user_space_entry()
{
    tty_print("Hello from user space!\n\r");
    
    while (1)
    {

    }
}

void _start(BootInfo* bootInfo)
{   
    kernel_init(bootInfo);

    tty_print("\n\rLoading program...\n\r");

    load_program("/programs/test/test.elf"); //This is really slow

    tty_print("Jumping to user space...\n\n\r");
    
    enable_interrupts();

    multitasking_yield_to_user_space();

    tty_print("\nBack in the main task, if you see this something has gone very wrong!\n\n\r");
    
    while (1)
    {
        asm volatile("hlt");
    }
}