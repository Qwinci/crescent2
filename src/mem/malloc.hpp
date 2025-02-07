#pragma once
#include "types.hpp"

void* kmalloc(usize size);
void kfree(void* ptr, usize size);
