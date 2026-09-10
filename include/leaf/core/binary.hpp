#pragma once

#include "leaf/core/schema.hpp"
#include "leaf/core/array.hpp"
#include "leaf/core/concepts.hpp"
#include "leaf/core/error.hpp"
#include "leaf/core/identifier.hpp"
#include "leaf/core/optional.hpp"
#include "leaf/core/progress.hpp"
#include "leaf/core/span.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/types.hpp"
#include "leaf/core/unit.hpp"
#include "leaf/core/unordered_map.hpp"
#include "leaf/core/vector.hpp"
#include "leaf/core/version.hpp"
#include <bit>
#include <concepts>
#include <cstring>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>
#include <leaf/core/math/dim.hpp>
#include <leaf/core/math/pos.hpp>
#include <leaf/core/math/vec.hpp>

template<typename...>
lf::error process(...) requires false;

namespace lf::bin {
	struct size;
}

template<>
struct lf::type_name_trait<lf::bin::size> {
	static constexpr const char* get();
};

namespace lf::bin {
	using ::process;
	template<typename Stream, typename Value, typename... Args>
	error process(Stream&, Value&, Args&...) requires false;

	struct size;
	struct read_limits;
	struct read_options;
	struct write_options;
	struct read_stream_tag;
	struct write_stream_tag;
	struct reference;
	struct reference_state;
	template<typename T>
	struct ref;
	template<size_t Amount>
	struct padding;
	template<typename T, typename Predicate>
	struct presence;
	template<typename Enum>
	struct enum_validator;
	struct write_refs;
	struct read_refs;
	struct read_stream;
	struct write_stream;
	struct fixed_write_stream;
	struct measure_stream;

	template<typename T, typename Concrete>
	concept data = std::same_as<std::remove_cvref_t<T>, Concrete>;

	template<typename T, template<typename...> typename Container>
	concept specialization_of = instantiation_of<std::remove_cvref_t<T>, Container>;

	template<typename T>
	concept fixed_binary_integer =
		std::same_as<std::remove_cvref_t<T>, u08> ||
		std::same_as<std::remove_cvref_t<T>, u16> ||
		std::same_as<std::remove_cvref_t<T>, u32> ||
		std::same_as<std::remove_cvref_t<T>, u64> ||
		std::same_as<std::remove_cvref_t<T>, i08> ||
		std::same_as<std::remove_cvref_t<T>, i16> ||
		std::same_as<std::remove_cvref_t<T>, i32> ||
		std::same_as<std::remove_cvref_t<T>, i64>;

	template<typename T>
	concept fixed_binary_float =
		std::same_as<std::remove_cvref_t<T>, f32> ||
		std::same_as<std::remove_cvref_t<T>, f64>;

	template<typename T>
	concept binary_enum = std::is_enum_v<std::remove_cvref_t<T>>;

	template<typename T>
	inline constexpr bool trivially_binary_serializable = false;
	template<>
	inline constexpr bool trivially_binary_serializable<lf::byte> = true;

	template<typename T>
	concept trivial_binary_data =
		std::is_trivially_copyable_v<std::remove_cvref_t<T>> &&
		trivially_binary_serializable<std::remove_cvref_t<T>> &&
		!fixed_binary_integer<T> && !fixed_binary_float<T> &&
		!data<T, bool> && !binary_enum<T>;

	template<typename T>
	concept bulk_binary_element = fixed_binary_integer<T> || fixed_binary_float<T> || trivial_binary_data<T>;

	template<typename Stream>
	concept readable_byte_stream = std::same_as<typename std::remove_cvref_t<Stream>::stream_tag, read_stream_tag>;
	template<typename Stream>
	concept writable_byte_stream = std::same_as<typename std::remove_cvref_t<Stream>::stream_tag, write_stream_tag>;
	template<typename Stream>
	concept byte_stream = readable_byte_stream<Stream> || writable_byte_stream<Stream>;

	struct size {
		size_t value = 0;
		constexpr size();
		constexpr size(size_t value);
		constexpr operator size_t() const;
		constexpr bool operator==(const size& other) const;
	};

	struct read_limits {
		size_t max_string_bytes = std::numeric_limits<size_t>::max();
		size_t max_vector_elements = std::numeric_limits<size_t>::max();
	};

	struct read_options {
		read_limits limits;
		optional<Progress> progress;
	};

	struct write_options {
		optional<Progress> progress;
	};

	struct reference_state {
		reference* value = nullptr;
	};

	template<typename T>
	struct ref {
		using value_type = T;
		reference_state* state = nullptr;
		T* get() const;
		T& operator*() const;
		T* operator->() const;
		explicit operator bool() const;
	};

	/* Graph values derive from reference. The state is stable across a move,
	** while its value pointer is rebound to the new object address. */
	struct reference {
		ref<reference> identity;

		reference();
		reference(const reference&);
		reference(reference&& other) noexcept;
		reference& operator=(const reference&);
		reference& operator=(reference&& other) noexcept;
		~reference();
	};

	template<size_t Amount>
	struct padding {
		static constexpr size_t amount = Amount;
	};

	template<typename T, typename Predicate>
	struct presence {
		string_view name;
		T& value;
		Predicate present;
		T fallback;
	};

