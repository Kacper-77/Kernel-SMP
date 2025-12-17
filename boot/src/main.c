#include <boot.h>
#include <boot_info.h>
#include <gop.h>
#include <memory_map.h>

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
    InitializeLib(ImageHandle, SystemTable);

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

    // Memory Map
    UINTN map_key = 0;
    status = init_memory_map(&boot_info.mmap, &map_key);
    if (EFI_ERROR(status)) {
        boot_panic(L"[BOOT] MemoryMap init failed");
    }

    boot_log(L"[BOOT] Nothing more to do yet.");
    return EFI_SUCCESS;
}
