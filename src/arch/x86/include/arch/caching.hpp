#pragma once

enum class CacheMode {
	WriteBack,
	WriteCombine,
	WriteThrough,
	Uncached,
	None
};