	template<typename T, typename Predicate>
	presence<T, Predicate> maybe(string_view name, T& value, Predicate present, T fallback = {});

	template<typename Enum>
	struct enum_validator {
		static constexpr bool is_valid(Enum);
	};

	struct read_stream_tag {};
	struct write_stream_tag {};

	struct write_refs {
		unordered_map<const reference_state*, u64> ids;
		unordered_map<const reference_state*, u64> definitions;
		u64 next = 0;
		u64 id_of(const reference_state* value);
		u64 define(const reference_state* value);
		error validate() const;
	};

	struct read_refs {
		struct fixup {
			reference_state** slot;
			u64 id;
		};
		unordered_map<u64, reference_state*> definitions;
		vector<fixup> fixups;
		error define(u64 id, reference_state* value);
		template<typename T>
		void defer(ref<T>* slot, u64 id);
		error resolve();
	};

	struct read_stream {
		using stream_tag = read_stream_tag;
		explicit read_stream(span<const lf::byte> input, read_limits limits = {});
		error bytes(lf::byte* output, size_t count);
		error padding(size_t count);
		template<schema_node... Fields> error operator()(Fields&&... fields);
		size_t cursor() const;
		size_t remaining() const;
		const read_limits& limits() const;
		read_refs& refs();
		const string& context() const;
		void set_context(string value);
		void set_progress(optional<Progress> value);
		void add_progress_total(size_t value);
		void advance_progress(size_t value = 1);
	  private:
		span<const lf::byte> input;
		read_limits limits_value;
		size_t cursor_value = 0;
		read_refs refs_value;
		string context_value;
		optional<Progress> progress_value;
	};

	struct write_stream {
		using stream_tag = write_stream_tag;
		error bytes(const lf::byte* input, size_t count);
		error padding(size_t count);
		template<schema_node... Fields> error operator()(Fields&&... fields);
		const vector<lf::byte>& written() const;
		vector<lf::byte> take_written();
		write_refs& refs();
		const string& context() const;
		void set_context(string value);
		void set_progress(optional<Progress> value);
		void add_progress_total(size_t value);
		void advance_progress(size_t value = 1);
	  private:
		vector<lf::byte> output;
		write_refs refs_value;
		string context_value;
		optional<Progress> progress_value;
	};

	struct fixed_write_stream {
		using stream_tag = write_stream_tag;
		explicit fixed_write_stream(span<lf::byte> output);
		span<const lf::byte> written() const;
		error bytes(const lf::byte* input, size_t count);
		error padding(size_t count);
		template<schema_node... Fields> error operator()(Fields&&... fields);
		write_refs& refs();
		const string& context() const;
		void set_context(string value);
		void set_progress(optional<Progress> value);
		void add_progress_total(size_t value);
		void advance_progress(size_t value = 1);
	  private:
		span<lf::byte> output;
		size_t cursor_value = 0;
		write_refs refs_value;
		string context_value;
		optional<Progress> progress_value;
	};

	struct measure_stream {
		using stream_tag = write_stream_tag;
		error bytes(const lf::byte*, size_t count);
		error padding(size_t count);
		template<schema_node... Fields> error operator()(Fields&&... fields);
		size_t size() const;
		write_refs& refs();
		const string& context() const;
		void set_context(string value);
		void set_progress(optional<Progress> value);
		void add_progress_total(size_t value);
		void advance_progress(size_t value = 1);
	  private:
		size_t count_value = 0;
		write_refs refs_value;
		string context_value;
		optional<Progress> progress_value;
	};

	namespace detail {
		template<byte_stream Stream>
		struct context_scope {
			Stream& stream;
			string previous;
			context_scope(Stream& stream, string_view name);
			~context_scope();
		};
}

