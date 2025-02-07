LIB(kernel32
	src/dummy.cpp
	src/console.cpp
)
target_link_options(kernel32 PRIVATE -nostdlib -Wl,/noentry)
