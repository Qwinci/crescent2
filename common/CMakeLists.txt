set(COMMON_SRC
	fb_log.cpp
	font.cpp
	stdio.cpp
	cxx.cpp
	mem/mem.cpp
)

add_library(common_elf STATIC ${COMMON_SRC})
target_compile_options(common_elf PRIVATE
	${COMMON_CXXFLAGS}
	--target=${ARCH}-unknown-windows-elf
)
target_include_directories(common_elf PUBLIC .)
target_link_libraries(common_elf PRIVATE hzutils)

if(ARCH STREQUAL "x86_64")
	target_include_directories(common_elf PUBLIC arch/x86)
else()
	message(FATAL_ERROR "Unsupported architecture ${ARCH}")
endif()
