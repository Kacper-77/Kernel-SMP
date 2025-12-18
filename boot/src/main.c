#include <boot.h>
#include <boot_info.h>
#include <gop.h>
#include <memory_map.h>
#include <acpi.h>

#include <Library/BaseMemoryLib.h>

EFI_SYSTEM_TABLE  *gST = NULL;
EFI_BOOT_SERVICES *gBS = NULL;
EFI_RUNTIME_SERVICES *gRT = NULL;

void boot_log(IN CHAR16 *msg) {
    if (gST && gST->ConOut) {
        gST->ConOut->OutputString(gST->ConOut, msg);
        gST->ConOut->OutputString(gST->ConOut, L"\r\n");
    }
}

void boot_panic(IN CHAR16 *msg) {
    boot_log(L"[BOOT PANIC]");
    boot_log(msg);
    for (;;) {}
}

EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;

    boot_log(L"[BOOT] UEFI entry OK");

    BootInfo boot_info;
    ZeroMem(&boot_info, sizeof(BootInfo));
    
    // GOP
    EFI_STATUS status = init_gop(&boot_info.fb);
    if (EFI_ERROR(status)) {
        boot_panic(L"[BOOT] GOP init failed");
    }

    // ACPI RSDP
    status = init_acpi(&boot_info.acpi);
    if (EFI_ERROR(status)) {
        boot_panic(L"[BOOT] ACPI init failed");
    }

    // Memory Map + Exit
    status = final_memory_map_and_exit(&boot_info.mmap, ImageHandle);

    boot_log(L"[BOOT] Nothing more to do yet.");
    return EFI_SUCCESS;
}
