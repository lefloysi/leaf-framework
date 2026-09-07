#pragma once

#include <cstddef>
#include <cstdint>

#include "leaf/core/typename.hpp"

using u08 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i08 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;
using usize = std::size_t;
static_assert(sizeof(f32) == 4, "f32 must be 4 bytes");
static_assert(sizeof(f64) == 8, "f64 must be 8 bytes");

namespace lf {
	using byte = std::byte;
}

using ch08 = char;
static_assert(sizeof(ch08) == 1, "ch08 must be 1 byte");
using wchar = char16_t;
static_assert(sizeof(wchar) == 2, "ch16 must be 2 bytes");

template<>
struct lf::type_name_trait<u08> {
	static constexpr const char* get() {
		return "u08";
	}
};
template<>
struct lf::type_name_trait<u16> {
	static constexpr const char* get() {
		return "u16";
	}
};
template<>
struct lf::type_name_trait<u32> {
	static constexpr const char* get() {
		return "u32";
	}
};
template<>
struct lf::type_name_trait<u64> {
	static constexpr const char* get() {
		return "u64";
	}
};

// signed integers
template<>
struct lf::type_name_trait<i08> {
	static constexpr const char* get() {
		return "i08";
	}
};
template<>
struct lf::type_name_trait<i16> {
	static constexpr const char* get() {
		return "i16";
	}
};
template<>
struct lf::type_name_trait<i32> {
	static constexpr const char* get() {
		return "i32";
	}
};
template<>
struct lf::type_name_trait<i64> {
	static constexpr const char* get() {
		return "i64";
	}
};

// floating point
template<>
struct lf::type_name_trait<f32> {
	static constexpr const char* get() {
		return "f32";
	}
};
template<>
struct lf::type_name_trait<f64> {
	static constexpr const char* get() {
		return "f64";
	}
};

// character types
template<>
struct lf::type_name_trait<ch08> {
	static constexpr const char* get() {
		return "ch08";
	}
};
template<>
struct lf::type_name_trait<wchar> {
	static constexpr const char* get() {
		return "wchar";
	}
};
