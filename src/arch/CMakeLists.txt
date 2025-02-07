target_sources(crescent PRIVATE
	cpu.cpp
)

if(ARCH STREQUAL "x86_64")
	add_subdirectory(x86)
else()
	message(FATAL_ERROR "Unsupported architecture ${ARCH}")
endif()
