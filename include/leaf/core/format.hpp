#pragma once

#include "leaf/core/string_types.hpp"

#include <concepts>
#include <format>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lf {
	namespace detail {
		template<typename T>
		decltype(auto) standard_format_argument(T&& value) {
			using value_type = std::remove_cvref_t<T>;
			if constexpr (std::is_enum_v<value_type>) {
				return std::to_underlying(value);
			} else if constexpr (
				std::is_pointer_v<value_type> &&
				std::is_object_v<std::remove_pointer_t<value_type>> &&
				!std::same_as<std::remove_cv_t<std::remove_pointer_t<value_type>>, char>
			) {
				return static_cast<const void*>(value);
			} else {
				return std::forward<T>(value);
			}
		}
	} // namespace detail

	template<typename... Args>
	string format(string_view pattern, Args&&... args) {
		auto values = std::make_tuple(detail::standard_format_argument(std::forward<Args>(args))...);
		return std::apply(
			[pattern](auto&... value) {
				return std::vformat(pattern, std::make_format_args(value...));
			},
			values
		);
	}
} // namespace lf
