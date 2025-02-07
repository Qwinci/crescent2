target_include_directories(crescent PRIVATE include)
target_sources(crescent PRIVATE
	arch_sched.cpp
	smp.cpp
	start.cpp
)

add_subdirectory(dev)
add_subdirectory(interrupts)
add_subdirectory(loader)
add_subdirectory(mem)
