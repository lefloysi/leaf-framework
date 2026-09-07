#include "leaf/core/filesystem.hpp"

#include "leaf/core/singleton.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <shared_mutex>
#include <unordered_set>
#include <utility>
#include <variant>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace lf::fs {
	report<string> parse_path_text(string_view text, bool absolute) {
		if (text.empty()) {
			if (!absolute) {
				return string();
			}
			return unexpected(error(error_code::invalid_path, "virtual path is empty"));
		}
		if (text.find('\\') != string_view::npos || text.find('\0') != string_view::npos || text.find(':') != string_view::npos) {
			return unexpected(error(error_code::invalid_path, "virtual path contains an invalid separator"));
		}
		if (absolute != text.starts_with('/')) {
			return unexpected(error(error_code::invalid_path, "virtual path has the wrong root form"));
		}
		if (absolute && text == "/") {
			return string("/");
		}

		const usize start = absolute ? 1 : 0;
		if (start == text.size() || text.back() == '/') {
			return unexpected(error(error_code::invalid_path, "virtual path has an empty component"));
		}

		usize component_begin = start;
		for (usize index = start; index <= text.size(); ++index) {
			if (index != text.size() && text[index] != '/') {
				continue;
			}
			const string_view component = text.substr(component_begin, index - component_begin);
			if (component.empty() || component == "." || component == "..") {
				return unexpected(error(error_code::invalid_path, "virtual path has an invalid component"));
			}
			component_begin = index + 1;
		}
		return string(text);
	}

	void validate_component(string_view component) {
		if (component.empty() || component == "." || component == ".." || component.find('/') != string_view::npos || component.find('\\') != string_view::npos || component.find('\0') != string_view::npos) {
			throw runtime_exception("invalid filesystem path component");
		}
	}

	path::path() = default;
	path::path(const ch08* text) : path(path_view(text)) {}
	path::path(const string& text) : path(string_view(text)) {}
	path::path(string_view text) {
		report<path> parsed = parse(text);
		if (!parsed) {
			throw runtime_exception(parsed.error().message);
		}
		value = std::move(parsed->value);
	}
	path::path(path_view text) : value(text.text()) {}
	report<path> path::parse(string_view text) {
		report<string> parsed = parse_path_text(text, text.starts_with('/'));
		if (!parsed) {
			return unexpected(parsed.error());
		}
		path result;
		result.value = std::move(*parsed);
		return result;
	}
	bool path::is_root() const noexcept { return value == "/"; }
	bool path::is_absolute() const noexcept { return value.starts_with('/'); }
	bool path::empty() const noexcept { return value.empty(); }
	string_view path::text() const noexcept { return value; }
	string_view path::filename() const noexcept {
		if (is_root()) {
			return "";
		}
		return string_view(value).substr(value.find_last_of('/') + 1);
	}
	path path::parent() const {
		if (is_root() || empty()) { return *this; }
		const usize separator = value.find_last_of('/');
		if (separator == string::npos) { return path{}; }
		if (separator == 0) {
			return path{ "/" };
		}
		return path(value.substr(0, separator));
	}
	path path::append(path_view child) const {
		if (child.is_absolute() || empty()) { return child.owning(); }
		if (child.empty()) { return *this; }
		return path{ value + (is_root() ? "" : "/") + string(child.text()) };
	}
	bool path::operator==(const path& other) const noexcept { return value == other.value; }

	path_view::path_view(const path& text) noexcept : value(text.value) {}
	path_view::path_view(const ch08* text) : path_view(string_view(text)) {}
	path_view::path_view(const string& text) : path_view(string_view(text)) {}
	path_view::path_view(string_view text) {
		report<string> parsed = parse_path_text(text, text.starts_with('/'));
		if (!parsed) {
			throw runtime_exception(parsed.error().message);
		}
		value = text;
	}
	bool path_view::is_root() const noexcept { return value == "/"; }
	bool path_view::is_absolute() const noexcept { return value.starts_with('/'); }
	bool path_view::empty() const noexcept { return value.empty(); }
	string_view path_view::text() const noexcept { return value; }
	string_view path_view::filename() const noexcept {
		if (is_root()) {
			return "";
		}
		return value.substr(value.find_last_of('/') + 1);
	}
	path path_view::owning() const { return path(*this); }

	file_times::file_times(optional<timepoint> creation, optional<timepoint> modification, optional<timepoint> access)
		: creation_value(std::move(creation)), modification_value(std::move(modification)), access_value(std::move(access)) {}
	optional<timepoint> file_times::creation() const noexcept { return creation_value; }
	optional<timepoint> file_times::modification() const noexcept { return modification_value; }
	optional<timepoint> file_times::access() const noexcept { return access_value; }

	node_status::node_status(node_type type, u64 size, access_mode access, file_times times)
		: type_value(type), size_value(size), access_value(access), times_value(std::move(times)) {}
	node_type node_status::type() const noexcept { return type_value; }
	u64 node_status::size() const noexcept { return size_value; }
	access_mode node_status::access() const noexcept { return access_value; }
	const file_times& node_status::times() const noexcept { return times_value; }

	directory_entry::directory_entry(string name, node_status status)
		: name_value(std::move(name)), status_value(std::move(status)) {}
	string_view directory_entry::name() const noexcept { return name_value; }
	const node_status& directory_entry::status() const noexcept { return status_value; }

	open_options::open_options(file_access access, file_disposition disposition)
		: access_value(access), disposition_value(disposition) {}
	file_access open_options::access() const noexcept { return access_value; }
	file_disposition open_options::disposition() const noexcept { return disposition_value; }

	copy_options::copy_options(existing_action existing, recursion recursive, metadata_action metadata)
		: existing_value(existing), recursion_value(recursive), metadata_value(metadata) {}
	existing_action copy_options::existing() const noexcept { return existing_value; }
	recursion copy_options::recursive() const noexcept { return recursion_value; }
	metadata_action copy_options::metadata() const noexcept { return metadata_value; }

	move_options::move_options(existing_action existing, cross_volume_action cross_volume)
		: existing_value(existing), cross_volume_value(cross_volume) {}
	existing_action move_options::existing() const noexcept { return existing_value; }
	cross_volume_action move_options::cross_volume() const noexcept { return cross_volume_value; }

	native_volume_options::native_volume_options(access_mode access, missing_action missing, symlink_policy symlinks)
		: access_value(access), missing_value(missing), symlink_value(symlinks) {}
	access_mode native_volume_options::access() const noexcept { return access_value; }
	missing_action native_volume_options::missing() const noexcept { return missing_value; }
	symlink_policy native_volume_options::symlinks() const noexcept { return symlink_value; }

	atomic_write_options::atomic_write_options(existing_action existing, parent_action parents, durability durability)
		: existing_value(existing), parents_value(parents), durability_value(durability) {}
	existing_action atomic_write_options::existing() const noexcept { return existing_value; }
	parent_action atomic_write_options::parents() const noexcept { return parents_value; }
	durability atomic_write_options::persistence() const noexcept { return durability_value; }

	stream::stream(unique_ptr<detail::stream_backend> value) : backend(std::move(value)) {}
	stream::stream(stream&&) noexcept = default;
	stream& stream::operator=(stream&&) noexcept = default;
	stream::~stream() = default;
	bool stream::readable() const noexcept { return backend && backend->readable(); }
	bool stream::writable() const noexcept { return backend && backend->writable(); }
	bool stream::seekable() const noexcept { return backend && backend->seekable(); }
	bool stream::resizable() const noexcept { return backend && backend->resizable(); }
	u64 stream::position() const noexcept { return backend ? backend->position() : 0; }
	report<usize> stream::read(span<u08> destination) {
		if (!backend) {
			return unexpected(error(error_code::io_error, "stream is closed"));
		}
		return backend->read(destination);
	}
	report<void> stream::read_exact(span<u08> destination) {
		usize offset = 0;
		while (offset < destination.size()) {
			report<usize> result = read(destination.subspan(offset));
			if (!result) {
				return unexpected(result.error());
			}
			if (*result == 0) {
				return unexpected(error(error_code::end_of_file, "unexpected end of file"));
			}
			offset += *result;
		}
		return {};
	}
	report<usize> stream::write(span<const u08> source) {
		if (!backend) {
			return unexpected(error(error_code::io_error, "stream is closed"));
		}
		return backend->write(source);
	}
	report<void> stream::write_all(span<const u08> source) {
		usize offset = 0;
		while (offset < source.size()) {
			report<usize> result = write(source.subspan(offset));
			if (!result) {
				return unexpected(result.error());
			}
			if (*result == 0) {
				return unexpected(error(error_code::io_error, "stream made no write progress"));
			}
			offset += *result;
		}
		return {};
	}
	report<u64> stream::seek(i64 offset, seek_origin origin) {
		if (!backend) {
			return unexpected(error(error_code::io_error, "stream is closed"));
		}
		return backend->seek(offset, origin);
	}
	report<void> stream::resize(u64 size) {
		if (!backend) {
			return unexpected(error(error_code::io_error, "stream is closed"));
		}
		return backend->resize(size);
	}
	report<void> stream::flush() {
		if (!backend) {
			return unexpected(error(error_code::io_error, "stream is closed"));
		}
		return backend->flush();
	}

	file::file(std::shared_ptr<detail::file_backend> value) : backend(std::move(value)) {}
	file::file(const file&) noexcept = default;
	file::file(file&&) noexcept = default;
	file& file::operator=(const file&) noexcept = default;
	file& file::operator=(file&&) noexcept = default;
	file::~file() = default;
	access_mode file::access() const noexcept { return backend ? backend->access() : access_mode::read_only; }
	report<node_status> file::status() const {
		if (!backend) {
			return unexpected(error(error_code::not_found, "file is empty"));
		}
		return backend->status();
	}
	report<stream> file::open(file_access access) const {
		if (!backend) {
			return unexpected(error(error_code::not_found, "file is empty"));
		}
		return backend->open(access);
	}
	report<vector<u08>> file::read_all() const {
		return read_all(std::numeric_limits<usize>::max());
	}
	report<vector<u08>> file::read_all(usize maximum_size) const {
		report<node_status> file_status = status();
		if (!file_status) {
			return unexpected(file_status.error());
		}
		if (file_status->type() != node_type::file) {
			return unexpected(error(error_code::not_a_file, "node is not a file"));
		}
		if (file_status->size() > maximum_size) {
			return unexpected(error(error_code::file_too_large, "file exceeds requested read limit"));
		}
		auto bytes = vector<u08>(static_cast<usize>(file_status->size()));
		report<stream> opened = open(file_access::read);
		if (!opened) {
			return unexpected(opened.error());
		}
		report<void> read_result = opened->read_exact(span<u08>(bytes.data(), bytes.size()));
		if (!read_result) {
			return unexpected(read_result.error());
		}
		return bytes;
	}

	volume::volume(std::shared_ptr<detail::volume_backend> value) : backend(std::move(value)) {}
	volume::volume(const volume&) noexcept = default;
	volume::volume(volume&&) noexcept = default;
	volume& volume::operator=(const volume&) noexcept = default;
	volume& volume::operator=(volume&&) noexcept = default;
	volume::~volume() = default;
	access_mode volume::access() const noexcept { return backend ? backend->access() : access_mode::read_only; }

	namespace detail {
		class borrowed_stream {
		  public:
			borrowed_stream(const u08* read_data, u08* write_data, usize length);

			bool readable() const noexcept;
			bool writable() const noexcept;
			bool seekable() const noexcept;
			bool resizable() const noexcept;
			u64 position() const noexcept;

			report<usize> read(span<u08> destination);
			report<usize> write(span<const u08> source);
			report<u64> seek(i64 value, seek_origin origin);

		  private:
			const u08* readable_data;
			u08* writable_data;
			usize size;
			usize offset;
		};

		class borrowed_file {
		  public:
			borrowed_file(const u08* read_data, u08* write_data, usize length);

			access_mode access() const noexcept;
			report<node_status> status() const;
			report<stream> open(file_access requested) const;

		  private:
			const u08* readable_data;
			u08* writable_data;
			usize size_value;
		};

		class buffer_stream {
		  public:
			explicit buffer_stream(std::shared_ptr<vector<u08>> value);

			bool readable() const noexcept;
			bool writable() const noexcept;
			bool seekable() const noexcept;
			bool resizable() const noexcept;
			u64 position() const noexcept;

			report<usize> read(span<u08> destination);
			report<usize> write(span<const u08> source);
			report<u64> seek(i64 value, seek_origin origin);
			report<void> resize(u64 size);

		  private:
			std::shared_ptr<vector<u08>> bytes;
			usize offset;
		};

		borrowed_stream::borrowed_stream(const u08* read_data, u08* write_data, usize length)
			: readable_data(read_data), writable_data(write_data), size(length), offset(0) {}
		bool borrowed_stream::readable() const noexcept { return true; }
		bool borrowed_stream::writable() const noexcept { return writable_data != nullptr; }
		bool borrowed_stream::seekable() const noexcept { return true; }
		bool borrowed_stream::resizable() const noexcept { return false; }
		u64 borrowed_stream::position() const noexcept { return offset; }
		report<usize> borrowed_stream::read(span<u08> destination) {
			const usize count = std::min(destination.size(), size - offset);
			if (count > 0) {
				std::memcpy(destination.data(), readable_data + offset, count);
				offset += count;
			}
			return count;
		}
		report<usize> borrowed_stream::write(span<const u08> source) {
			if (writable_data == nullptr) {
				return unexpected(error(error_code::read_only, "borrowed bytes are const"));
			}
			const usize count = std::min(source.size(), size - offset);
			if (count > 0) {
				std::memcpy(writable_data + offset, source.data(), count);
				offset += count;
			}
			return count;
		}
		report<u64> borrowed_stream::seek(i64 value, seek_origin origin) {
			i64 base = 0;
			if (origin == seek_origin::current) {
				base = static_cast<i64>(offset);
			}
			if (origin == seek_origin::end) {
				base = static_cast<i64>(size);
			}
			const i64 target = base + value;
			if (target < 0 || target > static_cast<i64>(size)) {
				return unexpected(error(error_code::out_of_range, "stream seek is outside borrowed bytes"));
			}
			offset = static_cast<usize>(target);
			return static_cast<u64>(offset);
		}

		borrowed_file::borrowed_file(const u08* read_data, u08* write_data, usize length)
			: readable_data(read_data), writable_data(write_data), size_value(length) {}
		access_mode borrowed_file::access() const noexcept { return writable_data ? access_mode::read_write : access_mode::read_only; }
		report<node_status> borrowed_file::status() const { return node_status(node_type::file, size_value, access()); }
		report<stream> borrowed_file::open(file_access requested) const {
			if (requested != file_access::read && writable_data == nullptr) {
				return unexpected(error(error_code::read_only, "borrowed bytes are const"));
			}
			return stream(borrowed_stream(readable_data, writable_data, size_value));
		}

		buffer_stream::buffer_stream(std::shared_ptr<vector<u08>> value) : bytes(std::move(value)), offset(0) {}
		bool buffer_stream::readable() const noexcept { return true; }
		bool buffer_stream::writable() const noexcept { return true; }
		bool buffer_stream::seekable() const noexcept { return true; }
		bool buffer_stream::resizable() const noexcept { return true; }
		u64 buffer_stream::position() const noexcept { return offset; }
		report<usize> buffer_stream::read(span<u08> destination) {
			const usize count = std::min(destination.size(), bytes->size() - offset);
			if (count > 0) {
				std::memcpy(destination.data(), bytes->data() + offset, count);
				offset += count;
			}
			return count;
		}
		report<usize> buffer_stream::write(span<const u08> source) {
			const usize required = offset + source.size();
			if (required > bytes->size()) {
				bytes->resize(required);
			}
			if (!source.empty()) {
				std::memcpy(bytes->data() + offset, source.data(), source.size());
				offset += source.size();
			}
			return source.size();
		}
		report<u64> buffer_stream::seek(i64 value, seek_origin origin) {
			i64 base = 0;
			if (origin == seek_origin::current) {
				base = static_cast<i64>(offset);
			}
			if (origin == seek_origin::end) {
				base = static_cast<i64>(bytes->size());
			}
			const i64 target = base + value;
			if (target < 0 || target > static_cast<i64>(bytes->size())) {
				return unexpected(error(error_code::out_of_range, "stream seek is outside staging bytes"));
			}
			offset = static_cast<usize>(target);
			return static_cast<u64>(offset);
		}
		report<void> buffer_stream::resize(u64 size) {
			if (size > std::numeric_limits<usize>::max()) {
				return unexpected(error(error_code::file_too_large, "staging file exceeds address space"));
			}
			bytes->resize(static_cast<usize>(size));
			if (offset > bytes->size()) {
				offset = bytes->size();
			}
			return {};
		}

		timepoint from_native_time(std::filesystem::file_time_type value) {
			const auto file_now = std::filesystem::file_time_type::clock::now();
			const auto system_now = std::chrono::system_clock::now();
			const auto system_time = system_now + std::chrono::duration_cast<std::chrono::system_clock::duration>(value - file_now);
			return timepoint::from_unix_epoch(duration::from_chrono(system_time.time_since_epoch()));
		}

		std::filesystem::file_time_type to_native_time(timepoint value) {
			const auto file_now = std::filesystem::file_time_type::clock::now();
			const auto system_now = std::chrono::system_clock::now();
			const auto elapsed = std::chrono::duration_cast<std::chrono::system_clock::duration>(value.since_unix_epoch().to_chrono<i64, std::nano>());
			const auto system_time = std::chrono::system_clock::time_point(elapsed);
			return file_now + std::chrono::duration_cast<std::filesystem::file_time_type::duration>(system_time - system_now);
		}

		bool path_is_within(const native_path& root, const native_path& value) {
			const native_path relative = value.lexically_relative(root);
			return !relative.empty() && *relative.begin() != native_path("..");
		}

		report<void> sync_native_file(const native_path& location) {
#if defined(_WIN32)
			const HANDLE handle = CreateFileW(location.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (handle == INVALID_HANDLE_VALUE) {
				return unexpected(error(error_code::io_error, "unable to open file for synchronization"));
			}
			const BOOL flushed = FlushFileBuffers(handle);
			CloseHandle(handle);
			if (!flushed) {
				return unexpected(error(error_code::io_error, "unable to synchronize file"));
			}
#else
			const int handle = ::open(location.c_str(), O_RDONLY);
			if (handle < 0) {
				return unexpected(error(error_code::io_error, "unable to open file for synchronization"));
			}
			const int result = ::fsync(handle);
			::close(handle);
			if (result != 0) {
				return unexpected(error(error_code::io_error, "unable to synchronize file"));
			}
#endif
			return {};
		}

		report<void> sync_native_directory(const native_path& location) {
#if defined(_WIN32)
			return unexpected(error(error_code::unsupported_operation, "directory synchronization is unavailable on Windows"));
#else
			const int handle = ::open(location.c_str(), O_RDONLY | O_DIRECTORY);
			if (handle < 0) {
				return unexpected(error(error_code::io_error, "unable to open directory for synchronization"));
			}
			const int result = ::fsync(handle);
			::close(handle);
			if (result != 0) {
				return unexpected(error(error_code::io_error, "unable to synchronize directory"));
			}
			return {};
#endif
		}

		class native_stream {
		  public:
			native_stream(native_path path, file_access requested);

			static report<native_stream> open(native_path location, file_access requested);

			bool readable() const noexcept;
			bool writable() const noexcept;
			bool seekable() const noexcept;
			bool resizable() const noexcept;
			u64 position() const noexcept;

			report<usize> read(span<u08> destination);
			report<usize> write(span<const u08> source);
			report<u64> seek(i64 value, seek_origin origin);
			report<void> resize(u64 size);
			report<void> flush();

		  private:
			native_path location;
			file_access requested_access;
			std::fstream handle;
			u64 offset;
		};

		native_stream::native_stream(native_path path, file_access requested)
			: location(std::move(path)), requested_access(requested), offset(0) {}
		report<native_stream> native_stream::open(native_path location, file_access requested) {
			native_stream result(std::move(location), requested);
			std::ios::openmode mode = std::ios::binary;
			if (requested == file_access::read) {
				mode |= std::ios::in;
			}
			if (requested == file_access::write) {
				mode |= std::ios::out;
			}
			if (requested == file_access::read_write) {
				mode |= std::ios::in | std::ios::out;
			}
			result.handle.open(result.location, mode);
			if (!result.handle) {
				return unexpected(error(error_code::io_error, "unable to open native file stream"));
			}
			return result;
		}
		bool native_stream::readable() const noexcept { return requested_access != file_access::write; }
		bool native_stream::writable() const noexcept { return requested_access != file_access::read; }
		bool native_stream::seekable() const noexcept { return true; }
		bool native_stream::resizable() const noexcept { return writable(); }
		u64 native_stream::position() const noexcept { return offset; }
		report<usize> native_stream::read(span<u08> destination) {
			if (!readable()) {
				return unexpected(error(error_code::read_only, "stream was opened for writing"));
			}
			handle.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(destination.size()));
			const usize count = static_cast<usize>(handle.gcount());
			if (handle.bad()) {
				return unexpected(error(error_code::io_error, "native stream read failed"));
			}
			handle.clear(handle.rdstate() & ~std::ios::failbit & ~std::ios::eofbit);
			offset += count;
			return count;
		}
		report<usize> native_stream::write(span<const u08> source) {
			if (!writable()) {
				return unexpected(error(error_code::read_only, "stream was opened for reading"));
			}
			handle.write(reinterpret_cast<const char*>(source.data()), static_cast<std::streamsize>(source.size()));
			if (!handle) {
				return unexpected(error(error_code::io_error, "native stream write failed"));
			}
			offset += source.size();
			return source.size();
		}
		report<u64> native_stream::seek(i64 value, seek_origin origin) {
			std::ios::seekdir direction = std::ios::beg;
			if (origin == seek_origin::current) {
				direction = std::ios::cur;
			}
			if (origin == seek_origin::end) {
				direction = std::ios::end;
			}
			handle.clear();
			if (readable()) {
				handle.seekg(value, direction);
			}
			if (writable()) {
				handle.seekp(value, direction);
			}
			if (!handle) {
				return unexpected(error(error_code::out_of_range, "native stream seek failed"));
			}
			const std::streampos position = readable() ? handle.tellg() : handle.tellp();
			if (position < 0) {
				return unexpected(error(error_code::out_of_range, "native stream position is invalid"));
			}
			offset = static_cast<u64>(position);
			return offset;
		}
		report<void> native_stream::resize(u64 size) {
			if (!writable()) {
				return unexpected(error(error_code::read_only, "stream was opened for reading"));
			}
			handle.flush();
			std::error_code system_error;
			std::filesystem::resize_file(location, size, system_error);
			if (system_error) {
				return unexpected(error(error_code::io_error, "native stream resize failed"));
			}
			if (offset > size) {
				offset = size;
			}
			return {};
		}
		report<void> native_stream::flush() {
			handle.flush();
			if (!handle) {
				return unexpected(error(error_code::io_error, "native stream flush failed"));
			}
			return {};
		}

		class native_file {
		  public:
			native_file(native_path path, access_mode permissions);

			access_mode access() const noexcept;
			report<node_status> status() const;
			report<stream> open(file_access requested) const;

		  private:
			native_path location;
			access_mode permissions;
		};

		native_file::native_file(native_path path, access_mode value) : location(std::move(path)), permissions(value) {}
		access_mode native_file::access() const noexcept { return permissions; }
		report<node_status> native_file::status() const {
			std::error_code system_error;
			const std::filesystem::file_status native_status = std::filesystem::status(location, system_error);
			if (system_error || !std::filesystem::exists(native_status)) {
				return unexpected(error(error_code::not_found, "native file no longer exists"));
			}
			if (!std::filesystem::is_regular_file(native_status)) {
				return unexpected(error(error_code::not_a_file, "native node is not a regular file"));
			}
			const u64 size = std::filesystem::file_size(location, system_error);
			if (system_error) {
				return unexpected(error(error_code::io_error, "unable to read native file size"));
			}
			const std::filesystem::file_time_type modified = std::filesystem::last_write_time(location, system_error);
			optional<timepoint> modification;
			if (!system_error) {
				modification = from_native_time(modified);
			}
			return node_status(node_type::file, size, permissions, file_times(optional<timepoint>(), modification));
		}
		report<stream> native_file::open(file_access requested) const {
			if (requested != file_access::read && permissions == access_mode::read_only) {
				return unexpected(error(error_code::read_only, "native file is mounted read-only"));
			}
			report<native_stream> opened = native_stream::open(location, requested);
			if (!opened) {
				return unexpected(opened.error());
			}
			return stream(std::move(*opened));
		}

		class native_volume {
		  public:
			native_volume(native_path root, native_volume_options value)
				: root_path(std::move(root)), options(value) {}

			access_mode access() const noexcept { return options.access(); }

			report<native_path> resolve(const path& location) const {
				if (location.is_absolute()) { return unexpected(error(error_code::invalid_path, "volume paths must be relative")); }
				native_path result = root_path;
				if (!location.empty()) {
					result /= native_path(location.text());
				}
				std::error_code system_error;
				if (options.symlinks() == symlink_policy::reject) {
					native_path segment = root_path;
					string_view text = location.text();
					usize begin = 0;
					while (begin < text.size()) {
						const usize end = text.find('/', begin);
						const string_view name = text.substr(begin, end == string_view::npos ? text.size() - begin : end - begin);
						segment /= native_path(name);
						const std::filesystem::file_status state = std::filesystem::symlink_status(segment, system_error);
						if (!system_error && std::filesystem::is_symlink(state)) {
							return unexpected(error(error_code::permission_denied, "native volume symlink is rejected"));
						}
						system_error.clear();
						if (end == string_view::npos) {
							break;
						}
						begin = end + 1;
					}
					return result;
				}
				const native_path existing = std::filesystem::weakly_canonical(result, system_error);
				if (!system_error && !path_is_within(root_path, existing) && existing != root_path) {
					return unexpected(error(error_code::permission_denied, "native volume path escapes root"));
				}
				return result;
			}

			report<node_status> status(const path& location) const {
				report<native_path> resolved = resolve(location);
				if (!resolved) {
					return unexpected(resolved.error());
				}
				std::error_code system_error;
				const std::filesystem::file_status state = std::filesystem::status(*resolved, system_error);
				if (system_error && system_error != std::errc::no_such_file_or_directory) {
					return unexpected(error(system_error, "reading native node status"));
				}
				if (!std::filesystem::exists(state)) {
					return unexpected(error(error_code::not_found, "native volume node does not exist"));
				}
				if (std::filesystem::is_directory(state)) {
					return node_status(node_type::directory, 0, access());
				}
				if (!std::filesystem::is_regular_file(state)) {
					return unexpected(error(error_code::unsupported_operation, "native volume node is not a file or directory"));
				}
				native_file source(*resolved, access());
				return source.status();
			}

			report<file> open_file(const path& location, open_options open) {
				report<native_path> resolved = resolve(location);
				if (!resolved) {
					return unexpected(resolved.error());
				}
				if (open.access() != file_access::read && access() == access_mode::read_only) {
					return unexpected(error(error_code::read_only, "native volume is mounted read-only"));
				}
				std::error_code system_error;
				const bool present = std::filesystem::exists(*resolved, system_error);
				if (system_error) {
					return unexpected(error(error_code::io_error, "unable to inspect native file"));
				}
				if (present && std::filesystem::is_directory(*resolved, system_error)) {
					return unexpected(error(error_code::not_a_file, "native node is a directory"));
				}
				const file_disposition disposition = open.disposition();
				if (!present && (disposition == file_disposition::open_existing || disposition == file_disposition::truncate_existing)) {
					return unexpected(error(error_code::not_found, "native file does not exist"));
				}
				if (present && disposition == file_disposition::create_new) {
					return unexpected(error(error_code::already_exists, "native file already exists"));
				}
				if (!present && open.access() == file_access::read) {
					return unexpected(error(error_code::not_found, "native file does not exist"));
				}
				if (!present || disposition == file_disposition::truncate_existing || disposition == file_disposition::create_or_truncate) {
					std::ofstream created(*resolved, std::ios::binary | std::ios::trunc);
					if (!created) {
						return unexpected(error(error_code::io_error, "unable to create native file"));
					}
				}
				return file(native_file(*resolved, access()));
			}

			report<vector<directory_entry>> list(const path& location) const {
				auto node{ status(location) };
				if (!node) { return unexpected(node.error()); }
				if (node->type() != node_type::directory) {
					return unexpected(error(error_code::not_a_directory, "native node is not a directory"));
				}
				report<native_path> resolved = resolve(location);
				if (!resolved) {
					return unexpected(resolved.error());
				}
				std::error_code system_error;
				auto result = vector<directory_entry>();
				for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(*resolved, system_error)) {
					if (system_error) {
						return unexpected(error(error_code::io_error, "native directory iteration failed"));
					}
					const string name = entry.path().filename().string();
					if (name.empty() || name.find('\\') != string::npos || name.find('/') != string::npos) {
						return unexpected(error(error_code::corrupt_data, "native directory has an invalid virtual name"));
					}
					report<node_status> child_status = status(location.append(name));
					if (!child_status) {
						return unexpected(child_status.error());
					}
					result.emplace_back(name, std::move(*child_status));
				}
				std::sort(result.begin(), result.end(), [](const directory_entry& lhs, const directory_entry& rhs) {
					return lhs.name() < rhs.name();
				});
				return result;
			}

			report<void> create_directories(const path& location) {
				if (access() == access_mode::read_only) {
					return unexpected(error(error_code::read_only, "native volume is mounted read-only"));
				}
				report<native_path> resolved = resolve(location);
				if (!resolved) {
					return unexpected(resolved.error());
				}
				std::error_code system_error;
				std::filesystem::create_directories(*resolved, system_error);
				if (system_error) {
					return unexpected(error(error_code::io_error, "unable to create native directories"));
				}
				return {};
			}

			report<void> remove(const path& location, recursion recursive) {
				if (access() == access_mode::read_only) {
					return unexpected(error(error_code::read_only, "native volume is mounted read-only"));
				}
				report<native_path> resolved = resolve(location);
				if (!resolved) {
					return unexpected(resolved.error());
				}
				std::error_code system_error;
				if (recursive == recursion::recursive) {
					std::filesystem::remove_all(*resolved, system_error);
				} else {
					const bool removed = std::filesystem::remove(*resolved, system_error);
					if (!removed && !system_error) {
						return unexpected(error(error_code::not_found, "native node does not exist"));
					}
				}
				if (system_error) {
					return unexpected(error(error_code::io_error, "unable to remove native node"));
				}
				return {};
			}

			report<void> rename(const path& source, const path& destination, existing_action existing) {
				if (access() == access_mode::read_only) {
					return unexpected(error(error_code::read_only, "native volume is mounted read-only"));
				}
				report<native_path> from = resolve(source);
				report<native_path> to = resolve(destination);
				if (!from) {
					return unexpected(from.error());
				}
				if (!to) {
					return unexpected(to.error());
				}
				std::error_code system_error;
				if (std::filesystem::exists(*to, system_error)) {
					if (existing == existing_action::fail) {
						return unexpected(error(error_code::already_exists, "rename destination already exists"));
					}
					std::filesystem::remove_all(*to, system_error);
					if (system_error) {
						return unexpected(error(error_code::io_error, "unable to replace rename destination"));
					}
				}
				std::filesystem::rename(*from, *to, system_error);
				if (system_error) {
					return unexpected(error(error_code::io_error, "native rename failed"));
				}
				return {};
			}

			report<void> set_times(const path& location, const file_times& times) {
				report<native_path> resolved = resolve(location);
				if (!resolved) {
					return unexpected(resolved.error());
				}
				if (!times.modification()) {
					return {};
				}
				std::error_code system_error;
				std::filesystem::last_write_time(*resolved, to_native_time(*times.modification()), system_error);
				if (system_error) {
					return unexpected(error(error_code::io_error, "unable to update native modification time"));
				}
				return {};
			}

			report<void> write_all_atomic(const path& location, span<const u08> bytes, atomic_write_options atomic_options) {
				if (access() == access_mode::read_only) {
					return unexpected(error(error_code::read_only, "native volume is mounted read-only"));
				}
				report<native_path> resolved = resolve(location);
				if (!resolved) {
					return unexpected(resolved.error());
				}
				std::error_code system_error;
				if (atomic_options.parents() == parent_action::create) {
					std::filesystem::create_directories(resolved->parent_path(), system_error);
					if (system_error) {
						return unexpected(error(error_code::io_error, "unable to create atomic-write parents"));
					}
				}
				if (!std::filesystem::exists(resolved->parent_path(), system_error)) {
					return unexpected(error(error_code::not_found, "atomic-write parent does not exist"));
				}
				if (std::filesystem::exists(*resolved, system_error) && atomic_options.existing() == existing_action::fail) {
					return unexpected(error(error_code::already_exists, "atomic-write destination already exists"));
				}
				const native_path temporary = resolved->parent_path() / native_path(resolved->filename().string() + ".leaf-tmp-" + std::to_string(next_temp_id.fetch_add(1)));
				{
					std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
					if (!output) {
						return unexpected(error(error_code::io_error, "unable to create atomic staging file"));
					}
					output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
					output.flush();
					if (!output) {
						std::filesystem::remove(temporary, system_error);
						return unexpected(error(error_code::io_error, "unable to write atomic staging file"));
					}
				}
				if (atomic_options.persistence() != durability::none) {
					report<void> synced = sync_native_file(temporary);
					if (!synced) {
						std::filesystem::remove(temporary, system_error);
						return unexpected(synced.error());
					}
				}
#if defined(_WIN32)
				if (!MoveFileExW(temporary.c_str(), resolved->c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
					std::filesystem::remove(temporary, system_error);
					return unexpected(error(error_code::io_error, "atomic native replacement failed"));
				}
#else
				std::filesystem::rename(temporary, *resolved, system_error);
				if (system_error) {
					std::filesystem::remove(temporary, system_error);
					return unexpected(error(error_code::io_error, "atomic native replacement failed"));
				}
#endif
				if (atomic_options.persistence() == durability::file_and_directory) {
					report<void> synced = sync_native_directory(resolved->parent_path());
					if (!synced) {
						return unexpected(synced.error());
					}
				}
				return {};
			}

			static std::atomic<u64> next_temp_id;

		  private:
			native_path root_path;
			native_volume_options options;
		};

		std::atomic<u64> native_volume::next_temp_id = 1;

		class subvolume_backend final : public volume_backend {
		  public:
			subvolume_backend(std::shared_ptr<volume_backend> parent, path base);

			access_mode access() const noexcept override;
			report<path> full_path(const path& location) const;
			report<node_status> status(const path& location) const override;
			report<file> open_file(const path& location, open_options options) override;
			report<vector<directory_entry>> list(const path& location) const override;
			report<void> create_directories(const path& location) override;
			report<void> remove(const path& location, recursion recursive) override;
			report<void> rename(const path& source, const path& destination, existing_action existing) override;
			report<void> set_times(const path& location, const file_times& times) override;
			report<void> write_all_atomic(const path& location, span<const u08> bytes, atomic_write_options options) override;

		  private:
			std::shared_ptr<volume_backend> parent_backend;
			path base_path;
		};

		subvolume_backend::subvolume_backend(std::shared_ptr<volume_backend> parent, path base)
			: parent_backend(std::move(parent)), base_path(std::move(base)) {}
		access_mode subvolume_backend::access() const noexcept { return parent_backend->access(); }
		report<path> subvolume_backend::full_path(const path& location) const {
			if (base_path.empty()) {
				return location;
			}
			if (location.empty()) {
				return base_path;
			}
			return path::parse(string(base_path.text()) + "/" + string(location.text()));
		}
		report<node_status> subvolume_backend::status(const path& location) const {
			report<path> full = full_path(location);
			if (!full) {
				return unexpected(full.error());
			}
			return parent_backend->status(*full);
		}
		report<file> subvolume_backend::open_file(const path& location, open_options options) {
			report<path> full = full_path(location);
			if (!full) {
				return unexpected(full.error());
			}
			return parent_backend->open_file(*full, options);
		}
		report<vector<directory_entry>> subvolume_backend::list(const path& location) const {
			report<path> full = full_path(location);
			if (!full) {
				return unexpected(full.error());
			}
			return parent_backend->list(*full);
		}
		report<void> subvolume_backend::create_directories(const path& location) {
			report<path> full = full_path(location);
			if (!full) {
				return unexpected(full.error());
			}
			return parent_backend->create_directories(*full);
		}
		report<void> subvolume_backend::remove(const path& location, recursion recursive) {
			report<path> full = full_path(location);
			if (!full) {
				return unexpected(full.error());
			}
			return parent_backend->remove(*full, recursive);
		}
		report<void> subvolume_backend::rename(const path& source, const path& destination, existing_action existing) {
			report<path> from = full_path(source);
			report<path> to = full_path(destination);
			if (!from) {
				return unexpected(from.error());
			}
			if (!to) {
				return unexpected(to.error());
			}
			return parent_backend->rename(*from, *to, existing);
		}
		report<void> subvolume_backend::set_times(const path& location, const file_times& times) {
			report<path> full = full_path(location);
			if (!full) {
				return unexpected(full.error());
			}
			return parent_backend->set_times(*full, times);
		}
		report<void> subvolume_backend::write_all_atomic(const path& location, span<const u08> bytes, atomic_write_options options) {
			report<path> full = full_path(location);
			if (!full) {
				return unexpected(full.error());
			}
			return parent_backend->write_all_atomic(*full, bytes, options);
		}
	}

	file borrow(span<const u08> bytes) {
		return file(detail::borrowed_file(bytes.data(), nullptr, bytes.size()));
	}
	file borrow(span<u08> bytes) {
		return file(detail::borrowed_file(bytes.data(), bytes.data(), bytes.size()));
	}

	report<file> native_file(native_path source, open_options options) {
		std::error_code system_error;
		const bool present = std::filesystem::exists(source, system_error);
		if (system_error) {
			return unexpected(error(error_code::io_error, "unable to inspect native file"));
		}
		if (!present && options.disposition() != file_disposition::open_existing && options.disposition() != file_disposition::truncate_existing && options.access() != file_access::read) {
			std::ofstream created(source, std::ios::binary | std::ios::trunc);
			if (!created) {
				return unexpected(error(error_code::io_error, "unable to create native file"));
			}
		}
		if (!std::filesystem::exists(source, system_error)) {
			return unexpected(error(error_code::not_found, "native file does not exist"));
		}
		if (!std::filesystem::is_regular_file(source, system_error)) {
			return unexpected(error(error_code::not_a_file, "native node is not a regular file"));
		}
		if (options.disposition() == file_disposition::truncate_existing || options.disposition() == file_disposition::create_or_truncate) {
			std::ofstream truncated(source, std::ios::binary | std::ios::trunc);
			if (!truncated) {
				return unexpected(error(error_code::io_error, "unable to truncate native file"));
			}
		}
		const access_mode access = options.access() == file_access::read ? access_mode::read_only : access_mode::read_write;
		return file(detail::native_file(std::move(source), access));
	}

	report<volume> native_volume(native_path source, native_volume_options options) {
		std::error_code system_error;
		if (!std::filesystem::exists(source, system_error)) {
			if (options.missing() != missing_action::create || options.access() == access_mode::read_only) {
				return unexpected(error(error_code::not_found, "native volume root does not exist"));
			}
			std::filesystem::create_directories(source, system_error);
			if (system_error) {
				return unexpected(error(error_code::io_error, "unable to create native volume root"));
			}
		}
		if (!std::filesystem::is_directory(source, system_error)) {
			return unexpected(error(error_code::not_a_directory, "native volume root is not a directory"));
		}
		if (options.symlinks() == symlink_policy::reject && std::filesystem::is_symlink(std::filesystem::symlink_status(source, system_error))) {
			return unexpected(error(error_code::permission_denied, "native volume root is a symlink"));
		}
		const native_path root = std::filesystem::weakly_canonical(source, system_error);
		if (system_error) {
			return unexpected(error(error_code::io_error, "unable to canonicalize native volume root"));
		}
		return volume(detail::native_volume(root, options));
	}

	report<volume> volume::subvolume(const path& location) const {
		if (location.is_absolute()) { return unexpected(error(error_code::invalid_path, "subvolume paths must be relative")); }
		if (!backend) {
			return unexpected(error(error_code::not_found, "volume is empty"));
		}
		report<node_status> node = backend->status(location);
		if (!node) {
			return unexpected(node.error());
		}
		if (node->type() != node_type::directory) {
			return unexpected(error(error_code::not_a_directory, "subvolume location is not a directory"));
		}
		return volume(std::static_pointer_cast<detail::volume_backend>(std::make_shared<detail::subvolume_backend>(backend, location)));
	}

	atomic_write::atomic_write(stream output, std::shared_ptr<vector<u08>> staging, std::shared_ptr<detail::volume_backend> backend, path location, atomic_write_options value)
		: output_stream(std::move(output)), staging_bytes(std::move(staging)), target(std::move(backend)), destination(std::move(location)), options(value), complete(false) {}
	atomic_write::atomic_write(atomic_write&&) noexcept = default;
	atomic_write& atomic_write::operator=(atomic_write&&) noexcept = default;
	atomic_write::~atomic_write() { cancel(); }
	stream& atomic_write::output() noexcept { return *output_stream; }
	report<void> atomic_write::commit() {
		if (complete || !output_stream || !staging_bytes || !target) {
			return unexpected(error(error_code::io_error, "atomic write is no longer active"));
		}
		report<void> flushed = output_stream->flush();
		if (!flushed) {
			return unexpected(flushed.error());
		}
		report<void> result = target->write_all_atomic(destination, span<const u08>(staging_bytes->data(), staging_bytes->size()), options);
		if (!result) {
			return unexpected(result.error());
		}
		complete = true;
		return {};
	}
	void atomic_write::cancel() noexcept {
		output_stream.reset();
		staging_bytes.reset();
		target.reset();
	}

	namespace detail {
		bool is_path_prefix(const path& parent, const path& child) {
			if (parent.is_root()) {
				return true;
			}
			if (child.text().size() < parent.text().size()) {
				return false;
			}
			if (!child.text().starts_with(parent.text())) {
				return false;
			}
			return child.text().size() == parent.text().size() || child.text()[parent.text().size()] == '/';
		}

		report<path> relative_to(const path& root, const path& location) {
			if (!is_path_prefix(root, location)) {
				return unexpected(error(error_code::not_mapped, "path is outside mount root"));
			}
			if (root == location) {
				return path();
			}
			const usize start = root.is_root() ? 1 : root.text().size() + 1;
			return path::parse(location.text().substr(start));
		}

		bool is_missing_error(const error& value) {
			return value.code == make_error_code(error_code::not_found) || value.code == make_error_code(error_code::not_mapped);
		}

		struct mount_record {
			u64 id;
			path destination;
			std::variant<file, volume> source;
		};

		struct resolved_node {
			optional<file> direct_file;
			std::shared_ptr<volume_backend> volume_source;
			path location;
			path mount_destination;
		};

		class mount_table final : public Singleton<mount_table> {
		  public:
			report<mapping> attach(path destination, file source) {
				return attach_record(destination, std::variant<file, volume>(std::move(source)));
			}

			report<mapping> attach(path destination, volume source) {
				return attach_record(destination, std::variant<file, volume>(std::move(source)));
			}

			void detach(u64 id) noexcept {
				std::unique_lock lock(records_mutex);
				records.erase(std::remove_if(records.begin(), records.end(), [id](const mount_record& record) {
					return record.id == id;
				}), records.end());
			}

			report<resolved_node> resolve(const path& location) const {
				if (!location.is_absolute()) { return unexpected(error(error_code::invalid_path, "namespace lookup requires an absolute path")); }
				const auto snapshot = snapshot_records();
				optional<path> selected_destination;
				for (const mount_record& record : snapshot) {
					const bool matches = std::holds_alternative<volume>(record.source)
						? is_path_prefix(record.destination, location)
						: record.destination == location;
					if (!matches) {
						continue;
					}
					if (!selected_destination || record.destination.text().size() > selected_destination->text().size()) {
						selected_destination = record.destination;
					}
				}
				if (!selected_destination) {
					return unexpected(error(error_code::not_mapped, "virtual path is not mounted"));
				}

				for (auto iterator = snapshot.rbegin(); iterator != snapshot.rend(); ++iterator) {
					if (iterator->destination != *selected_destination) {
						continue;
					}
					if (const auto* source = std::get_if<file>(&iterator->source)) {
						return resolved_node{ *source, std::shared_ptr<volume_backend>(), path(), iterator->destination };
					}
					const volume& source = std::get<volume>(iterator->source);
					report<path> relative = relative_to(iterator->destination, location);
					if (!relative) {
						return unexpected(relative.error());
					}
					report<node_status> current = source.backend->status(*relative);
					if (current) {
						return resolved_node{ optional<file>(), source.backend, std::move(*relative), iterator->destination };
					}
					if (!is_missing_error(current.error())) {
						return unexpected(current.error());
					}
				}
				return unexpected(error(error_code::not_found, "mounted path does not exist"));
			}

			report<resolved_node> resolve_writable(const path& location) const {
				if (!location.is_absolute()) { return unexpected(error(error_code::invalid_path, "namespace lookup requires an absolute path")); }
				const auto snapshot = snapshot_records();
				optional<path> selected_destination;
				for (const mount_record& record : snapshot) {
					if (!std::holds_alternative<volume>(record.source) || !is_path_prefix(record.destination, location)) {
						continue;
					}
					if (!selected_destination || record.destination.text().size() > selected_destination->text().size()) {
						selected_destination = record.destination;
					}
				}
				if (!selected_destination) {
					return unexpected(error(error_code::not_mapped, "virtual path has no writable volume"));
				}

				for (auto iterator = snapshot.rbegin(); iterator != snapshot.rend(); ++iterator) {
					if (iterator->destination != *selected_destination || !std::holds_alternative<volume>(iterator->source)) {
						continue;
					}
					const volume& source = std::get<volume>(iterator->source);
					report<path> relative = relative_to(iterator->destination, location);
					if (!relative) {
						return unexpected(relative.error());
					}
					report<node_status> current = source.backend->status(*relative);
					if (current) {
						return resolved_node{ optional<file>(), source.backend, std::move(*relative), iterator->destination };
					}
					if (!is_missing_error(current.error())) {
						return unexpected(current.error());
					}
					if (source.backend->access() == access_mode::read_write) {
						return resolved_node{ optional<file>(), source.backend, std::move(*relative), iterator->destination };
					}
				}
				return unexpected(error(error_code::read_only, "no writable layer can create the node"));
			}

			report<vector<directory_entry>> list(const path& location) const {
				const auto snapshot = snapshot_records();
				if (!location.is_absolute()) { return unexpected(error(error_code::invalid_path, "namespace lookup requires an absolute path")); }
				optional<path> selected_destination;
				for (const mount_record& record : snapshot) {
					if (std::holds_alternative<file>(record.source) && record.destination == location) {
						return unexpected(error(error_code::not_a_directory, "mounted node is a file"));
					}
					if (std::holds_alternative<volume>(record.source) && is_path_prefix(record.destination, location)) {
						if (!selected_destination || record.destination.text().size() > selected_destination->text().size()) {
							selected_destination = record.destination;
						}
					}
				}

				auto result = vector<directory_entry>();
				std::unordered_set<string> seen;
				bool directory_exists{};
				if (selected_destination) {
					for (auto iterator = snapshot.rbegin(); iterator != snapshot.rend(); ++iterator) {
						if (iterator->destination != *selected_destination || !std::holds_alternative<volume>(iterator->source)) {
							continue;
						}
						const volume& source = std::get<volume>(iterator->source);
						report<path> relative = relative_to(iterator->destination, location);
						if (!relative) {
							return unexpected(relative.error());
						}
						report<vector<directory_entry>> entries = source.backend->list(*relative);
						if (!entries) {
							if (is_missing_error(entries.error())) {
								continue;
							}
							return unexpected(entries.error());
						}
						directory_exists = true;
						for (directory_entry& entry : *entries) {
							if (seen.emplace(string(entry.name())).second) {
								result.emplace_back(std::move(entry));
							}
						}
					}
				}

				for (const mount_record& record : snapshot) {
					if (!is_path_prefix(location, record.destination) || location == record.destination) {
						continue;
					}
					const usize start = location.is_root() ? 1 : location.text().size() + 1;
					const string_view remainder = record.destination.text().substr(start);
					const usize separator = remainder.find('/');
					const string name = string(remainder.substr(0, separator));
					if (!seen.emplace(name).second) {
						continue;
					}
					if (separator != string_view::npos) {
						result.emplace_back(name, node_status(node_type::directory, 0, access_mode::read_only));
						continue;
					}
					if (const auto* direct_source = std::get_if<file>(&record.source)) {
						report<node_status> file_status = direct_source->status();
						if (!file_status) {
							return unexpected(file_status.error());
						}
						result.emplace_back(name, std::move(*file_status));
					} else {
						const volume& source = std::get<volume>(record.source);
						result.emplace_back(name, node_status(node_type::directory, 0, source.access()));
					}
				}

				if (!directory_exists && result.empty()) {
					return unexpected(error(selected_destination ? error_code::not_found : error_code::not_mapped, "virtual directory does not exist"));
				}
				std::sort(result.begin(), result.end(), [](const directory_entry& lhs, const directory_entry& rhs) {
					return lhs.name() < rhs.name();
				});
				return result;
			}

			bool has_descendant_mount(const path& location) const {
				const auto snapshot = snapshot_records();
				for (const mount_record& record : snapshot) {
					if (location != record.destination && is_path_prefix(location, record.destination)) {
						return true;
					}
				}
				return false;
			}

		  private:
			report<mapping> attach_record(path destination, std::variant<file, volume> source) {
				if (!destination.is_absolute()) { return unexpected(error(error_code::invalid_path, "mount destination must be absolute")); }
				std::unique_lock lock(records_mutex);
				const bool adding_file = std::holds_alternative<file>(source);
				for (const mount_record& record : records) {
					const bool existing_file = std::holds_alternative<file>(record.source);
					if (record.destination == destination && existing_file != adding_file) {
						return unexpected(error(error_code::mapping_conflict, "file and volume cannot share a mount destination"));
					}
					if ((adding_file && is_path_prefix(destination, record.destination) && destination != record.destination)
						|| (existing_file && is_path_prefix(record.destination, destination) && destination != record.destination)) {
						return unexpected(error(error_code::mapping_conflict, "file mount cannot have mounted descendants"));
					}
				}
				const u64 id = next_id.fetch_add(1);
				records.emplace_back(mount_record{ id, destination, std::move(source) });
				return mapping(id, std::move(destination));
			}

			vector<mount_record> snapshot_records() const {
				std::shared_lock lock(records_mutex);
				return records;
			}

			mutable std::shared_mutex records_mutex;
			vector<mount_record> records;
			std::atomic<u64> next_id = 1;
		};
	}

	mapping::mapping(u64 value, path destination) : id(value), location(std::move(destination)) {}
	mapping::mapping(mapping&& other) noexcept : id(other.id), location(std::move(other.location)) {
		other.id = 0;
	}
	mapping& mapping::operator=(mapping&& other) noexcept {
		if (this != &other) {
			unmount();
			id = other.id;
			location = std::move(other.location);
			other.id = 0;
		}
		return *this;
	}
	mapping::~mapping() { unmount(); }
	bool mapping::mounted() const noexcept { return id != 0; }
	const path& mapping::destination() const noexcept { return location; }
	void mapping::unmount() noexcept {
		if (id != 0) {
			detail::mount_table::instance().detach(id);
			id = 0;
		}
	}

	report<mapping> mount(path_view destination, file source) {
		return detail::mount_table::instance().attach(destination.owning(), std::move(source));
	}
	report<mapping> mount(path_view destination, volume source) {
		return detail::mount_table::instance().attach(destination.owning(), std::move(source));
	}

	bool exists(path_view location) noexcept {
		return static_cast<bool>(status(location));
	}
	report<node_status> status(path_view location) {
		const path owned = location.owning();
		report<detail::resolved_node> resolved = detail::mount_table::instance().resolve(owned);
		if (!resolved) {
			if (detail::mount_table::instance().has_descendant_mount(owned)) {
				return node_status(node_type::directory, 0, access_mode::read_only);
			}
			return unexpected(resolved.error());
		}
		if (resolved->direct_file) {
			return resolved->direct_file->status();
		}
		return resolved->volume_source->status(resolved->location);
	}
	report<file> open_file(path_view location, open_options options) {
		const path owned = location.owning();
		const bool needs_write = options.access() != file_access::read || options.disposition() != file_disposition::open_existing;
		if (needs_write) {
			report<detail::resolved_node> resolved = detail::mount_table::instance().resolve_writable(owned);
			if (!resolved) {
				return unexpected(resolved.error());
			}
			return resolved->volume_source->open_file(resolved->location, options);
		}
		report<detail::resolved_node> resolved = detail::mount_table::instance().resolve(owned);
		if (!resolved) {
			return unexpected(resolved.error());
		}
		if (resolved->direct_file) {
			return *resolved->direct_file;
		}
		report<node_status> node = resolved->volume_source->status(resolved->location);
		if (!node) {
			return unexpected(node.error());
		}
		if (node->type() != node_type::file) {
			return unexpected(error(error_code::not_a_file, "virtual node is a directory"));
		}
		return resolved->volume_source->open_file(resolved->location, options);
	}
	report<volume> open_volume(path_view location) {
		const path owned = location.owning();
		report<detail::resolved_node> resolved = detail::mount_table::instance().resolve(owned);
		if (!resolved) {
			return unexpected(resolved.error());
		}
		if (resolved->direct_file) {
			return unexpected(error(error_code::not_a_directory, "virtual node is a file"));
		}
		report<node_status> node = resolved->volume_source->status(resolved->location);
		if (!node) {
			return unexpected(node.error());
		}
		if (node->type() != node_type::directory) {
			return unexpected(error(error_code::not_a_directory, "virtual node is a file"));
		}
		return volume(std::static_pointer_cast<detail::volume_backend>(std::make_shared<detail::subvolume_backend>(resolved->volume_source, resolved->location)));
	}
	report<vector<directory_entry>> list(path_view location) {
		return detail::mount_table::instance().list(location.owning());
	}
	report<void> create_directories(path_view location) {
		report<detail::resolved_node> resolved = detail::mount_table::instance().resolve_writable(location.owning());
		if (!resolved) {
			return unexpected(resolved.error());
		}
		return resolved->volume_source->create_directories(resolved->location);
	}
	report<void> remove(path_view location, recursion recursive) {
		report<detail::resolved_node> resolved = detail::mount_table::instance().resolve(location.owning());
		if (!resolved) {
			return unexpected(resolved.error());
		}
		if (resolved->direct_file) {
			return unexpected(error(error_code::read_only, "direct file mounts cannot be removed"));
		}
		return resolved->volume_source->remove(resolved->location, recursive);
	}
	report<void> rename(path_view source, path_view destination, existing_action existing) {
		report<detail::resolved_node> from = detail::mount_table::instance().resolve_writable(source.owning());
		report<detail::resolved_node> to = detail::mount_table::instance().resolve_writable(destination.owning());
		if (!from) {
			return unexpected(from.error());
		}
		if (!to) {
			return unexpected(to.error());
		}
		if (from->volume_source != to->volume_source) {
			return unexpected(error(error_code::cross_volume_operation, "rename crosses volume boundaries"));
		}
		return from->volume_source->rename(from->location, to->location, existing);
	}
	report<vector<u08>> read_all(path_view location) {
		report<file> opened = open_file(location);
		if (!opened) {
			return unexpected(opened.error());
		}
		return opened->read_all();
	}
	report<vector<u08>> read_all(path_view location, usize maximum_size) {
		report<file> opened = open_file(location);
		if (!opened) {
			return unexpected(opened.error());
		}
		return opened->read_all(maximum_size);
	}
	report<void> write_all(path_view location, span<const u08> bytes, open_options options) {
		report<file> opened = open_file(location, options);
		if (!opened) {
			return unexpected(opened.error());
		}
		report<stream> output = opened->open(options.access());
		if (!output) {
			return unexpected(output.error());
		}
		report<void> wrote = output->write_all(bytes);
		if (!wrote) {
			return unexpected(wrote.error());
		}
		return output->flush();
	}
	report<void> set_times(path_view location, const file_times& times) {
		report<detail::resolved_node> resolved = detail::mount_table::instance().resolve_writable(location.owning());
		if (!resolved) {
			return unexpected(resolved.error());
		}
		return resolved->volume_source->set_times(resolved->location, times);
	}
	report<atomic_write> begin_atomic_write(path_view location, atomic_write_options options) {
		report<detail::resolved_node> resolved = detail::mount_table::instance().resolve_writable(location.owning());
		if (!resolved) {
			return unexpected(resolved.error());
		}
		auto staging = std::make_shared<vector<u08>>();
		auto output = stream(detail::buffer_stream(staging));
		return atomic_write(std::move(output), std::move(staging), resolved->volume_source, std::move(resolved->location), options);
	}
	report<void> write_all_atomic(path_view location, span<const u08> bytes, atomic_write_options options) {
		report<detail::resolved_node> resolved = detail::mount_table::instance().resolve_writable(location.owning());
		if (!resolved) {
			return unexpected(resolved.error());
		}
		return resolved->volume_source->write_all_atomic(resolved->location, bytes, options);
	}

	report<void> copy_file_contents(const path& source, const path& destination, copy_options options) {
		report<file> input_file = open_file(source);
		if (!input_file) {
			return unexpected(input_file.error());
		}
		report<stream> input = input_file->open(file_access::read);
		if (!input) {
			return unexpected(input.error());
		}
		report<file> output_file = open_file(destination, open_options(file_access::write, options.existing() == existing_action::replace ? file_disposition::create_or_truncate : file_disposition::create_new));
		if (!output_file) {
			return unexpected(output_file.error());
		}
		report<stream> output = output_file->open(file_access::write);
		if (!output) {
			return unexpected(output.error());
		}
		auto buffer = vector<u08>(64 * 1024);
		for (;;) {
			report<usize> count = input->read(span<u08>(buffer.data(), buffer.size()));
			if (!count) {
				return unexpected(count.error());
			}
			if (*count == 0) {
				break;
			}
			report<void> wrote = output->write_all(span<const u08>(buffer.data(), *count));
			if (!wrote) {
				return unexpected(wrote.error());
			}
		}
		report<void> flushed = output->flush();
		if (!flushed) {
			return unexpected(flushed.error());
		}
		if (options.metadata() == metadata_action::preserve) {
			report<node_status> source_status = input_file->status();
			if (!source_status) {
				return unexpected(source_status.error());
			}
			return set_times(destination, source_status->times());
		}
		return {};
	}
	report<void> copy(path_view source, path_view destination, copy_options options) {
		const path source_path = source.owning();
		const path destination_path = destination.owning();
		report<node_status> source_status = status(source_path);
		if (!source_status) {
			return unexpected(source_status.error());
		}
		if (source_status->type() == node_type::file) {
			return copy_file_contents(source_path, destination_path, options);
		}
		report<void> created = create_directories(destination_path);
		if (!created) {
			return unexpected(created.error());
		}
		if (options.recursive() == recursion::shallow) {
			return {};
		}
		report<vector<directory_entry>> entries = list(source_path);
		if (!entries) {
			return unexpected(entries.error());
		}
		for (const directory_entry& entry : *entries) {
			report<void> child = copy(source_path.append(entry.name()), destination_path.append(entry.name()), options);
			if (!child) {
				return unexpected(child.error());
			}
		}
		if (options.metadata() == metadata_action::preserve) {
			return set_times(destination_path, source_status->times());
		}
		return {};
	}
	report<void> move(path_view source, path_view destination, move_options options) {
		report<void> renamed = rename(source, destination, options.existing());
		if (renamed) {
			return {};
		}
		if (renamed.error().code != make_error_code(error_code::cross_volume_operation) || options.cross_volume() == cross_volume_action::fail) {
			return unexpected(renamed.error());
		}
		report<void> copied = copy(source, destination, copy_options(options.existing(), recursion::recursive));
		if (!copied) {
			return unexpected(copied.error());
		}
		report<void> removed = remove(source, recursion::recursive);
		if (!removed) {
			static_cast<void>(remove(destination, recursion::recursive));
			return unexpected(removed.error());
		}
		return {};
	}
} // namespace lf::fs

namespace lf::fs {
	vector<mapping> system_mappings;

	error init(const native_path& install, const native_path& appdata) {
		if (!system_mappings.empty()) {
			return error{ generic_errc::input_error, "filesystem is already initialized" };
		}
		const native_path sources[]{ install, appdata };
		vector<mapping> mounted;
		for (usize index{}; index < std::size(sources); ++index) {
			auto directory{ native_volume(sources[index], native_volume_options{ index == 0 ? access_mode::read_only : access_mode::read_write }) };
			if (!directory) { return directory.error(); }
			auto entry{ mount("/", *directory) };
			if (!entry) { return entry.error(); }
			mounted.emplace_back(std::move(*entry));
		}
		system_mappings = std::move(mounted);
		return {};
	}

	void exit() {
		system_mappings.clear();
	}
}
