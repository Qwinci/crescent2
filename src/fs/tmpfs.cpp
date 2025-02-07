#include "tmpfs.hpp"
#include "string.hpp"
#include "unordered_map.hpp"
#include "stdio.hpp"
#include "vector.hpp"
#include "cstring.hpp"

struct TmpfsNode : public VNode {
	explicit TmpfsNode(FileType new_type) : type {new_type} {}

	std::shared_ptr<VNode> lookup(kstd::wstring_view component) override {
		if (auto node = children.get(component)) {
			return *node;
		}
		else {
			return nullptr;
		}
	}

	hz::result<usize, int> read(void* data, usize offset, usize size) override {
		if (size > SIZE_MAX - offset) {
			panic("TmpfsNode::read: size overflow");
		}
		if (offset > content.size()) {
			panic("TmpfsNode::read: offset too big");
		}

		auto to_read = hz::min(content.size() - offset, size);
		memcpy(data, content.data() + offset, to_read);
		return hz::success<usize>(to_read);
	}

	hz::result<usize, int> write(const void* data, usize offset, usize size) override {
		if (size > SIZE_MAX - offset) {
			panic("TmpfsNode::write: size overflow");
		}
		if (offset > content.size()) {
			panic("TmpfsNode::write: offset too big");
		}

		auto to_write = hz::min(content.size() - offset, size);
		memcpy(content.data() + offset, data, to_write);
		return hz::success<usize>(to_write);
	}

	int truncate(usize new_size) override {
		content.resize(new_size);
		return 0;
	}

	hz::result<std::shared_ptr<VNode>, int> create(kstd::wstring_view name, FileType new_type) override {
		if (children.get(name)) {
			// todo
			panic("TmpfsNode::create(\"", name, "\"): file already exists");
		}

		auto node = std::make_shared<TmpfsNode>(new_type);
		children.insert(kstd::wstring {name}, node);
		return hz::success<std::shared_ptr<VNode>>(std::move(node));
	}

	hz::result<Stat, int> stat() override {
		return hz::success(Stat {
			.size = content.size()
		});
	}

	kstd::unordered_map<kstd::wstring, std::shared_ptr<TmpfsNode>> children;
	kstd::vector<u8> content;
	FileType type;
};

struct Tmpfs : public Vfs {
	Tmpfs() {
		root = std::make_shared<TmpfsNode>(FileType::Directory);
	}

	std::shared_ptr<VNode> get_root() override {
		return root;
	}

	std::shared_ptr<VNode> root;
};

std::unique_ptr<Vfs> tmpfs_create() {
	return std::make_unique<Tmpfs>();
}
