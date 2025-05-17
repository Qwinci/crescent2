include(${CMAKE_SOURCE_DIR}/uACPI/uacpi.cmake)

DRIVER(uacpi
	src/glue.cpp
	src/main.cpp
	${UACPI_SOURCES}
)
target_include_directories(uacpi PRIVATE ${UACPI_INCLUDES} src)
target_compile_definitions(uacpi PRIVATE UACPI_OVERRIDE_LIBC)