	template<byte_stream Stream, typename Value, typename Default, lf::version Version, typename... Args>
	error process(Stream& stream, const field_node<Value, Default, schema_version<Version>, Args...>& field);
	template<byte_stream Stream, typename Value, typename Default, typename... Args>
	error process(Stream& stream, const field_node<Value, Default, Args...>& field);
	template<byte_stream Stream, typename... Fields>
	error process(Stream& stream, const group_node<Fields...>& group);
	template<byte_stream Stream, typename Controller, typename Condition, typename... Children>
	error process(Stream& stream, const conditional_node<Controller, Condition, Children...>& node);
	template<byte_stream Stream, typename Condition, typename... Children>
	error process(Stream& stream, const when_node<Condition, Children...>& node);
	template<byte_stream Stream, data<lf::byte> byte>
	error process(Stream& stream, size count, byte* data);
	template<byte_stream Stream, bulk_binary_element Element>
	error process(Stream& stream, span<Element> values);
	template<byte_stream Stream, fixed_binary_integer Integer>
	error process(Stream& stream, Integer& value);
	template<byte_stream Stream, fixed_binary_float Float>
	error process(Stream& stream, Float& value);
	template<byte_stream Stream, data<bool> Bool>
	error process(Stream& stream, Bool& value);
	template<byte_stream Stream, data<size> size>
	error process(Stream& stream, size& value);
	template<byte_stream Stream, binary_enum Enum>
	error process(Stream& stream, Enum& value);
	template<byte_stream Stream, trivial_binary_data Value>
	error process(Stream& stream, Value& value);
	template<byte_stream Stream, data<string> string>
	error process(Stream& stream, string& value);
	template<byte_stream Stream, specialization_of<vector> vector>
	error process(Stream& stream, vector& value);
	template<byte_stream Stream, specialization_of<optional> optional>
	error process(Stream& stream, optional& value);
	template<byte_stream Stream, typename Element, size_t Count>
	error process(Stream& stream, std::array<Element, Count>& value);
	template<writable_byte_stream Stream, typename Element, size_t Count>
	error process(Stream& stream, const std::array<Element, Count>& value);
	template<byte_stream Stream, specialization_of<unordered_map> unordered_map>
	error process(Stream& stream, unordered_map& value);
	template<byte_stream Stream, specialization_of<ref> ref>
	error process(Stream& stream, ref& value);
	template<byte_stream Stream, specialization_of<::lf::identifier> identifier>
	error process(Stream& stream, identifier& value);
	template<byte_stream Stream, specialization_of<unit> unit>
	error process(Stream& stream, unit& value);
	template<byte_stream Stream, specialization_of<pos2> pos2>
	error process(Stream& stream, pos2& value);
	template<byte_stream Stream, specialization_of<dim2> dim2>
	error process(Stream& stream, dim2& value);
	template<byte_stream Stream, data<version> version>
	error process(Stream& stream, version& value);
	template<byte_stream Stream, size_t Amount>
	error process(Stream& stream, padding<Amount>& value);
	template<typename T>
	report<T> read(span<const lf::byte> bytes, read_options options = {});
	template<typename T>
	report<size_t> measure(const T& value, optional<Progress> progress = {});
	template<typename T>
	report<vector<lf::byte>> write(const T& value, write_options options = {});
	template<typename T>
	error write_to(span<lf::byte> output, const T& value, write_options options = {});

	namespace detail {
		template<byte_stream Stream>
		context_scope<Stream>::context_scope(Stream& stream, string_view name)
			: stream(stream), previous(stream.context()) {
			if (!name.empty()) {
				string next = previous;
				if (!next.empty()) {
					next += " : ";
				}
				next.append(name.data(), name.size());
				stream.set_context(std::move(next));
			}
		}

		template<byte_stream Stream>
		context_scope<Stream>::~context_scope() {
			stream.set_context(std::move(previous));
		}

		template<byte_stream Stream>
		string message(const Stream& stream, string_view value) {
			if (stream.context().empty()) {
				return string(value);
			}
			return lf::format("{} : {}", stream.context(), value);
		}

		template<typename Vector>
		error validate_allocation(const Vector& value, size_t count, const read_limits& limits) {
			if (count > limits.max_vector_elements || count > value.max_size()) {
				return error(generic_errc::parse_error, "container element count exceeds limit");
			}
			if constexpr (sizeof(typename Vector::value_type) != 0) {
				if (count > std::numeric_limits<size_t>::max() / sizeof(typename Vector::value_type)) {
					return error(generic_errc::parse_error, "container byte count overflows size_t");
				}
			}
			return {};
		}

		template<lf::version Version, byte_stream Stream, typename Value, typename... Args>
		error source_schema(Stream& stream, Value& value, lf::version source, Args&... args) {
			using value_t = std::remove_cvref_t<Value>;
			if (source == Version) {
				if constexpr (requires { { process(stream, value, lf::schema_version<Version>{}, args...) } -> std::same_as<error>; }) {
					return process(stream, value, lf::schema_version<Version>{}, args...);
				}
				return stream(lf::schema<Version>(value));
			}
			if constexpr (lf::detail::has_migration_source<value_t, Version>) {
				return source_schema<lf::migration_source<value_t, Version>>(stream, value, source, args...);
			}
			return error(generic_errc::parse_error, message(stream, "unsupported schema version"));
		}
	} // namespace detail

	constexpr size::size() = default;
	constexpr size::size(size_t value) : value(value) {}
	constexpr size::operator size_t() const { return value; }
	constexpr bool size::operator==(const size& other) const { return value == other.value; }

	template<typename T, typename Predicate>
	presence<T, Predicate> maybe(string_view name, T& value, Predicate present, T fallback) {
		return { name, value, std::move(present), std::move(fallback) };
	}

	template<typename Enum>
	constexpr bool enum_validator<Enum>::is_valid(Enum) {
		return true;
	}

	inline u64 write_refs::id_of(const reference_state* value) {
		if (!value) {
			return 0;
		}
		auto [entry, inserted] = ids.try_emplace(value, next + 1);
		if (inserted) {
			++next;
		}
		return entry->second;
	}

	inline u64 write_refs::define(const reference_state* value) {
		const u64 id = id_of(value);
		definitions.emplace(value, id);
		return id;
	}

	inline error write_refs::validate() const {
		for (const auto& [value, id] : ids) {
			if (value && !definitions.contains(value)) {
				return error(generic_errc::parse_error, "graph reference has no definition");
			}
		}
		return {};
	}

	inline error read_refs::define(u64 id, reference_state* value) {
		if (id == 0 || definitions.contains(id)) {
			return error(generic_errc::parse_error, "invalid graph definition id");
		}
		definitions.emplace(id, value);
		return {};
	}

	template<typename T>
	void read_refs::defer(ref<T>* slot, u64 id) {
		fixups.push_back({ std::addressof(slot->state), id });
	}

