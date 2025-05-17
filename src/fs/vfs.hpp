#pragma once
#include "shared_ptr.hpp"
#include "string_view.hpp"
#include "unique_ptr.hpp"
#include <hz/result.hpp>

enum class FileType {
	File,
	Directory
};

struct Stat {
	usize size;
};

struct VNode {
	virtual ~VNode() = default;

	virtual std::shared_ptr<VNode> lookup(kstd::wstring_view component) = 0;

	virtual hz::result<usize, int> read(void* data, usize offset, usize size) = 0;
	virtual hz::result<usize, int> write(const void* data, usize offset, usize size) = 0;
	virtual int truncate(usize new_size) = 0;

	virtual hz::result<std::shared_ptr<VNode>, int> create(kstd::wstring_view name, FileType type) = 0;
	virtual hz::result<Stat, int> stat() = 0;
};

struct Vfs {
	virtual ~Vfs() = default;

	virtual std::shared_ptr<VNode> get_root() = 0;
};

std::shared_ptr<VNode> vfs_lookup(std::shared_ptr<VNode> root, kstd::wstring_view path);
hz::result<std::shared_ptr<VNode>, int> vfs_create(
	std::shared_ptr<VNode> root,
	kstd::wstring_view path,
	FileType type,
	bool create_all);

extern std::unique_ptr<Vfs> ROOT_VFS;
