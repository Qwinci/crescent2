LIB(ntdll
	src/entry.cpp
	src/file.cpp
	src/mm.cpp
	src/ps.cpp
)
target_include_directories(ntdll PUBLIC include)
target_compile_definitions(ntdll PRIVATE NTAPI=[[gnu::naked,\ gnu::used]]\ __declspec\(dllexport\))
target_link_options(ntdll PRIVATE -nostdlib -Wl,/noentry,/ignore:longsections)
