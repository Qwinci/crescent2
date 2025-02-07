LIB(ntdll
	src/entry.cpp
)
target_link_options(ntdll PRIVATE -nostdlib)
