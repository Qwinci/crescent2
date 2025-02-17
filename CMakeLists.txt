cmake_minimum_required(VERSION 3.29)
project(crescent C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_SUFFIX .dll)
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_STATIC_LIBRARY_SUFFIX .lib)
set(CMAKE_SKIP_RPATH ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

set(COMMON_CFLAGS
	-Wall -Wextra -Werror=switch -Werror=return-type
	-fno-threadsafe-statics
	-fno-stack-protector -fno-omit-frame-pointer
	-ffreestanding -fno-strict-aliasing -fno-eliminate-unused-debug-types
	-Wno-invalid-offsetof -Wno-unused-parameter -Wno-unused-const-variable
	-g3 -ggdb -fvisibility=hidden #-finstrument-functions
)
set(COMMON_CXXFLAGS
	${COMMON_CFLAGS}
	-nostdinc++ -fno-exceptions -fno-rtti -fsized-deallocation
	-fvisibility-inlines-hidden
)

add_executable(crescent
	src/main.cpp
)
target_include_directories(crescent PRIVATE src src/std common include)
target_compile_options(crescent PRIVATE
	$<$<COMPILE_LANGUAGE:CXX>: ${COMMON_CXXFLAGS}>
	$<$<COMPILE_LANGUAGE:C>: ${COMMON_CFLAGS}>
	--target=${ARCH}-unknown-windows-elf
	-Wthread-safety-analysis -Werror=thread-safety-analysis
)
target_link_options(crescent PRIVATE
	-nostdlib
	--target=${ARCH}-unknown-linux-gnu
	-T ${PROJECT_SOURCE_DIR}/lds/${ARCH}.ld
	-Wl,--export-dynamic,--hash-style=sysv
)

if(ARCH STREQUAL "x86_64")
	include(uACPI/uacpi.cmake)

	target_sources(crescent PRIVATE ${UACPI_SOURCES})
	target_include_directories(crescent PRIVATE ${UACPI_INCLUDES})
	target_compile_definitions(crescent PRIVATE UACPI_OVERRIDE_LIBC UACPI_SIZED_FREES)
endif()

add_subdirectory(common)
add_subdirectory(hzutils)

target_link_libraries(crescent PRIVATE common_elf hzutils)

#add_subdirectory(loader)
add_subdirectory(src)

set(FILES
	${PROJECT_SOURCE_DIR}/limine.conf
	$<TARGET_FILE:crescent>
	initramfs.tar
)

set(QEMU_FLAGS -m 4G -M q35
	-smp 1
	-cpu qemu64,+umip,+smep,+smap
	#-serial stdio
	-no-reboot -no-shutdown
	-debugcon stdio
)

find_program(XORRISO xorriso REQUIRED)

add_custom_command(OUTPUT image.iso DEPENDS ${FILES} crescent
	COMMAND cmake -E copy_if_different ${FILES} ${PROJECT_BINARY_DIR}/iso_root
	COMMAND ${XORRISO} -as mkisofs -b limine-bios-cd.bin -no-emul-boot
	-boot-load-size 4 -boot-info-table --efi-boot limine-uefi-cd.bin
	--efi-boot-part --efi-boot-image --protective-msdos-label
	${PROJECT_BINARY_DIR}/iso_root -o ${PROJECT_BINARY_DIR}/image.iso
	-quiet &> /dev/null VERBATIM USES_TERMINAL
)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/limine")
	execute_process(
		COMMAND git clone https://github.com/limine-bootloader/limine
		-b v8.x-binary --depth=1 "${CMAKE_BINARY_DIR}/limine"
	)
	execute_process(COMMAND mkdir -p ${PROJECT_BINARY_DIR}/iso_root)
	execute_process(COMMAND cmake -E copy_if_different
		${PROJECT_BINARY_DIR}/limine/limine-bios.sys
		${PROJECT_BINARY_DIR}/limine/limine-bios-cd.bin
		${PROJECT_BINARY_DIR}/limine/limine-uefi-cd.bin
		${PROJECT_BINARY_DIR}/iso_root
	)
endif()

add_subdirectory(libs)
add_subdirectory(apps)
add_subdirectory(drivers)

include(CMakeOptions.cmake)

add_custom_command(OUTPUT initramfs.tar
	COMMAND tar -cf initramfs.tar -C "${CMAKE_BINARY_DIR}/sysroot" .
	DEPENDS ${APPS} ${LIBS} ${DRIVERS}
)

add_custom_target(run
	COMMAND qemu-system-x86_64 -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso ${QEMU_FLAGS}
	DEPENDS image.iso USES_TERMINAL VERBATIM
)

add_custom_target(run-kvm
	COMMAND qemu-system-x86_64 -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso ${QEMU_FLAGS} -enable-kvm
		-cpu host,migratable=off,tsc-frequency=1000000000
	DEPENDS image.iso USES_TERMINAL VERBATIM
)

add_custom_target(debug
	COMMAND qemu-system-x86_64 -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso -s -S ${QEMU_FLAGS}
	DEPENDS image.iso USES_TERMINAL VERBATIM
)