	inline error read_refs::resolve() {
		for (const fixup& fixup : fixups) {
			auto entry = definitions.find(fixup.id);
			if (entry == definitions.end()) {
				return error(generic_errc::parse_error, "dangling graph reference id");
			}
			*fixup.slot = entry->second;
		}
		fixups.clear();
		return {};
	}

	template<typename T>
	T* ref<T>::get() const {
		return state ? static_cast<T*>(state->value) : nullptr;
	}

	template<typename T>
	T& ref<T>::operator*() const {
		return *get();
	}

	template<typename T>
	T* ref<T>::operator->() const {
		return get();
	}

	template<typename T>
	ref<T>::operator bool() const {
		return get() != nullptr;
	}

	inline reference::reference() : identity{ new reference_state{ this } } {}
	inline reference::reference(const reference&) : reference() {}
	inline reference::reference(reference&& other) noexcept : identity{ other.identity } {
		identity.state->value = this;
		other.identity.state = new reference_state{ std::addressof(other) };
	}
	inline reference& reference::operator=(const reference&) {
		return *this;
	}
	inline reference& reference::operator=(reference&& other) noexcept {
		if (this != std::addressof(other)) {
			delete identity.state;
			identity = other.identity;
			identity.state->value = this;
			other.identity.state = new reference_state{ std::addressof(other) };
		}
		return *this;
	}
	inline reference::~reference() {
		delete identity.state;
	}

	inline read_stream::read_stream(span<const lf::byte> input, read_limits limits)
		: input(input), limits_value(limits) {}

	inline fixed_write_stream::fixed_write_stream(span<lf::byte> output)
		: output(output) {}

	inline size_t read_stream::cursor() const { return cursor_value; }
	inline size_t read_stream::remaining() const { return input.size() - cursor_value; }
	inline const read_limits& read_stream::limits() const { return limits_value; }
	inline read_refs& read_stream::refs() { return refs_value; }
	inline const string& read_stream::context() const { return context_value; }
	inline void read_stream::set_context(string value) { context_value = std::move(value); }
	inline void read_stream::set_progress(optional<Progress> value) { progress_value = std::move(value); }
	inline void read_stream::add_progress_total(size_t value) { if (progress_value) { progress_value->add_total(static_cast<u64>(value)); } }
	inline void read_stream::advance_progress(size_t value) { if (progress_value) { progress_value->advance(static_cast<u64>(value)); } }

	inline const vector<lf::byte>& write_stream::written() const { return output; }
	inline vector<lf::byte> write_stream::take_written() { return std::move(output); }
	inline write_refs& write_stream::refs() { return refs_value; }
	inline const string& write_stream::context() const { return context_value; }
	inline void write_stream::set_context(string value) { context_value = std::move(value); }
	inline void write_stream::set_progress(optional<Progress> value) { progress_value = std::move(value); }
	inline void write_stream::add_progress_total(size_t value) { if (progress_value) { progress_value->add_total(static_cast<u64>(value)); } }
	inline void write_stream::advance_progress(size_t value) { if (progress_value) { progress_value->advance(static_cast<u64>(value)); } }

	inline write_refs& fixed_write_stream::refs() { return refs_value; }
	inline span<const lf::byte> fixed_write_stream::written() const { return { output.data(), cursor_value }; }
	inline const string& fixed_write_stream::context() const { return context_value; }
	inline void fixed_write_stream::set_context(string value) { context_value = std::move(value); }
	inline void fixed_write_stream::set_progress(optional<Progress> value) { progress_value = std::move(value); }
	inline void fixed_write_stream::add_progress_total(size_t value) { if (progress_value) { progress_value->add_total(static_cast<u64>(value)); } }
	inline void fixed_write_stream::advance_progress(size_t value) { if (progress_value) { progress_value->advance(static_cast<u64>(value)); } }

	inline size_t measure_stream::size() const { return count_value; }
	inline write_refs& measure_stream::refs() { return refs_value; }
	inline const string& measure_stream::context() const { return context_value; }
	inline void measure_stream::set_context(string value) { context_value = std::move(value); }
	inline void measure_stream::set_progress(optional<Progress> value) { progress_value = std::move(value); }
	inline void measure_stream::add_progress_total(size_t value) { if (progress_value) { progress_value->add_total(static_cast<u64>(value)); } }
	inline void measure_stream::advance_progress(size_t value) { if (progress_value) { progress_value->advance(static_cast<u64>(value)); } }
	inline error measure_stream::bytes(const lf::byte*, size_t count) {
		count_value += count;
		return {};
	}
	inline error measure_stream::padding(size_t count) {
		count_value += count;
		return {};
	}

