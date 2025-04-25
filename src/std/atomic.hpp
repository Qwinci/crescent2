#pragma once

template<typename T>
inline T atomic_load(T* ptr, int memory_order) {
	return __atomic_load_n(ptr, memory_order);
}

template<typename T>
inline void atomic_store(T* ptr, T value, int memory_order) {
	__atomic_store_n(ptr, value, memory_order);
}

template<typename T>
inline bool atomic_compare_exchange(
	T* ptr,
	T* expected,
	T desired,
	bool weak,
	int success_order,
	int failure_order) {
	return __atomic_compare_exchange_n(ptr, expected, desired, weak, success_order, failure_order);
}
