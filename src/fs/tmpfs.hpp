#pragma once
#include "vfs.hpp"
#include "unique_ptr.hpp"

std::unique_ptr<Vfs> tmpfs_create();
