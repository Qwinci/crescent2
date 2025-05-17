cmake_minimum_required(VERSION 3.29)
project(crescent C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
set(CMAKE_SKIP_RPATH ON)
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_CXX_STANDARD_LIBRARIES "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_COMPILE_OPTIONS_MSVC_DEBUG_INFORMATION_FORMAT_Embedded -g -gdwarf-4)
set(CMAKE_ASM_COMPILE_OPTIONS_MSVC_DEBUG_INFORMATION_FORMAT_Embedded -g -gdwarf-4)
set(CMAKE_CXX_COMPILE_OPTIONS_MSVC_DEBUG_INFORMATION_FORMAT_Embedded -g -gdwarf-4)

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

set(COMMON_CFLAGS
	-Wall -Wextra -Werror=switch -Werror=return-type
	-fno-threadsafe-statics
	-fno-stack-protector -fno-omit-frame-pointer
	-ffreestanding -fno-strict-aliasing
	-Wno-invalid-offsetof -Wno-unused-parameter -Wno-unused-const-variable -Wno-unused-function
	-g3 -ggdb #-finstrument-functions
	-masm=intel
)
set(COMMON_CXXFLAGS
	${COMMON_CFLAGS}
	-nostdinc++ -fno-exceptions -fno-rtti -fsized-deallocation
	-funwind-tables
)

check_c_compiler_flag(-fno-eliminate-unused-debug-types COMPILER_SUPPORTS_NO_ELIMINATE_UNUSED_DEBUG_TYPES)
check_cxx_compiler_flag(-fasync-exceptions COMPILER_SUPPORTS_ASYNC_EXCEPTIONS)
check_cxx_compiler_flag(-Wthread-safety COMPILER_SUPPORTS_THREAD_SAFETY_WARNING)

if(COMPILER_SUPPORTS_NO_ELIMINATE_UNUSED_DEBUG_TYPES)
	list(APPEND COMMON_CFLAGS -fno-eliminate-unused-debug-types)
endif()

if(COMPILER_SUPPORTS_ASYNC_EXCEPTIONS)
	list(APPEND COMMON_CXXFLAGS -fasync-exceptions)
endif()

add_subdirectory(common)
add_subdirectory(hzutils)

add_executable(crescent
	src/main.cpp
)

set_target_properties(crescent PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/kernel.def)

target_include_directories(crescent PRIVATE src src/std common include)
target_compile_definitions(crescent PRIVATE NTAPI=__declspec\(dllexport\))

target_compile_options(crescent PRIVATE
	$<$<COMPILE_LANGUAGE:CXX>: ${COMMON_CXXFLAGS}>
	$<$<COMPILE_LANGUAGE:C>: ${COMMON_CFLAGS}>
	$<$<COMPILE_LANGUAGE:ASM>: ${COMMON_CFLAGS}>
)

if(COMPILER_SUPPORTS_THREAD_SAFETY_WARNING)
	target_compile_options(crescent PRIVATE -Wthread-safety -Werror=thread-safety)
endif()

target_link_options(crescent PRIVATE
	-nostdlib
	-Wl,/entry:_start,/base:0xFFFFFFFF80000000,/ignore:longsections,/def:${CMAKE_SOURCE_DIR}/kernel.def
)

target_link_libraries(crescent PRIVATE common hzutils)

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
	#-d int
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
		-b v9.x-binary --depth=1 "${CMAKE_BINARY_DIR}/limine"
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
