#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/memory.hpp"
#include "leaf/core/optional.hpp"
#include "leaf/core/span.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/time.hpp"
#include "leaf/core/types.hpp"
#include "leaf/core/vector.hpp"

#include <filesystem>
#include <memory>
#include <type_traits>
#include <utility>

namespace lf::fs {
	using native_path = std::filesystem::path;

	/*! @brief Overlays writable appdata on read-only installation at /; maps the working directory at /current. */
	error init(const native_path& install, const native_path& appdata);
	void exit();

	/*! @brief The kind of a node stored by a volume. */
	enum class node_type : u08 {
		file,
		directory
	};

	/*! @brief The mutation access supported by a file or volume. */
	enum class access_mode : u08 {
		read_only,
		read_write
	};

	/*! @brief The requested capabilities of a newly opened stream. */
	enum class file_access : u08 {
		read,
		write,
		read_write
	};

	/*! @brief The creation behavior when opening a file through a volume. */
	enum class file_disposition : u08 {
		open_existing,
		create_new,
		open_or_create,
		truncate_existing,
		create_or_truncate
	};

	/*! @brief The base position used by stream seeking. */
	enum class seek_origin : u08 {
		begin,
		current,
		end
	};

	/*! @brief The handling of an existing destination node. */
	enum class existing_action : u08 {
		fail,
		replace
	};

	/*! @brief Whether an operation may descend into child directories. */
	enum class recursion : u08 {
		shallow,
		recursive
	};

	/*! @brief The handling of source metadata while copying. */
	enum class metadata_action : u08 {
		ignore,
		preserve
	};

	/*! @brief The handling of move operations between distinct volumes. */
	enum class cross_volume_action : u08 {
		fail,
		copy
	};

	/*! @brief The handling of a missing native-volume root. */
	enum class missing_action : u08 {
		fail,
		create
	};

	/*! @brief The handling of symlinks within a native volume. */
	enum class symlink_policy : u08 {
		reject,
		follow_inside_root
	};

	/*! @brief The handling of missing parent directories for atomic writes. */
	enum class parent_action : u08 {
		require,
		create
	};

	/*! @brief The requested persistence barrier for an atomic write. */
	enum class durability : u08 {
		none,
		file,
		file_and_directory
	};

	class path;
	class stream;
	class file;
	class volume;
	class mapping;
	class atomic_write;

	/*! @brief A validated non-owning path in Leaf's virtual namespace. */
	class path_view {
	  public:
		path_view(const path& value) noexcept;
		path_view(const ch08* text);
		path_view(string_view text);
		path_view(const string& text);

		/*! @brief Reports whether this view refers to the virtual root. */
		bool is_root() const noexcept;
		bool is_absolute() const noexcept;
		bool empty() const noexcept;
		/*! @brief Gets canonical slash-separated path text. */
		string_view text() const noexcept;
		/*! @brief Gets the final component, or an empty view for root. */
		string_view filename() const noexcept;
		/*! @brief Copies this view into an owning validated path. */
		path owning() const;

	  private:
		string_view value;
	};

	/*! @brief A validated absolute or relative path in Leaf's virtual namespace. */
	class path {
	  public:
		path();
		path(const ch08* text);
		explicit path(const string& text);
		explicit path(string_view text);
		explicit path(path_view text);

		/*! @brief Parses untrusted text into an owning virtual path. */
		static report<path> parse(string_view text);

		/*! @brief Reports whether this is the virtual root path. */
		bool is_root() const noexcept;
		bool is_absolute() const noexcept;
		bool empty() const noexcept;
		/*! @brief Gets the canonical slash-separated text. */
		string_view text() const noexcept;
		/*! @brief Gets the final component, or an empty view for root. */
		string_view filename() const noexcept;
		/*! @brief Gets this path's parent, with root as its own parent. */
		path parent() const;
		/*! @brief Joins a relative path, or uses the right-hand path if it is absolute. */
		path append(path_view child) const;
		friend path operator/(const path& parent, path_view child) { return parent.append(child); }

		bool operator==(const path& other) const noexcept;

	  private:
		friend class path_view;
		friend class mapping;

