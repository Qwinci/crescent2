#include "ntdef.h"
#include "mem/mem.hpp"

enum class MEMORY_CACHING_TYPE {
	NonCached,
	Cached,
	WriteCombined
};

NTAPI extern "C" PVOID MmMapIoSpace(
	PHYSICAL_ADDRESS addr,
	SIZE_T num_of_bytes,
	MEMORY_CACHING_TYPE cache_type) {
	return to_virt<void>(static_cast<usize>(addr.QuadPart));
}