	inline error read_stream::bytes(lf::byte* output, size_t count) {
		if (count > remaining()) {
			return error(generic_errc::parse_error, detail::message(*this, "reading exceeds input"));
		}
		if (count != 0) {
			std::memcpy(output, input.data() + cursor_value, count);
		}
		cursor_value += count;
		return {};
	}
	inline error read_stream::padding(size_t count) {
		if (count > remaining()) {
			return error(generic_errc::parse_error, detail::message(*this, "padding exceeds input"));
		}
		cursor_value += count;
		return {};
	}
	inline error write_stream::bytes(const lf::byte* input, size_t count) {
		output.insert(output.end(), input, input + count);
		return {};
	}
	inline error write_stream::padding(size_t count) { output.resize(output.size() + count); return {}; }
	inline error fixed_write_stream::bytes(const lf::byte* input, size_t count) {
		if (count > output.size() - cursor_value) {
			return error(generic_errc::parse_error, detail::message(*this, "writing exceeds output"));
		}
		if (count != 0) {
			std::memcpy(output.data() + cursor_value, input, count);
		}
		cursor_value += count;
		return {};
	}
	inline error fixed_write_stream::padding(size_t count) {
		if (count > output.size() - cursor_value) {
			return error(generic_errc::parse_error, detail::message(*this, "padding exceeds output"));
		}
		cursor_value += count;
		return {};
	}

	template<byte_stream Stream, typename Value, typename Default, lf::version Version, typename... Args>
	error process(Stream& stream, const field_node<Value, Default, schema_version<Version>, Args...>& field) {
		auto payload = [&]() -> error {
			return std::apply([&](const auto&, auto&... args) -> error {
				if constexpr (writable_byte_stream<Stream>) {
					if constexpr (requires { { process(stream, field.value, lf::schema_version<Version>{}, args...) } -> std::same_as<error>; }) {
						return process(stream, field.value, lf::schema_version<Version>{}, args...);
					}
					return stream(lf::schema<Version>(field.value));
				}
				const lf::version source = lf::schema_version<Version>::value;
				if (auto err = detail::source_schema<Version>(stream, field.value, source, args...); err) {
					return err;
				}
				return lf::migrate<Version>(field.value, source);
			}, field.args);
		};
		if constexpr (std::derived_from<std::remove_cvref_t<Value>, reference>) {
			u64 id = 0;
			if constexpr (writable_byte_stream<Stream>) {
				id = stream.refs().define(field.value.identity.state);
			}
			if (auto err = stream(lf::field("id", id)); err) {
				return err;
			}
			if constexpr (readable_byte_stream<Stream>) {
				field.value.identity.state->value = static_cast<reference*>(std::addressof(field.value));
				if (auto err = stream.refs().define(id, field.value.identity.state); err) {
					return err;
				}
			}
			return payload();
		}
		return payload();
	}

	template<byte_stream Stream, typename Value, typename Default, typename... Args>
	error process(Stream& stream, const field_node<Value, Default, Args...>& field) {
		auto payload = [&]() -> error {
			return std::apply([&](auto&... args) -> error {
				return process(stream, field.value, args...);
			}, field.args);
		};
		if constexpr (std::derived_from<std::remove_cvref_t<Value>, reference>) {
			u64 id = 0;
			if constexpr (writable_byte_stream<Stream>) {
				id = stream.refs().define(field.value.identity.state);
			}
			if (auto err = stream(lf::field("id", id)); err) {
				return err;
			}
			if constexpr (readable_byte_stream<Stream>) {
				field.value.identity.state->value = static_cast<reference*>(std::addressof(field.value));
				if (auto err = stream.refs().define(id, field.value.identity.state); err) {
					return err;
				}
			}
			return payload();
		}
		return payload();
	}

	template<byte_stream Stream, typename... Fields>
	error process(Stream& stream, const group_node<Fields...>& group) {
		return std::apply([&](const auto&... fields) -> error { return stream(fields...); }, group.fields);
	}
	template<byte_stream Stream, typename Controller, typename Condition, typename... Children>
	error process(Stream& stream, const conditional_node<Controller, Condition, Children...>& node) {
		if (auto err = process(stream, node.controller); err) {
			return err;
		}
		if (!node.condition(node.controller.value)) {
			return {};
		}
		return std::apply([&](const auto&... fields) -> error { return stream(fields...); }, node.children);
	}
	template<byte_stream Stream, typename Condition, typename... Children>
	error process(Stream& stream, const when_node<Condition, Children...>& node) {
		if (!node.condition()) {
			return {};
		}
		return std::apply([&](const auto&... fields) -> error { return stream(fields...); }, node.children);
	}

	template<schema_node... Fields>
	error read_stream::operator()(Fields&&... values) {
		error result;
		auto one = [&](auto&& field) -> bool {
			string_view name;
			if constexpr (requires { field.name; }) {
				name = field.name;
			}
			detail::context_scope scope{ *this, name };
			result = process(*this, field);
			if (!result) {
				advance_progress();
			}
			return !result;
		};
		add_progress_total(sizeof...(Fields));
		(one(values) && ...);
		return result;
	}
	template<schema_node... Fields>
	error write_stream::operator()(Fields&&... values) {
		error result;
		auto one = [&](auto&& field) -> bool {
			string_view name;
			if constexpr (requires { field.name; }) {
				name = field.name;
			}
			detail::context_scope scope{ *this, name };
			result = process(*this, field);
			if (!result) {
				advance_progress();
			}
			return !result;
		};
		add_progress_total(sizeof...(Fields));
		(one(values) && ...);
		return result;
	}
	template<schema_node... Fields>
	error fixed_write_stream::operator()(Fields&&... values) {
		error result;
		auto one = [&](auto&& field) -> bool {
			string_view name;
			if constexpr (requires { field.name; }) {
				name = field.name;
			}
			detail::context_scope scope{ *this, name };
			result = process(*this, field);
			if (!result) {
				advance_progress();
			}
			return !result;
		};
		add_progress_total(sizeof...(Fields));
		(one(values) && ...);
		return result;
	}
	template<schema_node... Fields>
	error measure_stream::operator()(Fields&&... values) {
		error result;
		auto one = [&](auto&& field) -> bool {
			string_view name;
			if constexpr (requires { field.name; }) {
				name = field.name;
			}
			detail::context_scope scope{ *this, name };
			result = process(*this, field);
			if (!result) {
				advance_progress();
			}
			return !result;
		};
		add_progress_total(sizeof...(Fields));
		(one(values) && ...);
		return result;
	}

