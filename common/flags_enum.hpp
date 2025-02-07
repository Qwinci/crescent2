#pragma once
#include <hz/utility.hpp>

#define FLAGS_ENUM(Name) \
constexpr Name operator|(Name a, Name b) { \
	return static_cast<Name>(hz::to_underlying(a) | hz::to_underlying(b)); \
} \
\
constexpr Name& operator|=(Name& a, Name b) { \
	a = a | b; \
	return a; \
} \
\
constexpr bool operator&(Name a, Name b) { \
	return hz::to_underlying(a) & hz::to_underlying(b); \
} \
\
constexpr Name& operator&=(Name& a, Name b) { \
	a = static_cast<Name>(hz::to_underlying(a) & hz::to_underlying(b)); \
	return a; \
} \
\
constexpr Name operator~(Name a) { \
	return static_cast<Name>(~hz::to_underlying(a)); \
} \
namespace __ ## name {} \
using namespace __ ## name