		string value;
	};

	/*! @brief Optional wall-clock metadata attached to a filesystem node. */
	class file_times {
	  public:
		file_times(optional<timepoint> creation = optional<timepoint>(), optional<timepoint> modification = optional<timepoint>(), optional<timepoint> access = optional<timepoint>());

		optional<timepoint> creation() const noexcept;
		optional<timepoint> modification() const noexcept;
		optional<timepoint> access() const noexcept;

	  private:
		optional<timepoint> creation_value;
		optional<timepoint> modification_value;
		optional<timepoint> access_value;
	};

	/*! @brief Metadata and access information for one filesystem node. */
	class node_status {
	  public:
		node_status(node_type type, u64 size, access_mode access, file_times times = file_times());

		node_type type() const noexcept;
		u64 size() const noexcept;
		access_mode access() const noexcept;
		const file_times& times() const noexcept;

	  private:
		node_type type_value;
		u64 size_value;
		access_mode access_value;
		file_times times_value;
	};

	/*! @brief One immediate child returned by a directory listing. */
	class directory_entry {
	  public:
		directory_entry(string name, node_status status);

		string_view name() const noexcept;
		const node_status& status() const noexcept;

	  private:
		string name_value;
		node_status status_value;
	};

	/*! @brief Access and creation behavior for open_file. */
	class open_options {
	  public:
		open_options(file_access access = file_access::read, file_disposition disposition = file_disposition::open_existing);

		file_access access() const noexcept;
		file_disposition disposition() const noexcept;

	  private:
		file_access access_value;
		file_disposition disposition_value;
	};

	/*! @brief Destination and metadata behavior for copy operations. */
	class copy_options {
	  public:
		copy_options(existing_action existing = existing_action::fail, recursion recursive = recursion::shallow, metadata_action metadata = metadata_action::preserve);

		existing_action existing() const noexcept;
		recursion recursive() const noexcept;
		metadata_action metadata() const noexcept;

	  private:
		existing_action existing_value;
		recursion recursion_value;
		metadata_action metadata_value;
	};

	/*! @brief Destination behavior for move operations. */
	class move_options {
	  public:
		move_options(existing_action existing = existing_action::fail, cross_volume_action cross_volume = cross_volume_action::copy);

		existing_action existing() const noexcept;
		cross_volume_action cross_volume() const noexcept;

	  private:
		existing_action existing_value;
		cross_volume_action cross_volume_value;
	};

	/*! @brief Access, creation, and symlink behavior for a native volume. */
	class native_volume_options {
	  public:
		native_volume_options(access_mode access = access_mode::read_only, missing_action missing = missing_action::fail, symlink_policy symlinks = symlink_policy::reject);

		access_mode access() const noexcept;
		missing_action missing() const noexcept;
		symlink_policy symlinks() const noexcept;

	  private:
		access_mode access_value;
		missing_action missing_value;
		symlink_policy symlink_value;
	};

	/*! @brief Replacement, parent, and persistence behavior for atomic writes. */
	class atomic_write_options {
	  public:
		atomic_write_options(existing_action existing = existing_action::replace, parent_action parents = parent_action::require, durability durability = durability::file);

		existing_action existing() const noexcept;
		parent_action parents() const noexcept;
		durability persistence() const noexcept;

	  private:
		existing_action existing_value;
		parent_action parents_value;
		durability durability_value;
	};

	namespace detail {
		class stream_backend;
		class file_backend;
		class volume_backend;
		class mount_table;
		template<typename Backend>
		class stream_backend_model;
		template<typename Backend>
		class file_backend_model;
		template<typename Backend>
		class volume_backend_model;
	}

	/*! @brief A move-only cursor over file content. */
	class stream {
	  public:
		template<typename Backend>
		explicit stream(Backend backend);

		stream(stream&&) noexcept;
		stream& operator=(stream&&) noexcept;
		stream(const stream&) = delete;
		stream& operator=(const stream&) = delete;
		~stream();

		bool readable() const noexcept;
		bool writable() const noexcept;
		bool seekable() const noexcept;
		bool resizable() const noexcept;
		u64 position() const noexcept;

