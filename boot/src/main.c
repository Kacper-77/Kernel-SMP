#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>

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
__attribute__((ms_abi))
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gRT = SystemTable->RuntimeServices;

    if (gST && gST->ConOut) {
        gST->ConOut->Reset(gST->ConOut, TRUE);
        gST->ConOut->ClearScreen(gST->ConOut);
        gST->ConOut->OutputString(gST->ConOut, L"[BOOT] console ready\r\n");
    }

    boot_log(L"[BOOT] UEFI entry OK");

    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
    EFI_STATUS s = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (void**)&Gop);
    if (EFI_ERROR(s) || !Gop) boot_panic(L"[BOOT] GOP locate failed");

    boot_log(L"[BOOT] drew red pre-EBS");

    for (;;) { }
    return EFI_SUCCESS;
}
