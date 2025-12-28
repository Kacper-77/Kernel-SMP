#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/UefiBootServicesTableLib.h>

#include <boot.h>
#include <boot_info.h>
#include <kernel_loader.h>

#include <stddef.h>

void SetMem(void *dst, size_t size, unsigned char value) {
    unsigned char *p = (unsigned char*)dst;
    while (size--) {
        *p++ = value;
    }
}

#define EI_NIDENT 16
#define PT_LOAD   1
#define EM_X86_64 62
#define ET_EXEC   2
#define ET_DYN    3

typedef unsigned long long UINT64;

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

static EFI_STATUS open_kernel_file(
    EFI_HANDLE ImageHandle,
    EFI_FILE_PROTOCOL **out_root,
    EFI_FILE_PROTOCOL **out_file
) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
    EFI_FILE_PROTOCOL *Root = NULL;
    EFI_FILE_PROTOCOL *KernelFile = NULL;

    Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&LoadedImage);
    if (EFI_ERROR(Status) || LoadedImage == NULL) {
        boot_panic(L"[KERNEL] HandleProtocol(LoadedImage) failed");
        return EFI_DEVICE_ERROR;
    }

    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem);
    if (EFI_ERROR(Status) || FileSystem == NULL) {
        boot_panic(L"[KERNEL] HandleProtocol(SimpleFileSystem) failed");
        return EFI_DEVICE_ERROR;
    }

    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status) || Root == NULL) {
        boot_panic(L"[KERNEL] OpenVolume failed");
        return EFI_DEVICE_ERROR;
    }

    Status = Root->Open(
        Root,
        &KernelFile,
        L"\\kernel.elf",
        EFI_FILE_MODE_READ,
        0
    );
    if (EFI_ERROR(Status) || KernelFile == NULL) {
        boot_panic(L"[KERNEL] Failed to open \\kernel.elf");

        Root->Close(Root);
        return EFI_NOT_FOUND;
    }

    *out_root = Root;
    *out_file = KernelFile;
    return EFI_SUCCESS;
}

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

    // Validate
    if (Ehdr.e_ident[0] != 0x7F || Ehdr.e_ident[1] != 'E' || Ehdr.e_ident[2] != 'L' || Ehdr.e_ident[3] != 'F') {
        boot_panic(L"[KERNEL] Invalid ELF magic");
        return EFI_UNSUPPORTED;
    }
    if (Ehdr.e_machine != EM_X86_64) {
        boot_panic(L"[KERNEL] ELF is not x86_64");
        return EFI_UNSUPPORTED;
    }
    if (Ehdr.e_phentsize != sizeof(Elf64_Phdr) || Ehdr.e_phnum == 0) {
        boot_panic(L"[KERNEL] Invalid program header layout");
        return EFI_UNSUPPORTED;
    }
    if (Ehdr.e_type != ET_EXEC && Ehdr.e_type != ET_DYN) {
        boot_panic(L"[KERNEL] Unsupported ELF type (expect ET_EXEC/ET_DYN)");
        return EFI_UNSUPPORTED;
    }

    // Read program headers
    UINTN PhSize = (UINTN)Ehdr.e_phentsize * (UINTN)Ehdr.e_phnum;
    Elf64_Phdr *Phdrs = NULL;
    Status = gBS->AllocatePool(EfiLoaderData, PhSize, (VOID**)&Phdrs);
    if (EFI_ERROR(Status) || Phdrs == NULL) {
        boot_panic(L"[KERNEL] AllocatePool for program headers failed");
        return EFI_OUT_OF_RESOURCES;
    }

    Status = File->SetPosition(File, Ehdr.e_phoff);
    if (EFI_ERROR(Status)) { gBS->FreePool(Phdrs); return Status; }

    Size = PhSize;
    Status = File->Read(File, &Size, Phdrs);
    if (EFI_ERROR(Status) || Size != PhSize) {
        boot_panic(L"[KERNEL] Failed to read program headers");
        gBS->FreePool(Phdrs);
        return EFI_LOAD_ERROR;
    }

    // Compute virtual range [lo, hi) across PT_LOAD segments
    UINT64 lo = ~(UINT64)0;
    UINT64 hi = 0;
    for (UINT16 i = 0; i < Ehdr.e_phnum; ++i) {
        Elf64_Phdr *Ph = &Phdrs[i];
        if (Ph->p_type != PT_LOAD || Ph->p_memsz == 0) continue;
        if (Ph->p_vaddr < lo) lo = Ph->p_vaddr;
        UINT64 end = Ph->p_vaddr + Ph->p_memsz;
        if (end > hi) hi = end;
    }
    if (lo == ~(UINT64)0 || hi <= lo) {
        boot_panic(L"[KERNEL] No loadable PT_LOAD segments found");
        gBS->FreePool(Phdrs);
        return EFI_LOAD_ERROR;
    }

    UINT64 imageSize = hi - lo;
    UINTN pages = EFI_SIZE_TO_PAGES(imageSize);

    // Allocate pages
    EFI_PHYSICAL_ADDRESS kernel_addr = 0x2000000;
    Status = gBS->AllocatePages(AllocateAddress, EfiLoaderCode, pages, &kernel_addr);
    if (EFI_ERROR(Status)) {
        boot_panic(L"[KERNEL] AllocatePages failed");
        gBS->FreePool(Phdrs);
        return Status;
    }

    UINT8 *dst_base = (UINT8*)(UINTN)kernel_addr;

    // Load segments: copy filesz, zero memsz - filesz
    for (UINT16 i = 0; i < Ehdr.e_phnum; ++i) {
        Elf64_Phdr *Ph = &Phdrs[i];
        if (Ph->p_type != PT_LOAD || Ph->p_memsz == 0) continue;

        UINT8 *seg_dst = dst_base + (Ph->p_vaddr - lo);

        // Read file-backed part
        if (Ph->p_filesz > 0) {
            Status = File->SetPosition(File, Ph->p_offset);
            if (EFI_ERROR(Status)) { gBS->FreePool(Phdrs); return Status; }

            UINTN to_read = (UINTN)Ph->p_filesz;
            Status = File->Read(File, &to_read, seg_dst);
            if (EFI_ERROR(Status) || to_read != Ph->p_filesz) {
                boot_panic(L"[KERNEL] Failed to read segment data");
                gBS->FreePool(Phdrs);
                return EFI_LOAD_ERROR;
            }
        }

        // Zero BSS
        if (Ph->p_memsz > Ph->p_filesz) {
            SetMem(seg_dst + Ph->p_filesz, (UINTN)(Ph->p_memsz - Ph->p_filesz), 0);
        }
    }

    gBS->FreePool(Phdrs);

    // Entry point
    outKernel->kernel_base  = (VOID*)(UINTN)kernel_addr;
    outKernel->kernel_size  = (UINTN)imageSize;
    
    UINT64 entry_offset = Ehdr.e_entry - lo;
    outKernel->kernel_entry = (VOID*)(UINTN)(kernel_addr + entry_offset);

    boot_log(L"[KERNEL] ELF kernel loaded (relocated)");
    return EFI_SUCCESS;
}

EFI_STATUS init_kernel(EFI_HANDLE ImageHandle, BootKernel *kernel) {
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *Root = NULL;
    EFI_FILE_PROTOCOL *KernelFile = NULL;

    Status = open_kernel_file(ImageHandle, &Root, &KernelFile);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = load_kernel_elf(KernelFile, kernel);

    // Close file handles before EBS
    if (KernelFile) {
        KernelFile->Close(KernelFile);
    }
    if (Root) {
        Root->Close(Root);
    }

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}
