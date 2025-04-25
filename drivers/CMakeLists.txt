execute_process(
	COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/sysroot/drivers"
)

set(DRIVERS "" CACHE INTERNAL "")

macro(DRIVER NAME)
	add_executable(${NAME} ${ARGN})
	target_compile_options(${NAME} PRIVATE ${COMMON_CXXFLAGS})
	target_include_directories(
		${NAME}
		PRIVATE
		"${PROJECT_SOURCE_DIR}/include"
		"${PROJECT_SOURCE_DIR}/drivers"
		"${PROJECT_SOURCE_DIR}/drivers/crt"
	)
	target_link_options(${NAME} PRIVATE
		-nostdlib
		-Wl,/driver,/subsystem:native,/entry:DriverEntry,/ignore:longsections
		crescent.lib
	)
	add_dependencies(${NAME} crescent)
	set_target_properties(${NAME} PROPERTIES SUFFIX ".sys")
	add_custom_command(TARGET ${NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${NAME}> "${CMAKE_BINARY_DIR}/sysroot/drivers"
	)
	set(DRIVERS "${DRIVERS};${NAME}" CACHE INTERNAL "")
endmacro()

add_subdirectory(aux_klib)
add_subdirectory(pci)
add_subdirectory(rtl8169)
add_subdirectory(uacpi)
