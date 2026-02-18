#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h> 
#include <Guid/FileInfo.h>

#include <ramdisk.h>
#include <boot.h>
#include <boot_info.h>

typedef unsigned long long UINT64;

EFI_STATUS init_ramdisk(EFI_HANDLE ImageHandle, BootRamdisk *ramdisk) {
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
        return EFI_LOAD_ERROR;
    }

    UINT64 FinalSize = 0;
    
    s = InitrdFile->SetPosition(InitrdFile, 0xFFFFFFFFFFFFFFFFULL);
    if (EFI_ERROR(s)) {
        boot_panic(L"SetPosition to end failed!");
        return EFI_LOAD_ERROR;
    }

    s = InitrdFile->GetPosition(InitrdFile, &FinalSize);
    if (EFI_ERROR(s) || FinalSize == 0) {
        boot_panic(L"GetPosition failed or file is empty!");
        return EFI_LOAD_ERROR;
    }

    InitrdFile->SetPosition(InitrdFile, 0);

    UINTN Pages = (FinalSize + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS RamdiskPhys;
    s = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, Pages, &RamdiskPhys);
    if (EFI_ERROR(s)) {
        boot_panic(L"Failed to allocate pages for RAMDISK");
        return EFI_LOAD_ERROR;
    }

    UINTN BytesToRead = (UINTN)FinalSize;
    s = InitrdFile->Read(InitrdFile, &BytesToRead, (VOID*)RamdiskPhys);

    if (EFI_ERROR(s) || BytesToRead != FinalSize) {
        boot_panic(L"Read failed or size mismatch!");
        return EFI_LOAD_ERROR;
    }

    ramdisk->ramdisk_addr = (void*)RamdiskPhys;
    ramdisk->ramdisk_size = (uint64_t)FinalSize;

    boot_log(L"[BOOT] RAMDISK loaded successfully.");

    return EFI_SUCCESS;
}