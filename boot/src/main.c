#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <boot.h>
#include <boot_info.h>
#include <gop.h>
#include <acpi.h>
#include <kernel_loader.h>
#include <memory_map.h>

// System V ABI - entry for Kernel
typedef void (__attribute__((sysv_abi)) *KERNEL_ENTRY)(BootInfo *);

EFI_SYSTEM_TABLE  *gST = NULL;
EFI_BOOT_SERVICES *gBS = NULL;
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

    // 6. Get final memory map and exit UEFI
    // After this point, UEFI Boot Services are terminated. No more boot_log or gBS calls
    if (EFI_ERROR(final_memory_map_and_exit(&bi.mmap, ImageHandle))) {
        // Only HLT
        for(;;) { __asm__("hlt"); }
    }

    // 7. Jump to kernel. Passing BootInfo pointer in RDI (SysV ABI)
    KERNEL_ENTRY kernel_main = (KERNEL_ENTRY)bi.kernel.kernel_entry;
    kernel_main(&bi);

    // If kernel return we need to stop it (It shouldn't)
    for (;;) { __asm__("hlt"); }
    
    return EFI_SUCCESS;
}