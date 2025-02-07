set(CONFIG_ACPI_IMPL "uacpi" CACHE STRING "acpi implementation to use")
set_property(CACHE CONFIG_ACPI_IMPL PROPERTY STRINGS uacpi qacpi)

option(CONFIG_LAZY_IRQL "Use lazy irql mechanism" ON)

if(CONFIG_ACPI_IMPL STREQUAL "uacpi")
	target_compile_definitions(crescent PRIVATE CONFIG_ACPI_UACPI)
elseif(CONFIG_ACPI_IMPL STREQUAL "qacpi")
	target_compile_definitions(crescent PRIVATE CONFIG_ACPI_QACPI)
else()
	message(FATAL_ERROR "Unsupported acpi implementation ${CONFIG_ACPI_IMPL}")
endif()

configure_file(config.hpp.in config.hpp @ONLY)
target_include_directories(crescent PRIVATE "${CMAKE_BINARY_DIR}")
