#include "pmm.h"

#include <stddef.h>
#include <string.h>

#include <common/boot_info/boot_info.h>

#include "debug/debug.h"
#include "lock/lock.h"
#include "vmm/vmm.h"
#include "utils/utils.h"

static PageHeader* firstPage = NULL;
static PageHeader* lastPage = NULL;
static uint64_t pageAmount = 0;
static uint64_t freePageAmount = 0;

static Lock lock;

static inline uint8_t is_type_usable(uint64_t memoryType)
{
	return memoryType == EFI_CONVENTIONAL_MEMORY ||
		memoryType == EFI_LOADER_CODE ||
		memoryType == EFI_LOADER_DATA ||
		memoryType == EFI_BOOT_SERVICES_CODE ||
		memoryType == EFI_BOOT_SERVICES_DATA;
}

static inline void pmm_free_unlocked(void* address)
{
    freePageAmount++;

    lastPage->next = VMM_LOWER_TO_HIGHER(address);
    lastPage->next->next = NULL;
    lastPage = lastPage->next;
}

static inline void pmm_free_pages_unlocked(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        void* a = (void*)((uint64_t)address + i * PAGE_SIZE);
        pmm_free_unlocked(a);
    }    
}

static void pmm_load_memory_map(EfiMemoryMap* memoryMap)
{
    uint64_t i = 0;
    for (; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);
        
        if (is_type_usable(desc->type))
        {        
            firstPage = VMM_LOWER_TO_HIGHER(desc->physicalStart);
            firstPage->next = NULL;
            lastPage = firstPage;

            if (desc->amountOfPages != 0)
            {
                pmm_free_pages_unlocked((void*)((uint64_t)desc->physicalStart + PAGE_SIZE), desc->amountOfPages - 1);  
            }

            i++;
            break;
        }
    }

    for (; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);
        
        if (is_type_usable(desc->type))
        {
            pmm_free_pages_unlocked(desc->physicalStart, desc->amountOfPages);  
        }
    }
}

static void pmm_detect_memory(EfiMemoryMap* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (is_type_usable(desc->type))
        {
            pageAmount += desc->amountOfPages;
        }
    }
}

void pmm_init(EfiMemoryMap* memoryMap)
{   
    lock = lock_create();

    pmm_detect_memory(memoryMap);
    pmm_load_memory_map(memoryMap);
}

void* pmm_allocate(void)
{
    lock_acquire(&lock);

    void* address = firstPage;
    if (address == NULL)
    {
        lock_release(&lock);
        debug_panic("Physical Memory Manager full!");
    }
    
    firstPage = firstPage->next;
    freePageAmount--;

    lock_release(&lock);
    return VMM_HIGHER_TO_LOWER(address);
}

void pmm_free(void* address)
{
    lock_acquire(&lock);
    pmm_free_unlocked(address);
    lock_release(&lock);
}

void pmm_free_pages(void* address, uint64_t count)
{
    lock_acquire(&lock);
    pmm_free_pages_unlocked(address, count);
    lock_release(&lock);
}

uint64_t pmm_total_amount(void)
{
    return pageAmount;
}

uint64_t pmm_free_amount(void)
{
    return freePageAmount;
}

uint64_t pmm_reserved_amount(void)
{
    return pageAmount - freePageAmount;
}