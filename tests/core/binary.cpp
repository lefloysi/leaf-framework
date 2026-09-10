#include <catch2/catch_test_macros.hpp>

#include <leaf/core/binary.hpp>
#include <leaf/core/random.hpp>

#include <array>
#include <limits>

namespace leaf_test::binary {
	inline constexpr lf::version schema_version = lf::version(1, 0, 0, 0);

	struct narrow_urbg {
		using result_type = u08;

		static constexpr result_type min() { return 0; }
		static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

		result_type operator()() {
			++calls;
			return next++;
		}

		u08 next = 1;
		size_t calls = 0;
	};

	struct version_context {
		bool called = false;
	};

	struct custom_record {
		u32 value = 0;
	};

	struct fallback_record {
		u32 value = 0;
	};

	struct wrong_result_record {
		u32 value = 0;
	};

	struct identifier_target {};

	struct graph_node : lf::bin::reference {
		i32 value = 0;
		lf::bin::ref<graph_node> next;
	};

	struct graph_pair {
		graph_node first;
		graph_node second;
	};

	struct graph_base : lf::bin::reference {
		i32 base = 0;
	};

	struct graph_derived : graph_base {
		i32 derived = 0;
	};

	struct graph_base_reference {
		graph_derived value;
		lf::bin::ref<graph_base> link;
	};

	struct raw_block {
		u32 first = 0;
		u32 second = 0;
		bool operator==(const raw_block&) const = default;
	};

	template<lf::bin::byte_stream Stream>
	lf::error process(Stream& stream, custom_record& value, lf::schema_version<schema_version>, version_context& context) {
		context.called = true;
		u32 wire_value = value.value + 1;
		return stream(lf::field("custom", wire_value));
	}

	template<lf::bin::byte_stream Stream>
	bool process(Stream&, wrong_result_record&, lf::schema_version<schema_version>) {
		return true;
	}

	template<lf::bin::byte_stream Stream, lf::bin::data<graph_node> graph_node>
	lf::error process(Stream& stream, graph_node& value) {
		return stream(
			lf::field("value", value.value),
			lf::field("next", value.next)
		);
	}

	template<lf::bin::byte_stream Stream, lf::bin::data<graph_pair> graph_pair>
	lf::error process(Stream& stream, graph_pair& value) {
		return stream(
			lf::field("first", value.first),
			lf::field("second", value.second)
		);
	}

	template<lf::bin::byte_stream Stream, lf::bin::data<graph_derived> graph_derived>
	lf::error process(Stream& stream, graph_derived& value) {
		return stream(
			lf::field("base", value.base),
			lf::field("derived", value.derived)
		);
	}

	template<lf::bin::byte_stream Stream, lf::bin::data<graph_base_reference> graph_base_reference>
	lf::error process(Stream& stream, graph_base_reference& value) {
		return stream(
			lf::field("value", value.value),
			lf::field("link", value.link)
		);
	}

	template<typename T>
	lf::report<lf::vector<lf::byte>> write_versioned(T& value) {
		lf::bin::write_stream stream;
		if (lf::error err = stream(lf::field("value", value, lf::schema_version<schema_version>{}))) {
			return lf::unexpected(std::move(err));
		}
		return stream.take_written();
	}
} // namespace leaf_test::binary

template<>
inline constexpr bool lf::bin::trivially_binary_serializable<leaf_test::binary::raw_block> = true;

template<>
struct lf::schema_trait<leaf_test::binary::custom_record, leaf_test::binary::schema_version> {
	static auto get(leaf_test::binary::custom_record& value) { return lf::group(lf::field("fallback", value.value)); }
};

template<>
struct lf::schema_trait<leaf_test::binary::fallback_record, leaf_test::binary::schema_version> {
	static auto get(leaf_test::binary::fallback_record& value) { return lf::group(lf::field("fallback", value.value)); }
};

template<>
struct lf::schema_trait<leaf_test::binary::wrong_result_record, leaf_test::binary::schema_version> {
	static auto get(leaf_test::binary::wrong_result_record& value) { return lf::group(lf::field("fallback", value.value)); }
};

TEST_CASE("random seed consumes the full URBG result width") {
	leaf_test::binary::narrow_urbg generator;
	const u64 seed = lf::detail::random_seed(generator);

	REQUIRE(generator.calls >= sizeof(u64));
	REQUIRE((seed >> 32u) != 0);
}

