#include "leaf/core/error.hpp"

#include <magic_enum/magic_enum.hpp>

namespace lf {
	const error error::no_error = error();
	const error error::unknown_error = error(generic_errc::unknown);
	const char* generic_error_category::name() const noexcept {
		return "generic";
	}

	string generic_error_category::message(i32 ev) const {
		return string(magic_enum::enum_name(static_cast<generic_errc>(ev)));
	}

	const error_category& generic_category() {
		static generic_error_category instance;
		return instance;
	}
	error_code make_error_code(generic_errc value) {
		return { static_cast<i32>(value), generic_category() };
	}

	const char* graphics_error_category::name() const noexcept {
		return "graphics";
	}
	string graphics_error_category::message(i32 ev) const {
		return string(magic_enum::enum_name(static_cast<graphics_errc>(ev)));
	}
	const error_category& graphics_category() {
		static graphics_error_category instance;
		return instance;
	}
	error_code make_error_code(graphics_errc value) {
		return { static_cast<i32>(value), graphics_category() };
	}

	const ch08* filesystem_error_category::name() const noexcept {
		return "filesystem";
	}

	string filesystem_error_category::message(i32 value) const {
		return string(magic_enum::enum_name(static_cast<fs::error_code>(value)));
	}

	const error_category& filesystem_category() {
		static filesystem_error_category instance;
		return instance;
	}

	error_code make_error_code(fs::error_code value) {
		return { static_cast<i32>(value), filesystem_category() };
	}

	error::operator bool() const noexcept {
		return code.value() != 0;
	}
	error& error::add_context(string_view context) {
		if (context.empty()) {
			return *this;
		}
		message = lf::format("{}\n -> {}", context, message);
		return *this;
	}

} // namespace lf
