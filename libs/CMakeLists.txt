execute_process(
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/sysroot/libs"
)

set(LIBS "" CACHE INTERNAL "")

macro(LIB NAME)
	add_library(${NAME} SHARED ${ARGN})
	target_compile_options(${NAME} PRIVATE
		-Wall -Wextra
	)
	target_link_options(${NAME} PRIVATE -Wl,/ignore:longsections)
	target_include_directories(${NAME} PRIVATE "${PROJECT_SOURCE_DIR}/include")

	add_custom_command(TARGET ${NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:${NAME}> $<TARGET_LINKER_FILE:${NAME}> "${CMAKE_BINARY_DIR}/sysroot/libs"
	)
	set(LIBS "${LIBS};${NAME}" CACHE INTERNAL "")
endmacro()

macro(STATIC_LIB NAME)
	add_library(${NAME} STATIC ${ARGN})
	target_compile_options(${NAME} PRIVATE
		-Wall -Wextra
		-I "${PROJECT_SOURCE_DIR}/include"
	)

	add_custom_command(TARGET ${NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:${NAME}> "${CMAKE_BINARY_DIR}/sysroot/libs"
	)
	set(LIBS "${LIBS};${NAME}" CACHE INTERNAL "")
endmacro()

add_subdirectory(cmt)
add_subdirectory(kernel32)
add_subdirectory(ntdll)
add_subdirectory(oldnames)