TEST_CASE("versioned binary processing uses a compatible ADL processor or schema fallback") {
	leaf_test::binary::custom_record custom{41};
	leaf_test::binary::version_context context;
	lf::bin::write_stream custom_stream;
	REQUIRE_FALSE(custom_stream(lf::field("value", custom, lf::schema_version<leaf_test::binary::schema_version>{}, context)));
	const auto custom_bytes = custom_stream.take_written();
	REQUIRE(context.called);
	REQUIRE(custom_bytes.size() == sizeof(u32));

	leaf_test::binary::fallback_record fallback{17};
	auto fallback_bytes = leaf_test::binary::write_versioned(fallback);
	REQUIRE(fallback_bytes.has_value());
	REQUIRE(fallback_bytes->size() == sizeof(u32));

	leaf_test::binary::wrong_result_record wrong_result{23};
	auto wrong_result_bytes = leaf_test::binary::write_versioned(wrong_result);
	REQUIRE(wrong_result_bytes.has_value());
	REQUIRE(wrong_result_bytes->size() == sizeof(u32));
}

TEST_CASE("binary byte processing is an ADL process overload") {
	const std::array<lf::byte, 4> source{ lf::byte{ 1 }, lf::byte{ 2 }, lf::byte{ 3 }, lf::byte{ 4 } };
	lf::bin::write_stream writer;
	REQUIRE_FALSE(lf::bin::process(writer, lf::bin::size(source.size()), source.data()));

	std::array<lf::byte, 4> result{};
	lf::bin::read_stream reader(writer.written());
	REQUIRE_FALSE(lf::bin::process(reader, lf::bin::size(result.size()), result.data()));
	REQUIRE(result == source);
}

TEST_CASE("binary fixed scalars use little-endian bytes") {
	lf::bin::write_stream writer;
	u32 value = 0x78563412u;
	REQUIRE_FALSE(lf::bin::process(writer, value));

	const auto bytes = writer.written();
	REQUIRE(bytes.size() == sizeof(value));
	REQUIRE(std::to_integer<u08>(bytes[0]) == 0x12u);
	REQUIRE(std::to_integer<u08>(bytes[1]) == 0x34u);
	REQUIRE(std::to_integer<u08>(bytes[2]) == 0x56u);
	REQUIRE(std::to_integer<u08>(bytes[3]) == 0x78u);

	lf::bin::read_stream reader(bytes);
	u32 decoded = 0;
	REQUIRE_FALSE(lf::bin::process(reader, decoded));
	REQUIRE(decoded == value);
}

TEST_CASE("binary bool processing rejects non-canonical values") {
	const std::array<lf::byte, 1> bytes{ lf::byte{ 2 } };
	lf::bin::read_stream reader(lf::span<const lf::byte>(bytes.data(), bytes.size()));
	bool value = false;
	REQUIRE(lf::bin::process(reader, value));
}

TEST_CASE("binary size processing uses a bounded varint") {
	lf::bin::write_stream writer;
	lf::bin::size value{ 300 };
	REQUIRE_FALSE(lf::bin::process(writer, value));

	const auto bytes = writer.written();
	REQUIRE(bytes.size() == 2);
	REQUIRE(std::to_integer<u08>(bytes[0]) == 0xacu);
	REQUIRE(std::to_integer<u08>(bytes[1]) == 0x02u);

	lf::bin::read_stream reader(bytes);
	lf::bin::size decoded;
	REQUIRE_FALSE(lf::bin::process(reader, decoded));
	REQUIRE(decoded == value);
}

TEST_CASE("binary size processing rejects an overlong varint") {
	std::array<lf::byte, (sizeof(size_t) * 8u + 6u) / 7u> bytes;
	bytes.fill(lf::byte{ 0x80 });
	lf::bin::read_stream reader(bytes);
	lf::bin::size value;
	REQUIRE(lf::bin::process(reader, value));
}

TEST_CASE("binary containers use their process overloads") {
	lf::vector<u32> value{ 3, 5, 8 };
	lf::bin::write_stream writer;
	REQUIRE_FALSE(lf::bin::process(writer, value));

	lf::bin::read_stream reader(writer.written());
	lf::vector<u32> decoded;
	REQUIRE_FALSE(lf::bin::process(reader, decoded));
	REQUIRE(decoded == value);
}

