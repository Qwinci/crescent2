# Crescent2 (potentially to be renamed)
## A kernel and an OS distribution aiming for NT compatibility
Crescent2 is an os distribution and a kernel aiming both for
driver and userspace program compatibility with Windows NT.

Currently it is missing pretty much everything, first the focus is on getting basic drivers
working and then some basic syscalls.

### Features
- Architectures supported: X86-64 (AArch64 support is planned)
- Uses [uACPI](https://github.com/uACPI/uACPI) aml interpreter that aims for NT compatibility

### Building
For building the kernel, apps and libs you need the following tools:
- Clang
- CMake
- Ninja (or Make, but the steps below assume Ninja is used)

#### Steps
Note: If you want to build to an another architecture then substitute the cmake toolchain path in the below command.

- `mkdir build`
- `cd build`
- `cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_x86_64_clang.cmake ..`
- `ninja`

Running the resulting image in Qemu:
- `ninja run`

