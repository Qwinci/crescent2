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

hz::result<std::shared_ptr<VNode>, int> vfs_create(
	std::shared_ptr<VNode> root,
	kstd::wstring_view path,
	FileType type,
	bool create_all) {
	if (!root) {
		root = ROOT_VFS->get_root();
	}

	auto node = std::move(root);

	usize component_count = 0;
	path_for_each_component(path, [&](kstd::wstring_view component) {
		++component_count;
		return true;
	});

	usize i = 0;
	hz::result<std::shared_ptr<VNode>, int> res {hz::error(0)};
	path_for_each_component(path, [&](kstd::wstring_view component) {
		++i;

		auto next = node->lookup(component);
		if (!next) {
			bool last = i == component_count;
			FileType create_type;
			if (last) {
				create_type = type;
			}
			else if (create_all) {
				create_type = FileType::Directory;
			}
			else {
				return false;
			}

			auto tmp_res = node->create(component, create_type);
			if (tmp_res) {
				if (!last) {
					next = std::move(tmp_res).value();
				}
				else {
					res = std::move(tmp_res);
					return true;
				}
			}
			else {
				return false;
			}
		}

		node = std::move(next);
		return true;
	});

	return res;
}

std::unique_ptr<Vfs> ROOT_VFS;
