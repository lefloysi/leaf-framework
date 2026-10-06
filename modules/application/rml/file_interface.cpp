#include "application/rml/file_interface.hpp"

#include "leaf/core/filesystem.hpp"
#include "leaf/core/format.hpp"
#include "leaf/core/logging.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace lf {
	Rml::FileHandle RmlFile::Open(const Rml::String& path) {
		report<fs::path> location = string_view(path).starts_with('/') ? fs::path::parse(path) : fs::path::parse(lf::format("/{}", path));
		if (!location) {
			log::Warning("[rml] invalid resource '{}': {}", path, location.error().message);
			return 0;
		}

		report<vector<u08>> bytes = fs::read_all(*location);
		if (!bytes) {
			log::Warning("[rml] failed to open resource '{}': {}", path, bytes.error().message);
			return 0;
		}

		return reinterpret_cast<Rml::FileHandle>(new File{ std::move(*bytes) });
	}

	void RmlFile::Close(Rml::FileHandle file) {
		delete reinterpret_cast<File*>(file);
	}

	size_t RmlFile::Read(void* buffer, size_t size, Rml::FileHandle file) {
		File* source = reinterpret_cast<File*>(file);
		if (!source || !buffer || size == 0) {
			return 0;
		}

		const usize amount = std::min(size, source->bytes.size() - source->cursor);
		std::memcpy(buffer, source->bytes.data() + source->cursor, amount);
		source->cursor += amount;
		return amount;
	}

	bool RmlFile::Seek(Rml::FileHandle file, long offset, int origin) {
		File* source = reinterpret_cast<File*>(file);
		if (!source) {
			return false;
		}

		i64 base = 0;
		switch (origin) {
		case SEEK_SET:
			break;
		case SEEK_CUR:
			base = static_cast<i64>(source->cursor);
			break;
		case SEEK_END:
			base = static_cast<i64>(source->bytes.size());
			break;
		default:
			return false;
		}

		const i64 target = base + static_cast<i64>(offset);
		if (target < 0 || target > static_cast<i64>(source->bytes.size())) {
			return false;
		}

		source->cursor = static_cast<usize>(target);
		return true;
	}

	size_t RmlFile::Tell(Rml::FileHandle file) {
		const File* source = reinterpret_cast<const File*>(file);
		return source ? source->cursor : 0;
	}
} // namespace lf
