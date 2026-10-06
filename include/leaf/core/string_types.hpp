#pragma once

#include "leaf/core/typename.hpp"
#include "leaf/core/types.hpp"

#include <string>
#include <string_view>

namespace lf {
	using string = std::basic_string<char>;
	using wstring = std::basic_string<wchar>;
	using string_view = std::basic_string_view<char>;
	using wstring_view = std::basic_string_view<wchar>;

	template<>
	struct type_name_trait<string> {
		static constexpr const char* get() {
			return "string";
		}
	};
	template<>
	struct type_name_trait<wstring> {
		static constexpr const char* get() {
			return "wstring";
		}
	};
	template<>
	struct type_name_trait<string_view> {
		static constexpr const char* get() {
			return "string_view";
		}
	};
	template<>
	struct type_name_trait<wstring_view> {
		static constexpr const char* get() {
			return "wstring_view";
		}
	};
} // namespace lf
