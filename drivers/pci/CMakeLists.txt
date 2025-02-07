DRIVER(pci
	src/main.cpp
)

target_link_libraries(pci PRIVATE aux_klib)
