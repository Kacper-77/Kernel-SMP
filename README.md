# INTRODUCTION

Custom-built 64-bit multi-core kernel for the x86_64 architecture, written from scratch.

Designed as a low-level systems project to explore how modern operating systems manage memory, scheduling, and multi-core execution directly on hardware.

The goal is to build a practical environment for studying OS internals beyond high-level abstractions.

---

## Key Features

- Symmetric Multiprocessing (SMP) with INIT-SIPI-SIPI boot sequence  
- 4-level paging with Virtual Memory Areas (VMA)  
- Multilevel round-robin scheduler with work-stealing load balancing  
- Syscall interface for Ring 3 ↔ Ring 0 transitions  

---

## Current Status

Active development. A stable **pre-alpha** build is functional.

Capabilities:
- Multi-core boot (SMP)
- Dynamic memory management
- Basic user-space task execution

Targeted for experimentation and early testing.

---

## Documentation

See `docs/` for architecture details, design notes, and implementation breakdowns.

---

# BUILD AND RUN

## Prerequisites

### macOS
```bash
brew install cmake nasm qemu x86_64-elf-gcc x86_64-elf-binutils
```

### Windows (MSYS2 UCRT64)
```bash
pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-nasm \
mingw-w64-ucrt-x86_64-qemu mingw-w64-ucrt-x86_64-gcc
```

---

## Setup

Copy into project root:
- `edk2-x86_64-code.fd`
- `vars.fd`

---

## Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

---

## Run (QEMU)

### macOS
```bash
qemu-system-x86_64 \
-m 256M \
-cpu qemu64 \
-smp 32 \
-drive if=pflash,format=raw,readonly=on,file=./edk2-x86_64-code.fd \
-drive if=pflash,format=raw,file=./vars.fd \
-drive format=raw,file=fat:rw:esp \
-net none \
-chardev stdio,id=char0,mux=on \
-serial chardev:char0 \
-mon chardev=char0 \
-device isa-debugcon,iobase=0xe9,chardev=char0
```

### Windows (PowerShell)
```powershell
qemu-system-x86_64 -m 256M `
-cpu qemu64 -smp 32 `
-accel whpx `
-drive if=pflash,format=raw,readonly=on,file=.\edk2-x86_64-code.fd `
-drive if=pflash,format=raw,file=.\vars.fd `
-drive format=raw,file=fat:rw:esp `
-net none `
-chardev stdio,id=char0,mux=on `
-serial chardev:char0 `
-mon chardev=char0 `
-device isa-debugcon,iobase=0xe9,chardev=char0
```

---

## Troubleshooting

- **File not found** -> verify `edk2` and `vars` in root directory  
- **ESP locked (Windows)** -> close IDE / Explorer using folder  
- **No logs** -> check terminal output (kprint)  
- **Performance** -> reduce `-smp 32` to `4–8`  
