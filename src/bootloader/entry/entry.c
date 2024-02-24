#include <efi.h>
#include <efilib.h>
#include <common/boot_info/boot_info.h>

#include "gop/gop.h"
#include "psf/psf.h"
#include "rsdt/rsdt.h"
#include "loader/loader.h"
#include "ram_disk/ram_disk.h"
#include "file_system/file_system.h"
#include "memory/memory.h"

EFI_STATUS efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* systemTable)
{
	InitializeLib(imageHandle, imageHandle);
	Print(L"Hello from the bootloader!\n\r");

	BootInfo* bootInfo = memory_allocate_pages(1, EFI_MEMORY_TYPE_BOOT_INFO);
	bootInfo->physicalAddress = (void*)bootInfo;

	gop_get_buffer(&bootInfo->gopBuffer);
	pst_font_load(imageHandle, &bootInfo->font, L"/fonts/zap-vga16.psf");

	EFI_FILE* rootHandle = file_system_open_root_volume(imageHandle);
	bootInfo->ramRoot = ram_disk_load_directory(rootHandle, "root");
	file_system_close(rootHandle);

	bootInfo->rsdp = rsdt_get(systemTable);
	bootInfo->runtimeServices = systemTable->RuntimeServices;

	loader_load_kernel(imageHandle, systemTable, bootInfo);

	return EFI_SUCCESS;
}
