target_include_directories(crescent PRIVATE include)
target_sources(crescent PRIVATE
	arch_except.cpp
	arch_sched.cpp
	context.cpp
	context.S
	smp.cpp
	start.cpp
)

add_subdirectory(dev)
add_subdirectory(interrupts)
add_subdirectory(loader)
add_subdirectory(mem)
