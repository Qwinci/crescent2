set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR "x86_64")

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET "x86_64-unknown-windows-msvc")
set(CMAKE_CXX_COMPILER_TARGET "x86_64-unknown-windows-msvc")
set(CMAKE_ASM_COMPILER_TARGET "x86_64-unknown-windows-msvc")
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(ARCH "x86_64")
set(TARGET "x86_64-unknown-windows-msvc")
