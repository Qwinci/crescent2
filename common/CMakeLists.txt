set(COMMON_SRC
	fb_log.cpp
	font.cpp
	stdio.cpp
	cxx.cpp
	mem/mem.cpp
)

add_library(common STATIC ${COMMON_SRC})
target_compile_options(common PRIVATE
	${COMMON_CXXFLAGS}
)
target_include_directories(common PUBLIC .)
target_link_libraries(common PRIVATE hzutils)

if(ARCH STREQUAL "x86_64")
	target_include_directories(common PUBLIC arch/x86)
else()
	message(FATAL_ERROR "Unsupported architecture ${ARCH}")
endif()
