add_library(aux_klib STATIC
	src/main.cpp
)
target_include_directories(aux_klib PUBLIC include)

target_compile_options(aux_klib PRIVATE
	-Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti
)
target_include_directories(aux_klib PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_include_directories(aux_klib PRIVATE "${PROJECT_SOURCE_DIR}/drivers")
target_link_options(aux_klib PRIVATE
	-nostdlib
	-Wl,/noentry
)
