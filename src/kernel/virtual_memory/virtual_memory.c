#include "virtual_memory.h"

#include "kernel/page_allocator/page_allocator.h"

#include "kernel/string/string.h"

VirtualAddressSpace* virtual_memory_create()
{
    VirtualAddressSpace* addressSpace = (VirtualAddressSpace*)page_allocator_request();
    memset(addressSpace, 0, 0x1000);

    return addressSpace;
}

void virtual_memory_remap_range(VirtualAddressSpace* addressSpace, void* virtualAddress, void* physicalAddress, uint64_t size)
{
    for (uint64_t offset = 0; offset < size + 0x1000; offset += 0x1000)
    {
        virtual_memory_remap(addressSpace, (void*)((uint64_t)virtualAddress + offset), (void*)((uint64_t)physicalAddress + offset));
    }
}

void virtual_memory_remap(VirtualAddressSpace* addressSpace, void* virtualAddress, void* physicalAddress)
{    
    uint64_t indexer = (uint64_t)virtualAddress;
    indexer >>= 12;
    uint64_t pIndex = indexer & 0x1ff;

    indexer >>= 9;
    uint64_t ptIndex = indexer & 0x1ff;

    indexer >>= 9;
    uint64_t pdIndex = indexer & 0x1ff;

    indexer >>= 9;
    uint64_t pdpIndex = indexer & 0x1ff;

    PageDirEntry pde;

    pde = addressSpace->Entries[pdpIndex];
    PageTable* pdp;
    if (!pde.Present)
    {
        pdp = (PageTable*)page_allocator_request();
        memset(pdp, 0, 0x1000);
        pde.Address = (uint64_t)pdp >> 12;
        pde.Present = 1;
        pde.ReadWrite = 1;
        addressSpace->Entries[pdpIndex] = pde;
    }
    else
    {
        pdp = (PageTable*)((uint64_t)pde.Address << 12);
    }
    
    
    pde = pdp->Entries[pdIndex];
    PageTable* pd;
    if (!pde.Present)
    {
        pd = (PageTable*)page_allocator_request();
        memset(pd, 0, 0x1000);
        pde.Address = (uint64_t)pd >> 12;
        pde.Present = 1;
        pde.ReadWrite = 1;
        pdp->Entries[pdIndex] = pde;
    }
    else
    {
        pd = (PageTable*)((uint64_t)pde.Address << 12);
    }

    pde = pd->Entries[ptIndex];
    PageTable* pt;
    if (!pde.Present)
    {
        pt = (PageTable*)page_allocator_request();
        memset(pt, 0, 0x1000);
        pde.Address = (uint64_t)pt >> 12;
        pde.Present = 1;
        pde.ReadWrite = 1;
        pd->Entries[ptIndex] = pde;
    }
    else
    {
        pt = (PageTable*)((uint64_t)pde.Address << 12);
    }

    pde = pt->Entries[pIndex];
    pde.Address = (uint64_t)physicalAddress >> 12;
    pde.Present = 1;
    pde.ReadWrite = 1;
    pt->Entries[pIndex] = pde;
}

void virtual_memory_erase(VirtualAddressSpace* addressSpace)
{    
    PageDirEntry pde;

    for (uint64_t pdpIndex = 0; pdpIndex < 512; pdpIndex++)
    {
        pde = addressSpace->Entries[pdpIndex];
        PageTable* pdp;
        if (pde.Present)
        {
            pdp = (PageTable*)((uint64_t)pde.Address << 12);
            page_allocator_unlock_page(pdp);
        
            for (uint64_t pdIndex = 0; pdIndex < 512; pdIndex++)
            {
                pde = pdp->Entries[pdIndex]; 
                PageTable* pd;
                if (pde.Present)
                {
                    pd = (PageTable*)((uint64_t)pde.Address << 12);
                    page_allocator_unlock_page(pd);
                    for (uint64_t ptIndex = 0; ptIndex < 512; ptIndex++)
                    {
                        pde = pd->Entries[ptIndex];
                        PageTable* pt;
                        if (pde.Present)
                        {
                            pt = (PageTable*)((uint64_t)pde.Address << 12);
                            page_allocator_unlock_page(pt);
                        }
                    }
                }
            }
        }
    }

    page_allocator_unlock_page(addressSpace);
}