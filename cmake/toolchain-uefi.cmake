set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_AR x86_64-w64-mingw32-gcc-ar)
set(CMAKE_RANLIB x86_64-w64-mingw32-gcc-ranlib)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -fshort-wchar -mno-red-zone -fno-stack-protector -DMDE_CPU_X64=1")
