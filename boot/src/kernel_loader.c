#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Protocol/LoadedImage.h>
#include <Library/UefiBootServicesTableLib.h>

#include <boot.h>
#include <boot_info.h>
#include <kernel_loader.h>

//
// Minimal ELF64 structures for x86_64 kernel loading
//
#define EI_NIDENT 16
#define PT_LOAD   1
#define EM_X86_64 62

typedef struct {
    UINT8  e_ident[EI_NIDENT];
    UINT16 e_type;
    UINT16 e_machine;
    UINT32 e_version;
    UINT64 e_entry;
    UINT64 e_phoff;
    UINT64 e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;
    UINT16 e_shentsize;
    UINT16 e_shnum;
    UINT16 e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} Elf64_Phdr;

//
// Open the kernel file from the same device the bootloader was loaded from.
// For now, we assume the path is "\\kernel.elf" on the ESP.
//
static EFI_STATUS open_kernel_file(EFI_HANDLE ImageHandle, EFI_FILE_PROTOCOL **out_file) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root;
    EFI_FILE_PROTOCOL *KernelFile;

    // Get info about the current loaded image (the bootloader)
    Status = gBS->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID**)&LoadedImage
    );
    if (EFI_ERROR(Status)) {
        boot_panic(L"[KERNEL] HandleProtocol(LoadedImage) failed");
        return Status;
    }

    // Get the filesystem from the device the bootloader was loaded from
    Status = gBS->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID**)&FileSystem
    );
    if (EFI_ERROR(Status)) {
        boot_panic(L"[KERNEL] HandleProtocol(SimpleFileSystem) failed");
        return Status;
    }

    // Open the root directory of the filesystem (ESP)
    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        boot_panic(L"[KERNEL] OpenVolume failed");
        return Status;
    }

    // Open the kernel file
    Status = Root->Open(
        Root,
        &KernelFile,
        L"\\kernel.elf",  // !!! PATH !!!
        EFI_FILE_MODE_READ,
        0
    );
    if (EFI_ERROR(Status)) {
        boot_panic(L"[KERNEL] Failed to open \\kernel.elf");
        return Status;
    }

    *out_file = KernelFile;
    return EFI_SUCCESS;
}

