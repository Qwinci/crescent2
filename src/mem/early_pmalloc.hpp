#pragma once
#include "types.hpp"

void early_pmalloc_add_region(usize base, usize size);
void early_pmalloc_finalize();
usize early_pmalloc();