		report<usize> read(span<u08> destination);
		report<void> read_exact(span<u08> destination);
		report<usize> write(span<const u08> source);
		report<void> write_all(span<const u08> source);
		report<u64> seek(i64 offset, seek_origin origin);
		report<void> resize(u64 size);
		report<void> flush();

	  private:
		friend class file;
		friend class atomic_write;
		explicit stream(unique_ptr<detail::stream_backend> backend);

		unique_ptr<detail::stream_backend> backend;
	};

	/*! @brief A copyable type-erased file source that opens independent streams. */
	class file {
	  public:
		template<typename Backend>
		explicit file(Backend backend);

		file(const file&) noexcept;
		file(file&&) noexcept;
		file& operator=(const file&) noexcept;
		file& operator=(file&&) noexcept;
		~file();

		access_mode access() const noexcept;
		report<node_status> status() const;
		report<stream> open(file_access access = file_access::read) const;
		report<vector<u08>> read_all() const;
		report<vector<u08>> read_all(usize maximum_size) const;

	  private:
		friend class volume;
		friend class detail::mount_table;
		explicit file(std::shared_ptr<detail::file_backend> backend);

		std::shared_ptr<detail::file_backend> backend;
	};

	/*! @brief A copyable type-erased directory tree that can be mounted in the virtual namespace. */
	class volume {
	  public:
		template<typename Backend>
		explicit volume(Backend backend);

		volume(const volume&) noexcept;
		volume(volume&&) noexcept;
		volume& operator=(const volume&) noexcept;
		volume& operator=(volume&&) noexcept;
		~volume();

		access_mode access() const noexcept;
		report<volume> subvolume(const path& location) const;

	  private:
		friend class detail::mount_table;
		friend report<volume> open_volume(path_view location);
		explicit volume(std::shared_ptr<detail::volume_backend> backend);

		std::shared_ptr<detail::volume_backend> backend;
	};

	/*! @brief A move-only RAII mount whose destruction immediately detaches the mapping. */
	class mapping {
	  public:
		mapping(mapping&&) noexcept;
		mapping& operator=(mapping&&) noexcept;
		mapping(const mapping&) = delete;
		mapping& operator=(const mapping&) = delete;
		~mapping();

		bool mounted() const noexcept;
		const path& destination() const noexcept;
		void unmount() noexcept;

	  private:
		friend report<mapping> mount(path_view destination, file source);
		friend report<mapping> mount(path_view destination, volume source);
		friend class detail::mount_table;
		mapping(u64 id, path destination);

		u64 id;
		path location;
	};

	/*! @brief A move-only buffered transaction committed through a backend atomic replacement. */
	class atomic_write {
	  public:
		atomic_write(atomic_write&&) noexcept;
		atomic_write& operator=(atomic_write&&) noexcept;
		atomic_write(const atomic_write&) = delete;
		atomic_write& operator=(const atomic_write&) = delete;
		~atomic_write();

		stream& output() noexcept;
		report<void> commit();
		void cancel() noexcept;

	  private:
		friend report<atomic_write> begin_atomic_write(path_view location, atomic_write_options options);
		atomic_write(stream output, std::shared_ptr<vector<u08>> staging, std::shared_ptr<detail::volume_backend> backend, path destination, atomic_write_options options);

		optional<stream> output_stream;
		std::shared_ptr<vector<u08>> staging_bytes;
		std::shared_ptr<detail::volume_backend> target;
		path destination;
		atomic_write_options options;
		bool complete;
	};