TEST_CASE("binary arrays maps and raw data round-trip") {
	const std::array<u32, 3> array{ 2, 4, 6 };
	lf::unordered_map<u32, lf::string> map;
	map.emplace(1, "one");
	map.emplace(2, "two");
	leaf_test::binary::raw_block raw{ 3, 9 };

	lf::bin::write_stream writer;
	REQUIRE_FALSE(lf::bin::process(writer, array));
	REQUIRE_FALSE(lf::bin::process(writer, map));
	REQUIRE_FALSE(lf::bin::process(writer, raw));

	lf::bin::read_stream reader(writer.written());
	std::array<u32, 3> decoded_array{};
	lf::unordered_map<u32, lf::string> decoded_map;
	leaf_test::binary::raw_block decoded_raw;
	REQUIRE_FALSE(lf::bin::process(reader, decoded_array));
	REQUIRE_FALSE(lf::bin::process(reader, decoded_map));
	REQUIRE_FALSE(lf::bin::process(reader, decoded_raw));
	REQUIRE(decoded_array == array);
	REQUIRE(decoded_map == map);
	REQUIRE(decoded_raw == raw);
}

TEST_CASE("binary strings enforce their configured allocation limit") {
	lf::string value = "leaf";
	lf::bin::write_stream writer;
	REQUIRE_FALSE(lf::bin::process(writer, value));

	lf::bin::read_limits limits;
	limits.max_string_bytes = 3;
	lf::bin::read_stream reader(writer.written(), limits);
	lf::string decoded;
	REQUIRE(lf::bin::process(reader, decoded));
}

TEST_CASE("binary optional and identifier processors round-trip") {
	lf::optional<u32> optional{ 42 };
	lf::identifier<leaf_test::binary::identifier_target, u32, u16> identifier{ 12, 4 };
	lf::bin::write_stream writer;
	REQUIRE_FALSE(lf::bin::process(writer, optional));
	REQUIRE_FALSE(lf::bin::process(writer, identifier));

	lf::bin::read_stream reader(writer.written());
	lf::optional<u32> decoded_optional;
	lf::identifier<leaf_test::binary::identifier_target, u32, u16> decoded_identifier;
	REQUIRE_FALSE(lf::bin::process(reader, decoded_optional));
	REQUIRE_FALSE(lf::bin::process(reader, decoded_identifier));
	REQUIRE(decoded_optional == optional);
	REQUIRE(decoded_identifier == identifier);
}

TEST_CASE("binary progress uses an optional child Progress value") {
	lf::Progress root;
	root.add("binary");
	lf::bin::write_options options;
	options.progress.emplace(root());

	const u32 value = 42;
	auto bytes = lf::bin::write(value, std::move(options));
	REQUIRE(bytes.has_value());
	REQUIRE(root.value() == 1.0f);
}

TEST_CASE("binary references resolve through ordinary graph fields") {
	leaf_test::binary::graph_pair value;
	value.first.value = 3;
	value.first.next.state = value.second.identity.state;
	value.second.value = 8;

	auto bytes = lf::bin::write(value);
	REQUIRE(bytes.has_value());

	auto decoded = lf::bin::read<leaf_test::binary::graph_pair>(*bytes);
	REQUIRE(decoded.has_value());
	REQUIRE(decoded->first.value == 3);
	REQUIRE(decoded->second.value == 8);
	REQUIRE(decoded->first.next.get() == &decoded->second);
}

TEST_CASE("binary references reject an undefined graph target") {
	leaf_test::binary::graph_node value;
	leaf_test::binary::graph_node outside;
	value.next.state = outside.identity.state;

	auto bytes = lf::bin::write(value);
	REQUIRE_FALSE(bytes.has_value());
}

TEST_CASE("binary references resolve through a graph base") {
	leaf_test::binary::graph_base_reference value;
	value.value.base = 4;
	value.value.derived = 9;
	value.link.state = value.value.identity.state;

	auto bytes = lf::bin::write(value);
	REQUIRE(bytes.has_value());
	auto decoded = lf::bin::read<leaf_test::binary::graph_base_reference>(*bytes);
	REQUIRE(decoded.has_value());
	REQUIRE(decoded->link.get() == static_cast<leaf_test::binary::graph_base*>(&decoded->value));
}
