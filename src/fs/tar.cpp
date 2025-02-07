#include "tar.hpp"
#include "assert.hpp"
#include "vfs_internal.hpp"
#include "string.hpp"
#include "mem/mem.hpp"

struct TarHeader {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chk_sum[8];
	char type_flag;
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	char prefix[155];
};

static u64 parse_oct(const char* str) {
	u64 value = 0;
	for (; *str; ++str) {
		value *= 8;
		value += *str - '0';
	}
	return value;
}

void init_vfs_from_tar(Vfs& vfs, const void* data) {
	auto hdr = static_cast<const TarHeader*>(data);
	assert(kstd::string_view {hdr->magic, 5} == "ustar");

	while (true) {
		if (!*hdr->name) {
			break;
		}

		kstd::string_view name {hdr->name};

		auto size = parse_oct(hdr->size);

		auto node = vfs.get_root();
		path_for_each_component(name, [&](kstd::string_view component) {
			if (component != "." && component != "..") {
				kstd::wstring wide {};
				wide.reserve(component.size());
				for (auto c : component) {
					wide.push_back(c);
				}

				if (auto existing = node->lookup(wide)) {
					node = std::move(existing);
				}
				else {
					FileType type = FileType::Directory;
					bool last = component.end() == name.end() || (
						(name.ends_with('/') || name.ends_with('\\')) && component.end() == name.end() - 1);
					bool is_file = hdr->type_flag == '0';
					if (last && is_file) {
						type = FileType::File;
					}

					auto new_node_res = node->create(wide, type);
					assert(new_node_res);
					auto& new_node = new_node_res.value();

					if (last && is_file) {
						auto status = new_node->truncate(size);
						assert(status == 0);
						auto write_res = new_node->write(offset(hdr, const void*, 512), 0, size);
						assert(write_res);
						assert(write_res.value() == size);
					}

					if (!last) {
						node = std::move(new_node);
					}
				}
			}

			return true;
		});

		hdr = offset(hdr, const TarHeader*, 512 + ALIGNUP(size, 512));
	}
}