	/*! @brief Wraps immutable caller-owned bytes as a fixed-size read-only file. */
	file borrow(span<const u08> bytes);
	/*! @brief Wraps mutable caller-owned bytes as a fixed-size writable file. */
	file borrow(span<u08> bytes);
	/*! @brief Opens one native file as a type-erased file source. */
	report<file> native_file(native_path source, open_options options = open_options());
	/*! @brief Opens one native directory tree as a mountable volume. */
	report<volume> native_volume(native_path source, native_volume_options options = native_volume_options());
	/*! @brief Mounts an exact file path and returns its lifetime token. */
	report<mapping> mount(path_view destination, file source);
	/*! @brief Mounts a volume root and returns its lifetime token. */
	report<mapping> mount(path_view destination, volume source);
	/*! @brief Convenience predicate; use status for diagnostic errors. */
	bool exists(path_view location) noexcept;
	/*! @brief Gets metadata for a visible node. */
	report<node_status> status(path_view location);
	/*! @brief Opens a visible file node. */
	report<file> open_file(path_view location, open_options options = open_options());
	/*! @brief Opens a visible directory as a mountable subvolume. */
	report<volume> open_volume(path_view location);
	/*! @brief Lists immediate visible children in ascending name order. */
	report<vector<directory_entry>> list(path_view location);
	/*! @brief Creates a visible directory path in the selected writable volume. */
	report<void> create_directories(path_view location);
	/*! @brief Removes one visible node, optionally recursively. */
	report<void> remove(path_view location, recursion recursive = recursion::shallow);
	/*! @brief Atomically renames within one concrete writable volume. */
	report<void> rename(path_view source, path_view destination, existing_action existing = existing_action::fail);
	/*! @brief Copies visible files or directory trees. */
	report<void> copy(path_view source, path_view destination, copy_options options = copy_options());
	/*! @brief Moves within one volume or explicitly copies across volumes. */
	report<void> move(path_view source, path_view destination, move_options options = move_options());
	/*! @brief Reads one visible file in full. */
	report<vector<u08>> read_all(path_view location);
	/*! @brief Reads one visible file in full with a hard size limit. */
	report<vector<u08>> read_all(path_view location, usize maximum_size);
	/*! @brief Writes bytes through the selected writable volume. */
	report<void> write_all(path_view location, span<const u08> bytes, open_options options = open_options(file_access::write, file_disposition::create_or_truncate));
	/*! @brief Updates node timestamps supported by the selected backend. */
	report<void> set_times(path_view location, const file_times& times);
	/*! @brief Begins a buffered write committed through a true backend replacement. */
	report<atomic_write> begin_atomic_write(path_view location, atomic_write_options options = atomic_write_options());
	/*! @brief Replaces a file atomically when the selected backend supports it. */
	report<void> write_all_atomic(path_view location, span<const u08> bytes, atomic_write_options options = atomic_write_options());

	namespace detail {
		class stream_backend {
		  public:
			virtual ~stream_backend() = default;
			virtual bool readable() const noexcept = 0;
			virtual bool writable() const noexcept = 0;
			virtual bool seekable() const noexcept = 0;
			virtual bool resizable() const noexcept = 0;
			virtual u64 position() const noexcept = 0;
			virtual report<usize> read(span<u08> destination) = 0;
			virtual report<usize> write(span<const u08> source) = 0;
			virtual report<u64> seek(i64 offset, seek_origin origin) = 0;
			virtual report<void> resize(u64 size) = 0;
			virtual report<void> flush() = 0;
		};

		class file_backend {
		  public:
			virtual ~file_backend() = default;
			virtual access_mode access() const noexcept = 0;
			virtual report<node_status> status() const = 0;
			virtual report<stream> open(file_access access) const = 0;
		};

		class volume_backend {
		  public:
			virtual ~volume_backend() = default;
			virtual access_mode access() const noexcept = 0;
			virtual report<node_status> status(const path& location) const = 0;
			virtual report<file> open_file(const path& location, open_options options) = 0;
			virtual report<vector<directory_entry>> list(const path& location) const = 0;
			virtual report<void> create_directories(const path& location) = 0;
			virtual report<void> remove(const path& location, recursion recursive) = 0;
			virtual report<void> rename(const path& source, const path& destination, existing_action existing) = 0;
			virtual report<void> set_times(const path& location, const file_times& times) = 0;
			virtual report<void> write_all_atomic(const path& location, span<const u08> bytes, atomic_write_options options) = 0;
		};

