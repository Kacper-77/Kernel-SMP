#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h> 
#include <Guid/FileInfo.h>
#include <boot.h>
#include <boot_info.h>
#include <gop.h>
#include <acpi.h>
#include <kernel_loader.h>
#include <memory_map.h>

// System V ABI - entry for Kernel
typedef void (__attribute__((sysv_abi)) *KERNEL_ENTRY)(BootInfo *);

EFI_SYSTEM_TABLE     *gST = NULL;
EFI_BOOT_SERVICES    *gBS = NULL;
EFI_RUNTIME_SERVICES *gRT = NULL;

//
// Logs
//
void boot_log(IN CHAR16 *msg) {
    if (gST && gST->ConOut) {
        gST->ConOut->OutputString(gST->ConOut, msg);
        gST->ConOut->OutputString(gST->ConOut, L"\r\n");
    }
}

void boot_panic(IN CHAR16 *msg) {
    boot_log(L"!!!! [BOOT PANIC] !!!!");
    boot_log(msg);
    for (;;) { __asm__("hlt"); }
}

//
// efi_main() - Start
//
EFI_STATUS
__attribute__((ms_abi))
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;

    // 1. Console start
    gST->ConOut->Reset(gST->ConOut, TRUE);
    gST->ConOut->ClearScreen(gST->ConOut);
    boot_log(L"[BOOT] Kernel-SMP Loader starting...");

    // 2. Init BootInfo
    BootInfo bi;
    SetMem(&bi, sizeof(BootInfo), 0);

    // 3. Get graphics info
    if (EFI_ERROR(init_gop(&bi.fb))) {
        boot_panic(L"Failed to initialize graphics");
    }

    // 4. Find ACPI (RSDP)
    if (EFI_ERROR(init_acpi(&bi.acpi))) {
        boot_panic(L"Failed to find ACPI tables");
    }

    // 5. Load kernel ELF to memory
    if (EFI_ERROR(init_kernel(ImageHandle, &bi.kernel))) {
        boot_panic(L"Failed to load kernel ELF");
    }

    boot_log(L"[BOOT] Preparation complete. Exiting Boot Services...");

    // !!!!!!!!!!!!!!!!!!  CLEANUP  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FS = NULL;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage = NULL;
    EFI_FILE_PROTOCOL* Root = NULL;
    EFI_FILE_PROTOCOL* InitrdFile = NULL;

    gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&LoadedImage);
    gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&FS);
    FS->OpenVolume(FS, &Root);

    EFI_STATUS s = Root->Open(Root, &InitrdFile, L"initrd.tar", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(s)) {
        boot_panic(L"Could not find initrd.tar!");
    }

    UINT64 FinalSize = 0;
    
    s = InitrdFile->SetPosition(InitrdFile, 0xFFFFFFFFFFFFFFFFULL);
    if (EFI_ERROR(s)) {
        boot_panic(L"SetPosition to end failed!");
    }

    s = InitrdFile->GetPosition(InitrdFile, &FinalSize);
    if (EFI_ERROR(s) || FinalSize == 0) {
        boot_panic(L"GetPosition failed or file is empty!");
    }

    InitrdFile->SetPosition(InitrdFile, 0);

    UINTN Pages = (FinalSize + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS RamdiskPhys;
    s = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, Pages, &RamdiskPhys);
    if (EFI_ERROR(s)) {
        boot_panic(L"Failed to allocate pages for RAMDISK");
    }

    UINTN BytesToRead = (UINTN)FinalSize;
    s = InitrdFile->Read(InitrdFile, &BytesToRead, (VOID*)RamdiskPhys);

    if (EFI_ERROR(s) || BytesToRead != FinalSize) {
        boot_panic(L"Read failed or size mismatch!");
    }

    bi.ramdisk_addr = (void*)RamdiskPhys;
    bi.ramdisk_size = (uint64_t)FinalSize;

    boot_log(L"[BOOT] RAMDISK loaded successfully.");
    // !!!!!!!!!!!!!!!!!!  CLEANUP  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    // 6. Get final memory map and exit UEFI
    // After this point, UEFI Boot Services are terminated. No more boot_log or gBS calls
    if (EFI_ERROR(final_memory_map_and_exit(&bi.mmap, ImageHandle))) {
        for(;;) { __asm__("hlt"); }
    }

    // 7. Jump to kernel. Passing BootInfo pointer in RDI (SysV ABI)
    KERNEL_ENTRY kernel_main = (KERNEL_ENTRY)bi.kernel.kernel_entry;
    kernel_main(&bi);

    // If kernel return we need to stop it (It shouldn't)
    for (;;) { __asm__("hlt"); }
    
    return EFI_SUCCESS;
}