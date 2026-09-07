#include "leaf/core/zip.hpp"

#include <zip.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace lf::detail {
	class zip_source_bridge {
	  public:
		explicit zip_source_bridge(fs::file source);
		~zip_source_bridge();

		static zip_int64_t callback(void* user, void* data, zip_uint64_t length, zip_source_cmd_t command);
		zip_int64_t dispatch(void* data, zip_uint64_t length, zip_source_cmd_t command);
		zip_int64_t read(void* data, zip_uint64_t length);
		zip_int64_t seek(const zip_source_args_seek_t& arguments);
		zip_int64_t stat(void* data, zip_uint64_t length);
		zip_int64_t error_data(void* data, zip_uint64_t length);
		zip_int64_t supported_commands() const;
		void set_error(int value);

	  private:
		fs::file package_file;
		optional<fs::stream> input;
		u64 size = 0;
		int zip_error = ZIP_ER_OK;
	};

	class zip_archive {
	  public:
		static report<zip_t*> open(fs::file source);
	};

	struct zip_entry {
		u64 index;
		bool directory;
		u64 size;
		optional<timepoint> modification;
	};

	class zip_stream {
	  public:
		zip_stream(zip_t* archive, zip_file_t* file, u64 size);
		zip_stream(zip_stream&& other) noexcept;
		zip_stream& operator=(zip_stream&& other) noexcept;
		zip_stream(const zip_stream&) = delete;
		zip_stream& operator=(const zip_stream&) = delete;
		~zip_stream();

		static report<zip_stream> open(fs::file source, u64 index, u64 size);
		bool readable() const noexcept;
		bool writable() const noexcept;
		bool seekable() const noexcept;
		bool resizable() const noexcept;
		u64 position() const noexcept;
		report<usize> read(span<u08> destination);

	  private:
		void close() noexcept;

		zip_t* archive;
		zip_file_t* file;
		u64 size_value;
		u64 offset;
	};

	class zip_file {
	  public:
		zip_file(fs::file source, u64 index, u64 size, optional<timepoint> modification);

		fs::access_mode access() const noexcept;
		report<fs::node_status> status() const;
		report<fs::stream> open(fs::file_access requested) const;

	  private:
		fs::file package_file;
		u64 entry_index;
		u64 size_value;
		optional<timepoint> modification_value;
	};

	class zip_volume {
	  public:
		zip_volume(fs::file source, std::unordered_map<string, zip_entry> entries);

		fs::access_mode access() const noexcept;
		report<fs::node_status> status(const fs::path& location) const;
		report<fs::file> open_file(const fs::path& location, fs::open_options options);
		report<vector<fs::directory_entry>> list(const fs::path& location) const;

		static report<fs::volume> decode(fs::file source);
		static report<string> normalized_entry_name(const ch08* name, bool directory);
		static optional<timepoint> entry_time(const zip_stat_t& stat);

	  private:
		report<void> add_entry(string name, zip_entry entry);
		report<void> add_parents(const fs::path& location);

		fs::file package_file;
		std::unordered_map<string, zip_entry> entries;
	};

	class zip_decoder_backend {
	  public:
		string_view name() const noexcept;
		report<fs::volume> open(fs::file source) const;
	};

	zip_source_bridge::zip_source_bridge(fs::file source) : package_file(std::move(source)) {}
	zip_source_bridge::~zip_source_bridge() = default;

	zip_int64_t zip_source_bridge::callback(void* user, void* data, zip_uint64_t length, zip_source_cmd_t command) {
		return static_cast<zip_source_bridge*>(user)->dispatch(data, length, command);
	}

	zip_int64_t zip_source_bridge::dispatch(void* data, zip_uint64_t length, zip_source_cmd_t command) {
		if (command == ZIP_SOURCE_OPEN) {
			report<fs::node_status> file_status = package_file.status();
			if (!file_status) {
				set_error(ZIP_ER_READ);
				return -1;
			}
			size = file_status->size();
			report<fs::stream> opened = package_file.open(fs::file_access::read);
			if (!opened || !opened->seekable()) {
				set_error(ZIP_ER_SEEK);
				return -1;
			}
			input.emplace(std::move(*opened));
			return 0;
		}
		if (command == ZIP_SOURCE_READ) {
			return read(data, length);
		}
		if (command == ZIP_SOURCE_CLOSE) {
			input.reset();
			return 0;
		}
		if (command == ZIP_SOURCE_STAT) {
			return stat(data, length);
		}
		if (command == ZIP_SOURCE_ERROR) {
			return error_data(data, length);
		}
		if (command == ZIP_SOURCE_SEEK) {
			if (length != sizeof(zip_source_args_seek_t)) {
				set_error(ZIP_ER_INVAL);
				return -1;
			}
			return seek(*static_cast<const zip_source_args_seek_t*>(data));
		}
		if (command == ZIP_SOURCE_TELL) {
			if (!input) {
				set_error(ZIP_ER_INVAL);
				return -1;
			}
			return static_cast<zip_int64_t>(input->position());
		}
		if (command == ZIP_SOURCE_SUPPORTS) {
			return supported_commands();
		}
		if (command == ZIP_SOURCE_FREE) {
			delete this;
			return 0;
		}
		set_error(ZIP_ER_OPNOTSUPP);
		return -1;
	}

	zip_int64_t zip_source_bridge::read(void* data, zip_uint64_t length) {
		if (!input || length > static_cast<zip_uint64_t>(std::numeric_limits<usize>::max())) {
			set_error(ZIP_ER_READ);
			return -1;
		}
		report<usize> count = input->read(span<u08>(static_cast<u08*>(data), static_cast<usize>(length)));
		if (!count) {
			set_error(ZIP_ER_READ);
			return -1;
		}
		return static_cast<zip_int64_t>(*count);
	}

	zip_int64_t zip_source_bridge::seek(const zip_source_args_seek_t& arguments) {
		if (!input) {
			set_error(ZIP_ER_INVAL);
			return -1;
		}
		fs::seek_origin origin = fs::seek_origin::begin;
		if (arguments.whence == SEEK_CUR) {
			origin = fs::seek_origin::current;
		}
		if (arguments.whence == SEEK_END) {
			origin = fs::seek_origin::end;
		}
		report<u64> result = input->seek(arguments.offset, origin);
		if (!result) {
			set_error(ZIP_ER_SEEK);
			return -1;
		}
		return 0;
	}

	zip_int64_t zip_source_bridge::stat(void* data, zip_uint64_t length) {
		if (length < sizeof(zip_stat_t)) {
			set_error(ZIP_ER_INVAL);
			return -1;
		}
		zip_stat_t* result = static_cast<zip_stat_t*>(data);
		zip_stat_init(result);
		result->valid = ZIP_STAT_SIZE;
		result->size = size;
		return sizeof(zip_stat_t);
	}

	zip_int64_t zip_source_bridge::error_data(void* data, zip_uint64_t length) {
		if (length < sizeof(int) * 2) {
			return -1;
		}
		int* result = static_cast<int*>(data);
		result[0] = zip_error;
		result[1] = 0;
		return sizeof(int) * 2;
	}

	zip_int64_t zip_source_bridge::supported_commands() const {
		return zip_source_make_command_bitmap(ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE, ZIP_SOURCE_STAT, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, ZIP_SOURCE_SEEK, ZIP_SOURCE_TELL, ZIP_SOURCE_SUPPORTS, -1);
	}

	void zip_source_bridge::set_error(int value) {
		zip_error = value;
	}

	report<zip_t*> zip_archive::open(fs::file source) {
		zip_error_t error_value;
		zip_error_init(&error_value);
		auto bridge = new zip_source_bridge(std::move(source));
		zip_source_t* source_handle = zip_source_function_create(zip_source_bridge::callback, bridge, &error_value);
		if (source_handle == nullptr) {
			delete bridge;
			return unexpected(error(fs::error_code::io_error, zip_error_strerror(&error_value)));
		}
		zip_t* archive = zip_open_from_source(source_handle, ZIP_RDONLY, &error_value);
		if (archive == nullptr) {
			zip_source_free(source_handle);
			return unexpected(error(fs::error_code::corrupt_data, zip_error_strerror(&error_value)));
		}
		return archive;
	}

	zip_stream::zip_stream(zip_t* archive_value, zip_file_t* file_value, u64 size) : archive(archive_value), file(file_value), size_value(size), offset(0) {}
	zip_stream::zip_stream(zip_stream&& other) noexcept : archive(other.archive), file(other.file), size_value(other.size_value), offset(other.offset) {
		other.archive = nullptr;
		other.file = nullptr;
	}
	zip_stream& zip_stream::operator=(zip_stream&& other) noexcept {
		if (this != &other) {
			close();
			archive = other.archive;
			file = other.file;
			size_value = other.size_value;
			offset = other.offset;
			other.archive = nullptr;
			other.file = nullptr;
		}
		return *this;
	}
	zip_stream::~zip_stream() { close(); }

	report<zip_stream> zip_stream::open(fs::file source, u64 index, u64 size) {
		report<zip_t*> archive = zip_archive::open(std::move(source));
		if (!archive) {
			return unexpected(archive.error());
		}
		zip_file_t* file = zip_fopen_index(*archive, index, 0);
		if (file == nullptr) {
			const string message(zip_strerror(*archive));
			zip_close(*archive);
			return unexpected(error(fs::error_code::corrupt_data, message));
		}
		return zip_stream(*archive, file, size);
	}

	bool zip_stream::readable() const noexcept { return true; }
	bool zip_stream::writable() const noexcept { return false; }
	bool zip_stream::seekable() const noexcept { return false; }
	bool zip_stream::resizable() const noexcept { return false; }
	u64 zip_stream::position() const noexcept { return offset; }
	report<usize> zip_stream::read(span<u08> destination) {
		const zip_int64_t count = zip_fread(file, destination.data(), destination.size());
		if (count < 0) {
			return unexpected(error(fs::error_code::corrupt_data, zip_file_strerror(file)));
		}
		offset += static_cast<u64>(count);
		return static_cast<usize>(count);
	}
	void zip_stream::close() noexcept {
		if (file != nullptr) {
			zip_fclose(file);
			file = nullptr;
		}
		if (archive != nullptr) {
			zip_close(archive);
			archive = nullptr;
		}
	}

	zip_file::zip_file(fs::file source, u64 index, u64 size, optional<timepoint> modification)
		: package_file(std::move(source)), entry_index(index), size_value(size), modification_value(std::move(modification)) {}
	fs::access_mode zip_file::access() const noexcept { return fs::access_mode::read_only; }
	report<fs::node_status> zip_file::status() const { return fs::node_status(fs::node_type::file, size_value, access(), fs::file_times(optional<timepoint>(), modification_value)); }
	report<fs::stream> zip_file::open(fs::file_access requested) const {
		if (requested != fs::file_access::read) {
			return unexpected(error(fs::error_code::read_only, "ZIP entries are read-only"));
		}
		report<zip_stream> opened = zip_stream::open(package_file, entry_index, size_value);
		if (!opened) {
			return unexpected(opened.error());
		}
		return fs::stream(std::move(*opened));
	}

	zip_volume::zip_volume(fs::file source, std::unordered_map<string, zip_entry> values) : package_file(std::move(source)), entries(std::move(values)) {}
	fs::access_mode zip_volume::access() const noexcept { return fs::access_mode::read_only; }
	report<fs::node_status> zip_volume::status(const fs::path& location) const {
		if (location.empty()) {
			return fs::node_status(fs::node_type::directory, 0, access());
		}
		const auto entry = entries.find(string(location.text()));
		if (entry == entries.end()) {
			return unexpected(error(fs::error_code::not_found, "ZIP entry does not exist"));
		}
		if (entry->second.directory) {
			return fs::node_status(fs::node_type::directory, 0, access());
		}
		return fs::node_status(fs::node_type::file, entry->second.size, access(), fs::file_times(optional<timepoint>(), entry->second.modification));
	}
	report<fs::file> zip_volume::open_file(const fs::path& location, fs::open_options options) {
		if (options.access() != fs::file_access::read || options.disposition() != fs::file_disposition::open_existing) {
			return unexpected(error(fs::error_code::read_only, "ZIP entries are read-only"));
		}
		const auto entry = entries.find(string(location.text()));
		if (entry == entries.end()) {
			return unexpected(error(fs::error_code::not_found, "ZIP entry does not exist"));
		}
		if (entry->second.directory) {
			return unexpected(error(fs::error_code::not_a_file, "ZIP entry is a directory"));
		}
		return fs::file(zip_file(package_file, entry->second.index, entry->second.size, entry->second.modification));
	}
	report<vector<fs::directory_entry>> zip_volume::list(const fs::path& location) const {
		report<fs::node_status> node = status(location);
		if (!node) {
			return unexpected(node.error());
		}
		if (node->type() != fs::node_type::directory) {
			return unexpected(error(fs::error_code::not_a_directory, "ZIP entry is not a directory"));
		}
		const string prefix = location.empty() ? string() : string(location.text()) + "/";
		auto result = vector<fs::directory_entry>();
		std::unordered_set<string> names;
		for (const auto& [name, entry] : entries) {
			if (!string_view(name).starts_with(prefix)) {
				continue;
			}
			const string_view remainder = string_view(name).substr(prefix.size());
			if (remainder.empty()) {
				continue;
			}
			const usize separator = remainder.find('/');
			const string child(remainder.substr(0, separator));
			if (!names.emplace(child).second) {
				continue;
			}
			if (separator != string_view::npos) {
				result.emplace_back(child, fs::node_status(fs::node_type::directory, 0, access()));
			} else if (entry.directory) {
				result.emplace_back(child, fs::node_status(fs::node_type::directory, 0, access()));
			} else {
				result.emplace_back(child, fs::node_status(fs::node_type::file, entry.size, access(), fs::file_times(optional<timepoint>(), entry.modification)));
			}
		}
		std::sort(result.begin(), result.end(), [](const fs::directory_entry& lhs, const fs::directory_entry& rhs) {
			return lhs.name() < rhs.name();
		});
		return result;
	}

	report<fs::volume> zip_volume::decode(fs::file source) {
		report<zip_t*> archive = zip_archive::open(source);
		if (!archive) {
			return unexpected(archive.error());
		}
		std::unordered_map<string, zip_entry> entries;
		const zip_int64_t count = zip_get_num_entries(*archive, 0);
		if (count < 0) {
			const string message(zip_strerror(*archive));
			zip_close(*archive);
			return unexpected(error(fs::error_code::corrupt_data, message));
		}
		for (zip_uint64_t index = 0; index < static_cast<zip_uint64_t>(count); ++index) {
			zip_stat_t stat;
			zip_stat_init(&stat);
			if (zip_stat_index(*archive, index, 0, &stat) != 0 || stat.name == nullptr) {
				const string message(zip_strerror(*archive));
				zip_close(*archive);
				return unexpected(error(fs::error_code::corrupt_data, message));
			}
			const string_view raw_name(stat.name);
			const bool directory = raw_name.ends_with('/');
			report<string> normalized = normalized_entry_name(stat.name, directory);
			if (!normalized) {
				zip_close(*archive);
				return unexpected(normalized.error());
			}
			if (normalized->empty()) {
				continue;
			}
			zip_entry entry{ index, directory, stat.size, entry_time(stat) };
			auto [position, inserted] = entries.try_emplace(*normalized, entry);
			if (!inserted) {
				zip_close(*archive);
				return unexpected(error(fs::error_code::corrupt_data, "ZIP contains duplicate entries"));
			}
		}
		zip_volume volume(source, std::move(entries));
		auto names = vector<string>();
		names.reserve(volume.entries.size());
		for (const auto& [name, entry] : volume.entries) {
			names.emplace_back(name);
		}
		for (const string& name : names) {
			report<fs::path> location = fs::path::parse(name);
			if (!location) {
				zip_close(*archive);
				return unexpected(location.error());
			}
			report<void> parents = volume.add_parents(*location);
			if (!parents) {
				zip_close(*archive);
				return unexpected(parents.error());
			}
		}
		zip_close(*archive);
		return fs::volume(std::move(volume));
	}

	report<string> zip_volume::normalized_entry_name(const ch08* name, bool directory) {
		string text(name);
		if (directory) {
			text.pop_back();
		}
		if (text.empty()) {
			return string();
		}
		report<fs::path> parsed = fs::path::parse(text);
		if (!parsed || parsed->is_absolute()) {
			return unexpected(error(fs::error_code::corrupt_data, "ZIP entry path is invalid"));
		}
		return string(parsed->text());
	}

	optional<timepoint> zip_volume::entry_time(const zip_stat_t& stat) {
		if ((stat.valid & ZIP_STAT_MTIME) == 0) {
			return optional<timepoint>();
		}
		return timepoint::from_unix_epoch(duration::from_chrono(std::chrono::seconds(stat.mtime)));
	}

	report<void> zip_volume::add_entry(string name, zip_entry entry) {
		auto [position, inserted] = entries.try_emplace(std::move(name), entry);
		if (!inserted && position->second.directory != entry.directory) {
			return unexpected(error(fs::error_code::corrupt_data, "ZIP has file and directory path conflict"));
		}
		return {};
	}

	report<void> zip_volume::add_parents(const fs::path& location) {
		fs::path parent = location.parent();
		while (!parent.empty()) {
			report<void> added = add_entry(string(parent.text()), zip_entry{ 0, true, 0, optional<timepoint>() });
			if (!added) {
				return unexpected(added.error());
			}
			parent = parent.parent();
		}
		return {};
	}

	string_view zip_decoder_backend::name() const noexcept { return "zip"; }
	report<fs::volume> zip_decoder_backend::open(fs::file source) const { return zip_volume::decode(std::move(source)); }
} // namespace lf::detail

namespace lf {
	package_decoder zip_decoder() { return package_decoder(detail::zip_decoder_backend()); }
} // namespace lf
