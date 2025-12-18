#include <boot.h>
#include <gop.h>
#include <Library/PrintLib.h>

EFI_STATUS init_gop(BootFramebuffer *fb) {
    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;

    EFI_STATUS status = gBS->LocateProtocol(&gopGuid, NULL, (VOID**)&gop);
    if (EFI_ERROR(status) || gop == NULL) {
        boot_panic(L"[GOP] Failed to locate GOP protocol");
    }

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = gop->Mode->Info;

    fb->framebuffer_base    = (VOID*)gop->Mode->FrameBufferBase;
    fb->framebuffer_size    = gop->Mode->FrameBufferSize;
    fb->width               = info->HorizontalResolution;
    fb->height              = info->VerticalResolution;
    fb->pixels_per_scanline = info->PixelsPerScanLine;

    boot_log(L"[GOP] Graphics Output Protocol OK");

    // Log resolution
    CHAR16 buf[128];
    UnicodeSPrint(buf, sizeof(buf), L"[GOP] Resolution: %ux%u",
                  fb->width, fb->height);
    boot_log(buf);

    // Log framebuffer base
    UnicodeSPrint(buf, sizeof(buf), L"[GOP] Framebuffer base: 0x%lx",
                  fb->framebuffer_base);
    boot_log(buf);

    // Log framebuffer size
    UnicodeSPrint(buf, sizeof(buf), L"[GOP] Framebuffer size: %lu bytes",
                  fb->framebuffer_size);
    boot_log(buf);

    return EFI_SUCCESS;
}
