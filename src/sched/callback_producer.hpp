#pragma once
#include "types.hpp"
#include "functional.hpp"
#include "vector.hpp"

struct CallbackProducer {
	void add_callback(kstd::small_function<void()> callback);

	void signal_all();

private:
	kstd::vector<kstd::small_function<void()>> callbacks {};
};