	template<byte_stream Stream, data<lf::byte> byte>
	error process(Stream& stream, size count, byte* data) {
		return stream.bytes(data, count.value);
	}

	template<byte_stream Stream, bulk_binary_element Element>
	error process(Stream& stream, span<Element> values) {
		if constexpr (std::endian::native == std::endian::little || trivial_binary_data<Element>) {
			if constexpr (writable_byte_stream<Stream>) {
				if (auto err = process(stream, size{ values.size_bytes() }, reinterpret_cast<const lf::byte*>(values.data())); err) {
					return err;
				}
			} else {
				static_assert(!std::is_const_v<Element>);
				if (auto err = process(stream, size{ values.size_bytes() }, reinterpret_cast<lf::byte*>(values.data())); err) {
					return err;
				}
			}
			stream.advance_progress(values.size());
			return {};
		}
		for (Element& value : values) {
			if (auto err = process(stream, value); err) {
				return err;
			}
			stream.advance_progress();
		}
		return {};
	}

	template<byte_stream Stream, fixed_binary_integer Integer>
	error process(Stream& stream, Integer& value) {
		using integer = std::remove_cvref_t<Integer>;
		using bits = std::make_unsigned_t<integer>;
		lf::byte bytes[sizeof(integer)]{};
		if constexpr (writable_byte_stream<Stream>) {
			const bits encoded = std::bit_cast<bits>(value);
			for (size_t index = 0; index < sizeof(integer); ++index) {
				bytes[index] = static_cast<lf::byte>((encoded >> (index * 8u)) & 0xffu);
			}
			return process(stream, size{ sizeof bytes }, bytes);
		} else {
			static_assert(!std::is_const_v<Integer>);
			if (auto err = process(stream, size{ sizeof bytes }, bytes); err) {
				return err;
			}
			bits encoded = 0;
			for (size_t index = 0; index < sizeof(integer); ++index) {
				encoded |= static_cast<bits>(std::to_integer<u08>(bytes[index])) << (index * 8u);
			}
			value = std::bit_cast<integer>(encoded);
			return {};
		}
	}
	template<byte_stream Stream, fixed_binary_float Float>
	error process(Stream& stream, Float& value) {
		using floating = std::remove_cvref_t<Float>;
		using bits = std::conditional_t<sizeof(floating) == sizeof(u32), u32, u64>;
		bits encoded = 0;
		if constexpr (writable_byte_stream<Stream>) {
			encoded = std::bit_cast<bits>(value);
		}
		if (auto err = process(stream, encoded); err) {
			return err;
		}
		if constexpr (readable_byte_stream<Stream>) {
			value = std::bit_cast<floating>(encoded);
		}
		return {};
	}
	template<byte_stream Stream, data<bool> Bool>
	error process(Stream& stream, Bool& value) {
		u08 encoded = value ? 1 : 0;
		if (auto err = process(stream, encoded); err) {
			return err;
		}
		if constexpr (readable_byte_stream<Stream>) {
			if (encoded > 1) {
				return error(generic_errc::parse_error, detail::message(stream, "boolean is not canonical"));
			}
			value = encoded != 0;
		}
		return {};
	}
	template<byte_stream Stream, data<size> size>
	error process(Stream& stream, size& value) {
		if constexpr (writable_byte_stream<Stream>) {
			size_t remaining = value.value;
			do {
				u08 encoded = static_cast<u08>(remaining & 0x7fu);
				remaining >>= 7u;
				if (remaining != 0) {
					encoded |= 0x80u;
				}
				if (auto err = process(stream, encoded); err) {
					return err;
				}
			} while (remaining != 0);
			return {};
		} else {
			constexpr size_t max_bytes = (sizeof(size_t) * 8u + 6u) / 7u;
			size_t result = 0;
			for (size_t index = 0; index < max_bytes; ++index) {
				u08 encoded = 0;
				if (auto err = process(stream, encoded); err) {
					return err;
				}
				if (index == max_bytes - 1 && (encoded & 0x7fu) > ((size_t{1} << ((sizeof(size_t) * 8u - 1u) % 7u + 1u)) - 1u)) {
					return error(generic_errc::parse_error, detail::message(stream, "size varint overflows"));
				}
				result |= static_cast<size_t>(encoded & 0x7fu) << (index * 7u);
				if ((encoded & 0x80u) == 0) {
					value.value = result;
					return {};
				}
			}
			return error(generic_errc::parse_error, detail::message(stream, "size varint is too long"));
		}
	}
	template<byte_stream Stream, binary_enum Enum>
	error process(Stream& stream, Enum& value) {
		using enum_t = std::remove_cvref_t<Enum>;
		using underlying = std::underlying_type_t<enum_t>;
		underlying encoded = static_cast<underlying>(value);
		if (auto err = process(stream, encoded); err) {
			return err;
		}
		if constexpr (readable_byte_stream<Stream>) {
			value = static_cast<enum_t>(encoded);
			if (!enum_validator<enum_t>::is_valid(value)) {
				return error(generic_errc::parse_error, detail::message(stream, "enum value is invalid"));
			}
		}
		return {};
	}
	template<byte_stream Stream, trivial_binary_data Value>
	error process(Stream& stream, Value& value) {
		if constexpr (writable_byte_stream<Stream>) {
			return process(stream, size{ sizeof value }, reinterpret_cast<const lf::byte*>(std::addressof(value)));
		} else {
			static_assert(!std::is_const_v<Value>);
			return process(stream, size{ sizeof value }, reinterpret_cast<lf::byte*>(std::addressof(value)));
		}
	}

