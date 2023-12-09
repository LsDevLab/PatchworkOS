#include "virtual_memory.h"

#include "page_allocator/page_allocator.h"

#include "string/string.h"
#include "debug/debug.h"

VirtualAddressSpace* virtual_memory_create()
{
    VirtualAddressSpace* addressSpace = (VirtualAddressSpace*)page_allocator_request();
    memset(addressSpace, 0, 0x1000);

    return addressSpace;
}

void virtual_memory_remap_pages(VirtualAddressSpace* addressSpace, void* virtualAddress, void* physicalAddress, uint64_t pageAmount)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        virtual_memory_remap(addressSpace, (void*)((uint64_t)virtualAddress + page * 0x1000), (void*)((uint64_t)physicalAddress + page * 0x1000));
    }
}

void virtual_memory_remap(VirtualAddressSpace* addressSpace, void* virtualAddress, void* physicalAddress)
{    
    if ((uint64_t)virtualAddress % 0x1000 != 0)
    {
        debug_error("Attempt to map invalid virtual address!");
    }    
    else if ((uint64_t)physicalAddress % 0x1000 != 0)
    {
        debug_error("Attempt to map invalid physical address!");
    }

    uint64_t indexer = (uint64_t)virtualAddress;
    indexer >>= 12;
    uint64_t pIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t ptIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdpIndex = indexer & 0x1ff;

    PageDirEntry pde = addressSpace->Entries[pdpIndex];
    PageDirectory* pdp;
    if (!PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
    {
        pdp = (PageDirectory*)page_allocator_request();
        memset(pdp, 0, 0x1000);
        PAGE_DIR_SET_ADDRESS(pde, (uint64_t)pdp >> 12);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_PRESENT);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_READ_WRITE);
        addressSpace->Entries[pdpIndex] = pde;
    }
    else
    {
        pdp = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde) << 12);
    }
    
    pde = pdp->Entries[pdIndex];
    PageDirectory* pd;
    if (!PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
    {
        pd = (PageDirectory*)page_allocator_request();
        memset(pd, 0, 0x1000);
        PAGE_DIR_SET_ADDRESS(pde, (uint64_t)pd >> 12);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_PRESENT);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_READ_WRITE);
        pdp->Entries[pdIndex] = pde;
    }
    else
    {
        pd = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde) << 12);
    }

    pde = pd->Entries[ptIndex];
    PageDirectory* pt;
    if (!PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
    {
        pt = (PageDirectory*)page_allocator_request();
        memset(pt, 0, 0x1000);
        PAGE_DIR_SET_ADDRESS(pde, (uint64_t)pt >> 12);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_PRESENT);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_READ_WRITE);
        pd->Entries[ptIndex] = pde;
    }
    else
    {
        pt = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde) << 12);
    }

    pde = pt->Entries[pIndex];
    PAGE_DIR_SET_ADDRESS(pde, (uint64_t)physicalAddress >> 12);
    PAGE_DIR_SET_FLAG(pde, PAGE_DIR_PRESENT);
    PAGE_DIR_SET_FLAG(pde, PAGE_DIR_READ_WRITE);
    pt->Entries[pIndex] = pde;
}

void virtual_memory_erase(VirtualAddressSpace* addressSpace)
{    
    PageDirEntry pde;

    for (uint64_t pdpIndex = 0; pdpIndex < 512; pdpIndex++)
    {
        pde = addressSpace->Entries[pdpIndex];
        PageDirectory* pdp;
        if (PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
        {
            pdp = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde) << 12);        
            for (uint64_t pdIndex = 0; pdIndex < 512; pdIndex++)
            {
                pde = pdp->Entries[pdIndex]; 
                PageDirectory* pd;
                if (PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
                {
                    pd = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde) << 12);
                    for (uint64_t ptIndex = 0; ptIndex < 512; ptIndex++)
                    {
                        pde = pd->Entries[ptIndex];
                        PageDirectory* pt;
                        if (PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
                        {
                            pt = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde) << 12);
                            page_allocator_unlock_page(pt);
                        }
                    }
                    page_allocator_unlock_page(pd);
                }
            }
            page_allocator_unlock_page(pdp);
        }
    }

    page_allocator_unlock_page(addressSpace);
}

void virtual_memory_invalidate_page(void* address) 
{
   asm volatile("invlpg (%0)" :: "r" ((void*)address) : "memory");
}