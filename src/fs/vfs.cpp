#include "vfs.hpp"
#include "vfs_internal.hpp"

std::shared_ptr<VNode> vfs_lookup(std::shared_ptr<VNode> root, kstd::wstring_view path) {
	if (!root) {
		root = ROOT_VFS->get_root();
	}

	auto node = std::move(root);

	if (!path_for_each_component(path, [&](kstd::wstring_view component) {
		auto next = node->lookup(component);
		if (!next) {
			return false;
		}

		node = std::move(next);
		return true;
	})) {
		return nullptr;
	}

	return node;
}

std::unique_ptr<Vfs> ROOT_VFS;
