#include "program_loader.h"

#include "kernel/string/string.h"
#include "kernel/file_system/file_system.h"
#include "kernel/heap/heap.h"
#include "kernel/page_allocator/page_allocator.h"

#include "kernel/tty/tty.h"

Program* load_program(const char* path)
{
    Program* newProgram = kmalloc(sizeof(Program));

    FILE* file = file_system_open(path, "r");

    if (file == 0)
    {
        tty_print("ERROR: Failed to open file!\n\r");
        return 0;
    }

    file_system_read(&newProgram->Header, sizeof(ElfHeader), file);

    if(newProgram->Header.Ident[0] != 0x7F ||
       newProgram->Header.Ident[1] != 'E' ||
       newProgram->Header.Ident[2] != 'L' ||
       newProgram->Header.Ident[3] != 'F')
    {
        tty_print("ERROR: Corrupt program file!\n\r");
    }

    uint64_t programHeaderTableSize = newProgram->Header.ProgramHeaderAmount * newProgram->Header.ProgramHeaderSize;

    ElfProgramHeader* programHeaders = kmalloc(programHeaderTableSize);

    file_system_read(programHeaders, programHeaderTableSize, file);

    newProgram->SegmentAmount = newProgram->Header.ProgramHeaderAmount;
    newProgram->Segments = kmalloc(newProgram->SegmentAmount * sizeof(ProgramSegment));

    for (uint64_t i = 0; i < newProgram->SegmentAmount; i++)
    {
        newProgram->Segments[i].PageAmount = 0;
        newProgram->Segments[i].Segment = 0;
    }
    
    newProgram->AddressSpace = virtual_memory_create();

    uint64_t lastMappedAddress = 0;
	for (uint64_t i = 0; i < newProgram->Header.ProgramHeaderAmount; i++)
	{		
        switch (programHeaders[i].Type)
		{
		case PT_LOAD:
		{
            newProgram->Segments[i].PageAmount = programHeaders[i].MemorySize / 0x1000 + 1;
            newProgram->Segments[i].Segment = page_allocator_request_amount(newProgram->Segments[i].PageAmount);

            file_system_seek(file, programHeaders[i].Offset, SEEK_SET);
            file_system_read(newProgram->Segments[i].Segment, programHeaders[i].MemorySize, file);

            for (uint64_t page = 0; page < newProgram->Segments[i].PageAmount; page++)
            {
                virtual_memory_remap(newProgram->AddressSpace, (void*)(programHeaders[i].VirtualAddress + page * 0x1000), (void*)(newProgram->Segments[i].Segment + page * 0x1000));
                lastMappedAddress = programHeaders[i].VirtualAddress + page * 0x1000;
            }
		}
		break;
		}
	}

    newProgram->StackTop = page_allocator_request();
    virtual_memory_remap(newProgram->AddressSpace, newProgram->StackTop, (void*)((lastMappedAddress + 0x1000) / 0x1000));

    kfree(programHeaders);
    file_system_close(file);

	return newProgram;
}