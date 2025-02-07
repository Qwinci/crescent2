add_library(kernel_stubs SHARED
	src/main.cpp
)

set_target_properties(kernel_stubs PROPERTIES
	POSITION_INDEPENDENT_CODE OFF
	NO_SONAME ON
)
target_compile_options(kernel_stubs PRIVATE
	-Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti
	--target=${TARGET}
)
target_include_directories(kernel_stubs PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_include_directories(kernel_stubs PRIVATE "${PROJECT_SOURCE_DIR}/drivers")
target_link_options(kernel_stubs PRIVATE
	--target=${TARGET}
	-nostdlib
	-Wl,/noentry
)
