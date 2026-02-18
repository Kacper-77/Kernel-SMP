#ifndef RAMDISK_H
#define RAMDISK_H

#include <Uefi.h>
#include <boot_info.h>

EFI_STATUS init_ramdisk(EFI_HANDLE ImageHandle, BootRamdisk *ramdisk);

#endif
