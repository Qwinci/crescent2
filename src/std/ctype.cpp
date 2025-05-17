#include "ctype.hpp"

NTAPI extern "C" int tolower(int ch) {
	return ch >= 'A' && ch <= 'Z' ? (ch | 1 << 5) : ch;
}

NTAPI extern "C" int toupper(int ch) {
	return ch >= 'a' && ch <= 'z' ? (ch & ~(1 << 5)) : ch;
}
