execute_process(
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/sysroot/apps"
)

set(APPS "" CACHE INTERNAL "")

macro(APP NAME)
	add_executable(${NAME} ${ARGN})
	target_compile_options(${NAME} PRIVATE
		-Wall -Wextra
		--target=${TARGET}
	)
	target_include_directories(${NAME} PRIVATE "${PROJECT_SOURCE_DIR}/include")
	target_link_options(${NAME} PRIVATE
		--target=${TARGET}
		-L "${CMAKE_BINARY_DIR}/sysroot/libs"
	)
	target_link_libraries(${NAME} PRIVATE kernel32.lib)
	add_dependencies(${NAME} libcmt oldnames kernel32)
	add_custom_command(TARGET ${NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${NAME}> "${CMAKE_BINARY_DIR}/sysroot/apps"
	)
	set(APPS "${APPS};${NAME}" CACHE INTERNAL "")
endmacro()

add_subdirectory(hello_world)