	template<byte_stream Stream, data<string> string>
	error process(Stream& stream, string& value) {
		size count{ value.size() };
		if (auto err = stream(lf::field("size", count)); err) {
			return err;
		}
		if constexpr (readable_byte_stream<Stream>) {
			if (count.value > stream.limits().max_string_bytes) {
				return error(generic_errc::parse_error, detail::message(stream, "string byte count exceeds limit"));
			}
			string result;
			result.resize(count.value);
			if (auto err = process(stream, count, reinterpret_cast<lf::byte*>(result.data())); err) {
				return err;
			}
			value = std::move(result);
			return {};
		} else {
			return process(stream, count, reinterpret_cast<const lf::byte*>(value.data()));
		}
	}

	template<byte_stream Stream, specialization_of<vector> vector>
	error process(Stream& stream, vector& value) {
		using element = typename std::remove_cvref_t<vector>::value_type;
		size count{ value.size() };
		if (auto err = stream(lf::field("size", count)); err) {
			return err;
		}
		stream.add_progress_total(count.value);
		if constexpr (readable_byte_stream<Stream>) {
			vector result;
			if (auto err = detail::validate_allocation(result, count.value, stream.limits()); err) {
				return err;
			}
			result.resize(count.value);
			if constexpr (bulk_binary_element<element>) {
				if (auto err = process(stream, span(result.data(), result.size())); err) {
					return err;
				}
			} else {
				for (size_t index = 0; index < count.value; ++index) {
					if (auto err = stream(lf::field(lf::format("[{}]", index), result[index])); err) {
						return err;
					}
				}
			}
			value = std::move(result);
			return {};
		} else if constexpr (bulk_binary_element<element>) {
			return process(stream, span(value.data(), value.size()));
		} else {
			for (size_t index = 0; index < count.value; ++index) {
				if (auto err = stream(lf::field(lf::format("[{}]", index), value[index])); err) {
					return err;
				}
			}
			return {};
		}
	}
	template<byte_stream Stream, specialization_of<optional> optional>
	error process(Stream& stream, optional& value) {
		bool present = value.has_value();
		if (auto err = stream(lf::field("present", present)); err) {
			return err;
		}
		if (!present) {
			if constexpr (readable_byte_stream<Stream>) {
				value.reset();
			}
			return {};
		}
		if constexpr (readable_byte_stream<Stream>) {
			value.emplace();
		}
		return stream(lf::field("value", *value));
	}
	template<byte_stream Stream, typename Element, size_t Count>
	error process(Stream& stream, std::array<Element, Count>& value) {
		if constexpr (bulk_binary_element<Element>) {
			return process(stream, span(value.data(), value.size()));
		}
		for (size_t index = 0; index < Count; ++index) {
			if (auto err = stream(lf::field(lf::format("[{}]", index), value[index])); err) {
				return err;
			}
		}
		return {};
	}
	template<writable_byte_stream Stream, typename Element, size_t Count>
	error process(Stream& stream, const std::array<Element, Count>& value) {
		if constexpr (bulk_binary_element<Element>) {
			return process(stream, span<const Element>{value.data(), value.size()});
		}
		for (size_t index = 0; index < Count; ++index) {
			if (auto error = stream(lf::field(lf::format("[{}]", index), value[index]))) { return error; }
		}
		return {};
	}
	template<byte_stream Stream, specialization_of<unordered_map> unordered_map>
	error process(Stream& stream, unordered_map& value) {
		using map = std::remove_cvref_t<unordered_map>;
		size count{ value.size() };
		if (auto err = stream(lf::field("size", count)); err) {
			return err;
		}
		if constexpr (readable_byte_stream<Stream>) {
			map result;
			if (auto err = detail::validate_allocation(result, count.value, stream.limits()); err) {
				return err;
			}
			for (size_t index = 0; index < count.value; ++index) {
				typename map::key_type key{};
				typename map::mapped_type mapped{};
				if (auto err = stream(lf::field("key", key), lf::field("value", mapped)); err) {
					return err;
				}
				result.emplace(std::move(key), std::move(mapped));
			}
			value = std::move(result);
			return {};
		}
		else {
			for (const auto& [key, mapped] : value) {
				if (auto err = stream(lf::field("key", key), lf::field("value", mapped)); err) {
					return err;
				}
			}
			return {};
		}
	}