		template<typename Backend>
		class stream_backend_model final : public stream_backend {
		  public:
			explicit stream_backend_model(Backend value);
			bool readable() const noexcept override;
			bool writable() const noexcept override;
			bool seekable() const noexcept override;
			bool resizable() const noexcept override;
			u64 position() const noexcept override;
			report<usize> read(span<u08> destination) override;
			report<usize> write(span<const u08> source) override;
			report<u64> seek(i64 offset, seek_origin origin) override;
			report<void> resize(u64 size) override;
			report<void> flush() override;

		  private:
			Backend implementation;
			u64 offset;
		};

		template<typename Backend>
		class file_backend_model final : public file_backend {
		  public:
			explicit file_backend_model(Backend value);
			access_mode access() const noexcept override;
			report<node_status> status() const override;
			report<stream> open(file_access access) const override;

		  private:
			Backend implementation;
		};

		template<typename Backend>
		class volume_backend_model final : public volume_backend {
		  public:
			explicit volume_backend_model(Backend value);
			access_mode access() const noexcept override;
			report<node_status> status(const path& location) const override;
			report<file> open_file(const path& location, open_options options) override;
			report<vector<directory_entry>> list(const path& location) const override;
			report<void> create_directories(const path& location) override;
			report<void> remove(const path& location, recursion recursive) override;
			report<void> rename(const path& source, const path& destination, existing_action existing) override;
			report<void> set_times(const path& location, const file_times& times) override;
			report<void> write_all_atomic(const path& location, span<const u08> bytes, atomic_write_options options) override;

		  private:
			Backend implementation;
		};
	}

	template<typename Backend>
	stream::stream(Backend value) : backend(make_unique<detail::stream_backend_model<std::decay_t<Backend>>>(std::move(value))) {}

	template<typename Backend>
	file::file(Backend value) : backend(std::make_shared<detail::file_backend_model<std::decay_t<Backend>>>(std::move(value))) {}

	template<typename Backend>
	volume::volume(Backend value) : backend(std::make_shared<detail::volume_backend_model<std::decay_t<Backend>>>(std::move(value))) {}

	template<typename Backend>
	detail::stream_backend_model<Backend>::stream_backend_model(Backend value) : implementation(std::move(value)), offset(0) {}

	template<typename Backend>
	bool detail::stream_backend_model<Backend>::readable() const noexcept {
		if constexpr (requires(const Backend& value) { value.readable(); }) {
			return implementation.readable();
		}
		return requires(Backend& value, span<u08> bytes) { value.read(bytes); };
	}

	template<typename Backend>
	bool detail::stream_backend_model<Backend>::writable() const noexcept {
		if constexpr (requires(const Backend& value) { value.writable(); }) {
			return implementation.writable();
		}
		return requires(Backend& value, span<const u08> bytes) { value.write(bytes); };
	}

	template<typename Backend>
	bool detail::stream_backend_model<Backend>::seekable() const noexcept {
		if constexpr (requires(const Backend& value) { value.seekable(); }) {
			return implementation.seekable();
		}
		return requires(Backend& value) { value.seek(i64(), seek_origin::begin); };
	}

	template<typename Backend>
	bool detail::stream_backend_model<Backend>::resizable() const noexcept {
		if constexpr (requires(const Backend& value) { value.resizable(); }) {
			return implementation.resizable();
		}
		return requires(Backend& value) { value.resize(u64()); };
	}

	template<typename Backend>
	u64 detail::stream_backend_model<Backend>::position() const noexcept {
		if constexpr (requires(const Backend& value) { value.position(); }) {
			return implementation.position();
		}
		return offset;
	}

	template<typename Backend>
	report<usize> detail::stream_backend_model<Backend>::read(span<u08> destination) {
		if constexpr (requires(Backend& value, span<u08> bytes) { value.read(bytes); }) {
			report<usize> result = implementation.read(destination);
			if (result) {
				offset += *result;
			}
			return result;
		}
		return unexpected(error(error_code::unsupported_operation, "stream is not readable"));
	}

	template<typename Backend>
	report<usize> detail::stream_backend_model<Backend>::write(span<const u08> source) {
		if constexpr (requires(Backend& value, span<const u08> bytes) { value.write(bytes); }) {
			report<usize> result = implementation.write(source);
			if (result) {
				offset += *result;
			}
			return result;
		}
		return unexpected(error(error_code::read_only, "stream is not writable"));
	}