//
// Load an ELF64 kernel image into memory at its physical addresses.
// We assume the kernel is linked for physical addresses (p_paddr).
//
static EFI_STATUS load_kernel_elf(EFI_FILE_PROTOCOL *File, BootKernel *outKernel) {
    EFI_STATUS Status;
    Elf64_Ehdr Ehdr;
    UINTN Size = sizeof(Ehdr);

    // Read ELF header
    Status = File->Read(File, &Size, &Ehdr);
    if (EFI_ERROR(Status) || Size != sizeof(Ehdr)) {
        boot_panic(L"[KERNEL] Failed to read ELF header");
        return EFI_LOAD_ERROR;
    }

    // Basic ELF64 / x86_64 validation
    if (Ehdr.e_ident[0] != 0x7F ||
        Ehdr.e_ident[1] != 'E' ||
        Ehdr.e_ident[2] != 'L' ||
        Ehdr.e_ident[3] != 'F') {
        boot_panic(L"[KERNEL] Invalid ELF magic");
        return EFI_UNSUPPORTED;
    }

    if (Ehdr.e_machine != EM_X86_64) {
        boot_panic(L"[KERNEL] ELF is not x86_64");
        return EFI_UNSUPPORTED;
    }

    // Read program headers
    UINTN PhSize = Ehdr.e_phentsize * Ehdr.e_phnum;
    Elf64_Phdr *Phdrs = NULL;

    Status = gBS->AllocatePool(EfiLoaderData, PhSize, (VOID**)&Phdrs);
    if (EFI_ERROR(Status) || Phdrs == NULL) {
        boot_panic(L"[KERNEL] AllocatePool for program headers failed");
        return Status;
    }

    Status = File->SetPosition(File, Ehdr.e_phoff);
    if (EFI_ERROR(Status)) {
        boot_panic(L"[KERNEL] SetPosition to program headers failed");
        gBS->FreePool(Phdrs);
        return Status;
    }

    Size = PhSize;
    Status = File->Read(File, &Size, Phdrs);
    if (EFI_ERROR(Status) || Size != PhSize) {
        boot_panic(L"[KERNEL] Failed to read program headers");
        gBS->FreePool(Phdrs);
        return EFI_LOAD_ERROR;
    }

    // Determine the physical address range covering all PT_LOAD segments
    UINT64 MinAddr = (UINT64)-1;
    UINT64 MaxAddr = 0;

    for (UINT16 i = 0; i < Ehdr.e_phnum; ++i) {
        Elf64_Phdr *Ph = &Phdrs[i];
        if (Ph->p_type != PT_LOAD) {
            continue;
        }

        if (Ph->p_paddr < MinAddr) {
            MinAddr = Ph->p_paddr;
        }

        UINT64 End = Ph->p_paddr + Ph->p_memsz;
        if (End > MaxAddr) {
            MaxAddr = End;
        }
    }

    if (MinAddr == (UINT64)-1 || MaxAddr <= MinAddr) {
        boot_panic(L"[KERNEL] No loadable PT_LOAD segments found");
        gBS->FreePool(Phdrs);
        return EFI_LOAD_ERROR;
    }

    UINT64 KernelPhysBase = MinAddr;
    UINT64 KernelSize     = MaxAddr - MinAddr;
    UINTN  Pages          = EFI_SIZE_TO_PAGES(KernelSize);
    EFI_PHYSICAL_ADDRESS AllocAddr = KernelPhysBase;

    // Allocate pages exactly at the physical address range required by the kernel
    // This assumes the kernel is linked for those physical addresses.
    EFI_STATUS allocStatus = gBS->AllocatePages(
        AllocateAddress,
        EfiLoaderCode,
        Pages,
        &AllocAddr
    );
    if (EFI_ERROR(allocStatus)) {
        boot_panic(L"[KERNEL] AllocatePages for kernel image failed");
        gBS->FreePool(Phdrs);
        return allocStatus;
    }

    // Load each PT_LOAD segment into memory
    for (UINT16 i = 0; i < Ehdr.e_phnum; ++i) {
        Elf64_Phdr *Ph = &Phdrs[i];
        if (Ph->p_type != PT_LOAD) {
            continue;
        }

        VOID   *SegmentDest = (VOID*)(UINTN)Ph->p_paddr;
        UINT64 FileSz       = Ph->p_filesz;
        UINT64 MemSz        = Ph->p_memsz;

        // Seek to segment offset in the file
        Status = File->SetPosition(File, Ph->p_offset);
        if (EFI_ERROR(Status)) {
            boot_panic(L"[KERNEL] SetPosition to segment failed");
            gBS->FreePool(Phdrs);
            return Status;
        }

        // Read file-backed part
        Size = (UINTN)FileSz;
        Status = File->Read(File, &Size, SegmentDest);
        if (EFI_ERROR(Status) || Size != FileSz) {
            boot_panic(L"[KERNEL] Failed to read segment data");
            gBS->FreePool(Phdrs);
            return EFI_LOAD_ERROR;
        }

        // Zero the remaining (BSS) if p_memsz > p_filesz
        if (MemSz > FileSz) {
            UINT8 *bss      = (UINT8*)SegmentDest + FileSz;
            UINT64 bss_size = MemSz - FileSz;
            for (UINT64 j = 0; j < bss_size; ++j) {
                bss[j] = 0;
            }
        }
    }

    gBS->FreePool(Phdrs);

    // Fill BootKernel structure
    outKernel->kernel_base  = (VOID*)(UINTN)KernelPhysBase;
    outKernel->kernel_size  = (UINTN)KernelSize;
    outKernel->kernel_entry = (VOID*)(UINTN)Ehdr.e_entry;

    boot_log(L"[KERNEL] ELF kernel loaded successfully");
    return EFI_SUCCESS;
}

//
// Public entry for kernel loading:
// - open filesystem
// - open kernel.elf
// - load ELF64 into memory
// - fill BootKernel
//
EFI_STATUS init_kernel(EFI_HANDLE ImageHandle, BootKernel *kernel) {
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *KernelFile = NULL;

    Status = open_kernel_file(ImageHandle, &KernelFile);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = load_kernel_elf(KernelFile, kernel);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}