	template<byte_stream Stream, specialization_of<ref> ref>
	error process(Stream& stream, ref& value) {
		using ref_t = std::remove_cvref_t<ref>;
		using element = typename ref_t::value_type;
		u64 id = 0;
		if constexpr (writable_byte_stream<Stream>) {
			id = stream.refs().id_of(value.state);
		}
		if (auto err = stream(lf::field("ref", id)); err) {
			return err;
		}
		if constexpr (readable_byte_stream<Stream>) {
			if (id == 0) {
				value.state = nullptr;
			} else {
				stream.refs().defer<element>(&value, id);
			}
		}
		return {};
	}
	template<byte_stream Stream, specialization_of<::lf::identifier> identifier>
	error process(Stream& stream, identifier& value) {
		using type = std::remove_cvref_t<identifier>;
		using vnum = typename type::vnum_t;
		using gnum = typename type::gnum_t;
		vnum id = value.get();
		if constexpr (std::same_as<gnum, void>) {
			if (auto err = stream(lf::field("id", id)); err) {
				return err;
			}
			if constexpr (readable_byte_stream<Stream>) {
				value = type::from_raw(id);
			}
			return {};
		} else {
			gnum generation{};
			if constexpr (writable_byte_stream<Stream>) {
				generation = value.gen();
			}
			if (auto err = stream(lf::field("id", id), lf::field("generation", generation)); err) {
				return err;
			}
			if constexpr (readable_byte_stream<Stream>) {
				value = type::from_raw(id, generation);
			}
			return {};
		}
	}
	template<byte_stream Stream, specialization_of<unit> unit>
	error process(Stream& stream, unit& value) { return process(stream, value.value); }
	template<byte_stream Stream, specialization_of<pos2> pos2>
	error process(Stream& stream, pos2& value) { return stream(lf::field("x", value.x), lf::field("y", value.y)); }
	template<byte_stream Stream, specialization_of<dim2> dim2>
	error process(Stream& stream, dim2& value) { return stream(lf::field("width", value.width), lf::field("height", value.height)); }
	template<byte_stream Stream, glm::length_t Length, typename T, glm::qualifier Qualifier>
	error process(Stream& stream, glm::vec<Length, T, Qualifier>& value) {
		if constexpr (Length == 2) {
			return stream(lf::field("x", value.x), lf::field("y", value.y));
		} else if constexpr (Length == 3) {
			return stream(lf::field("x", value.x), lf::field("y", value.y), lf::field("z", value.z));
		} else {
			static_assert(Length == 4);
			return stream(lf::field("x", value.x), lf::field("y", value.y), lf::field("z", value.z), lf::field("w", value.w));
		}
	}
	template<writable_byte_stream Stream, glm::length_t Length, typename T, glm::qualifier Qualifier>
	error process(Stream& stream, const glm::vec<Length, T, Qualifier>& value) {
		if constexpr (Length == 2) {
			return stream(lf::field("x", value.x), lf::field("y", value.y));
		} else if constexpr (Length == 3) {
			return stream(lf::field("x", value.x), lf::field("y", value.y), lf::field("z", value.z));
		} else {
			static_assert(Length == 4);
			return stream(lf::field("x", value.x), lf::field("y", value.y), lf::field("z", value.z), lf::field("w", value.w));
		}
	}
	template<byte_stream Stream, data<version> version>
	error process(Stream& stream, version& value) { return stream(lf::field("major", value.major), lf::field("minor", value.minor), lf::field("patch", value.patch), lf::field("snapshot", value.snapshot)); }
	template<byte_stream Stream, size_t Amount>
	error process(Stream& stream, padding<Amount>&) { return stream.padding(Amount); }

	template<typename T>
	report<T> read(span<const lf::byte> bytes, read_options options) {
		T value{};
		read_stream stream{ bytes, options.limits };
		stream.set_progress(std::move(options.progress));
		if (auto err = stream(lf::field("", value)); err) {
			return unexpected(err);
		}
		if (auto err = stream.refs().resolve(); err) {
			return unexpected(err);
		}
		if (stream.cursor() != bytes.size()) {
			return unexpected(error(generic_errc::parse_error, detail::message(stream, "trailing bytes remain")));
		}
		return value;
	}
	template<typename T>
	report<size_t> measure(const T& value, optional<Progress> progress) {
		measure_stream stream;
		stream.set_progress(std::move(progress));
		if (auto err = stream(lf::field("", value)); err) {
			return unexpected(err);
		}
		return stream.size();
	}
	template<typename T>
	report<vector<lf::byte>> write(const T& value, write_options options) {
		write_stream stream;
		stream.set_progress(std::move(options.progress));
		if (auto err = stream(lf::field("", value)); err) {
			return unexpected(err);
		}
		if (auto err = stream.refs().validate(); err) {
			return unexpected(err);
		}
		return stream.take_written();
	}
	template<typename T>
	error write_to(span<lf::byte> output, const T& value, write_options options) {
		fixed_write_stream stream{ output };
		stream.set_progress(std::move(options.progress));
		if (auto err = stream(lf::field("", value)); err) {
			return err;
		}
		return stream.refs().validate();
	}
} // namespace lf::bin

constexpr const char* lf::type_name_trait<lf::bin::size>::get() {
	return "lf::bin::size";
}