	template<typename Backend>
	report<u64> detail::stream_backend_model<Backend>::seek(i64 value, seek_origin origin) {
		if constexpr (requires(Backend& backend) { backend.seek(i64(), seek_origin::begin); }) {
			report<u64> result = implementation.seek(value, origin);
			if (result) {
				offset = *result;
			}
			return result;
		}
		return unexpected(error(error_code::unsupported_operation, "stream is not seekable"));
	}

	template<typename Backend>
	report<void> detail::stream_backend_model<Backend>::resize(u64 size) {
		if constexpr (requires(Backend& value) { value.resize(u64()); }) {
			return implementation.resize(size);
		}
		return unexpected(error(error_code::unsupported_operation, "stream is not resizable"));
	}

	template<typename Backend>
	report<void> detail::stream_backend_model<Backend>::flush() {
		if constexpr (requires(Backend& value) { value.flush(); }) {
			return implementation.flush();
		}
		return {};
	}

	template<typename Backend>
	detail::file_backend_model<Backend>::file_backend_model(Backend value) : implementation(std::move(value)) {}

	template<typename Backend>
	access_mode detail::file_backend_model<Backend>::access() const noexcept { return implementation.access(); }

	template<typename Backend>
	report<node_status> detail::file_backend_model<Backend>::status() const { return implementation.status(); }

	template<typename Backend>
	report<stream> detail::file_backend_model<Backend>::open(file_access access) const { return implementation.open(access); }

	template<typename Backend>
	detail::volume_backend_model<Backend>::volume_backend_model(Backend value) : implementation(std::move(value)) {}

	template<typename Backend>
	access_mode detail::volume_backend_model<Backend>::access() const noexcept { return implementation.access(); }

	template<typename Backend>
	report<node_status> detail::volume_backend_model<Backend>::status(const path& location) const { return implementation.status(location); }

	template<typename Backend>
	report<file> detail::volume_backend_model<Backend>::open_file(const path& location, open_options options) { return implementation.open_file(location, options); }

	template<typename Backend>
	report<vector<directory_entry>> detail::volume_backend_model<Backend>::list(const path& location) const { return implementation.list(location); }

	template<typename Backend>
	report<void> detail::volume_backend_model<Backend>::create_directories(const path& location) {
		if constexpr (requires(Backend& value, const path& path) { value.create_directories(path); }) {
			return implementation.create_directories(location);
		}
		return unexpected(error(error_code::read_only, "volume is read-only"));
	}

	template<typename Backend>
	report<void> detail::volume_backend_model<Backend>::remove(const path& location, recursion recursive) {
		if constexpr (requires(Backend& value, const path& path) { value.remove(path, recursion::shallow); }) {
			return implementation.remove(location, recursive);
		}
		return unexpected(error(error_code::read_only, "volume is read-only"));
	}

	template<typename Backend>
	report<void> detail::volume_backend_model<Backend>::rename(const path& source, const path& destination, existing_action existing) {
		if constexpr (requires(Backend& value, const path& from, const path& to) { value.rename(from, to, existing_action::fail); }) {
			return implementation.rename(source, destination, existing);
		}
		return unexpected(error(error_code::unsupported_operation, "volume cannot rename"));
	}

	template<typename Backend>
	report<void> detail::volume_backend_model<Backend>::set_times(const path& location, const file_times& times) {
		if constexpr (requires(Backend& value, const path& path, const file_times& value_times) { value.set_times(path, value_times); }) {
			return implementation.set_times(location, times);
		}
		return unexpected(error(error_code::unsupported_operation, "volume cannot set timestamps"));
	}

	template<typename Backend>
	report<void> detail::volume_backend_model<Backend>::write_all_atomic(const path& location, span<const u08> bytes, atomic_write_options options) {
		if constexpr (requires(Backend& value, const path& path, span<const u08> data) { value.write_all_atomic(path, data, atomic_write_options()); }) {
			return implementation.write_all_atomic(location, bytes, options);
		}
		return unexpected(error(error_code::unsupported_operation, "volume cannot replace files atomically"));
	}
} // namespace lf::fs

