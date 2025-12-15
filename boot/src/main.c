#include <boot.h>
#include <boot_info.h>

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
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;

    boot_log(L"[BOOT] UEFI entry OK");

    boot_log(L"[BOOT] Nothing more to do yet.");
    return EFI_SUCCESS;
}
