#pragma once

#include "leaf/core/string_types.hpp"

namespace lf {
	struct string_parser {
		string_view str;
		size_t pos = 0;

		explicit string_parser(string_view s);

		bool eof() const;
		char peek() const;

		void skip_whitespace();
		bool consume(string_view expected);

		i64 parse_int();
		string_view parse_alpha();
		i64 parse_fraction(i64& digits);
	};

	template<typename T>
	struct pretty_string_trait {
		static string to_string(const T& value) = delete;
		static T from_string(string_view str) = delete;
	};
	template<typename T>
	string to_pretty_string(const T& value) {
		return pretty_string_trait<T>::to_string(value);
	}
	template<typename T>
	T from_pretty_string(string_view str) {
		return pretty_string_trait<T>::from_string(str);
	}
} // namespace lf

#include "leaf/core/string_api.hpp"
