#ifndef KERNEL_LOADER_H
#define KERNEL_LOADER_H

#include <Uefi.h>
#include <boot_info.h>

EFI_STATUS init_kernel(EFI_HANDLE ImageHandle, BootKernel *kernel);

#endif
