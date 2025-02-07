add_library(aux_klib STATIC
	src/main.cpp
)
target_include_directories(aux_klib PUBLIC include)

set_target_properties(aux_klib PROPERTIES
	POSITION_INDEPENDENT_CODE OFF
)
target_compile_options(aux_klib PRIVATE
	-Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti
	--target=${TARGET}
)
target_include_directories(aux_klib PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_include_directories(aux_klib PRIVATE "${PROJECT_SOURCE_DIR}/drivers")
target_link_options(aux_klib PRIVATE
	--target=${TARGET}
	-nostdlib
	-Wl,/noentry
)
