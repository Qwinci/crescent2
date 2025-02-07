#include "callback_producer.hpp"

void CallbackProducer::add_callback(kstd::small_function<void()> callback) {
	callbacks.push_back(std::move(callback));
}

void CallbackProducer::signal_all() {
	for (auto& callback : callbacks) {
		callback();
	}
}
