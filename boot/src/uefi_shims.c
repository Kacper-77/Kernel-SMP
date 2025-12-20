#include <Uefi.h>
#include <Library/BaseLib.h>

//
// Minimal implementations to avoid linking full EDK2 libs
// Only what bootloader actually needs
//

//
// CompareGuid
//
BOOLEAN
EFIAPI
CompareGuid (
  IN CONST EFI_GUID  *Guid1,
  IN CONST EFI_GUID  *Guid2
  )
{
  UINTN i;
  const UINT8 *a = (const UINT8 *)Guid1;
  const UINT8 *b = (const UINT8 *)Guid2;

  for (i = 0; i < sizeof(EFI_GUID); ++i) {
    if (a[i] != b[i]) {
      return FALSE;
    }
  }
  return TRUE;
}

//
// ZeroMem
//
VOID *
EFIAPI
ZeroMem (
  OUT VOID  *Buffer,
  IN  UINTN Size
  )
{
  UINT8 *p = (UINT8 *)Buffer;
  UINTN i;

  for (i = 0; i < Size; ++i) {
    p[i] = 0;
  }
  return Buffer;
}

//
// UnicodeSPrint
//
UINTN
EFIAPI
UnicodeSPrint (
  OUT CHAR16        *StartOfBuffer,
  IN  UINTN         BufferSize,
  IN  CONST CHAR16  *FormatString,
  ...
  )
{
  if (StartOfBuffer != NULL && BufferSize >= sizeof(CHAR16)) {
    StartOfBuffer[0] = L'\0';
  }
  return 0;
}

//
// GUID
//

// ACPI 1.0 RSDP
EFI_GUID gEfiAcpiTableGuid = {
  0xeb9d2d30, 0x2d88, 0x11d3,
  { 0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d }
};

// ACPI 2.0+ RSDP
EFI_GUID gEfiAcpi20TableGuid = {
  0x8868e871, 0xe4f1, 0x11d3,
  { 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81 }
};

// Loaded Image Protocol
EFI_GUID gEfiLoadedImageProtocolGuid = {
  0x5B1B31A1, 0x9562, 0x11d2,
  { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
};

// Simple File System Protocol
EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
  0x964e5b22, 0x6459, 0x11d2,
  { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }
};

// Graphics Output Protocol
EFI_GUID gEfiGraphicsOutputProtocolGuid = 
    { 0x9042a9de, 0x23dc, 0x4a38,
      { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } };
