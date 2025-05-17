#pragma once

#include <hz/type_traits.hpp>

enum class memory_order {
	relaxed = __ATOMIC_RELAXED,
	acquire = __ATOMIC_ACQUIRE,
	release = __ATOMIC_RELEASE,
	acq_rel = __ATOMIC_ACQ_REL,
	seq_cst = __ATOMIC_SEQ_CST
};

template<typename T>
inline T atomic_load(T* ptr, memory_order order) {
	return __atomic_load_n(ptr, static_cast<int>(order));
}

template<typename T, typename V = T>
inline void atomic_store(T* ptr, V value, memory_order order) {
	__atomic_store_n(ptr, value, static_cast<int>(order));
}

template<typename T, typename V = T>
inline V atomic_fetch_add(T* ptr, V value, memory_order order) {
	return __atomic_fetch_add(ptr, value, static_cast<int>(order));
}

template<typename T>
inline bool atomic_compare_exchange(
	T* ptr,
	T* expected,
	T desired,
	bool weak,
	memory_order success_order,
	memory_order failure_order) {
	return __atomic_compare_exchange_n(
		ptr,
		expected,
		desired,
		weak,
		static_cast<int>(success_order),
		static_cast<int>(failure_order));
}
