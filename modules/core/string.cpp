#include "leaf/core/string.hpp"
#include "leaf/core/dynamic_object.hpp"
#include "leaf/core/exception.hpp"

#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace lf {
	class literal_reader {
	  public:
		literal_reader(string_view input, size_t position) : input(input), position(position) {}

		report<object> read() {
			skip_whitespace();
			auto value = read_value();
			if (!value) {
				return value;
			}
			skip_whitespace();
			if (!eof()) {
				return fail("unexpected trailing input");
			}
			return value;
		}

		size_t cursor() const { return position; }

	  private:
		bool eof() const { return position >= input.size(); }
		char peek() const { return eof() ? '\0' : input[position]; }

		void skip_whitespace() {
			while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
				++position;
			}
		}

		bool consume(char expected) {
			if (peek() != expected) {
				return false;
			}
			++position;
			return true;
		}

		bool at_value_end() const {
			return eof() || std::isspace(static_cast<unsigned char>(peek())) || peek() == ',' || peek() == ']' || peek() == '}';
		}

		report<object> fail(string_view message) const {
			return unexpected(error(generic_errc::parse_error, lf::format("string literal at offset {}: {}", position, message)));
		}

		report<object> read_value() {
			skip_whitespace();
			if (eof()) {
				return fail("expected value");
			}
			switch (peek()) {
			case '{': return read_dict();
			case '[': return read_list();
			case '"': return read_string();
			default:
				if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek()))) {
					return read_number();
				}
				return read_word();
			}
		}

		report<object> read_dict() {
			consume('{');
			dict result;
			skip_whitespace();
			if (consume('}')) {
				return object(result);
			}
			while (true) {
				auto name = read_field_name();
				if (!name) {
					return unexpected(name.error());
				}
				skip_whitespace();
				if (!consume(':')) {
					return fail("expected ':' after object field");
				}
				auto value = read_value();
				if (!value) {
					return value;
				}
				if (!result.emplace(*name, std::move(*value)).second) {
					return fail(lf::format("duplicate object field '{}'", *name));
				}
				skip_whitespace();
				if (consume('}')) {
					return object(result);
				}
				if (!consume(',')) {
					return fail("expected ',' or '}' after object value");
				}
				skip_whitespace();
				if (peek() == '}') {
					return fail("trailing comma in object");
				}
			}
		}

		report<object> read_list() {
			consume('[');
			list result;
			skip_whitespace();
			if (consume(']')) {
				return object(result);
			}
			while (true) {
				auto value = read_value();
				if (!value) {
					return value;
				}
				result.push_back(std::move(*value));
				skip_whitespace();
				if (consume(']')) {
					return object(result);
				}
				if (!consume(',')) {
					return fail("expected ',' or ']' after list value");
				}
				skip_whitespace();
				if (peek() == ']') {
					return fail("trailing comma in list");
				}
			}
		}

		report<string> read_field_name() {
			skip_whitespace();
			if (eof() || (!std::isalpha(static_cast<unsigned char>(peek())) && peek() != '_')) {
				return unexpected(error(generic_errc::parse_error, lf::format("string literal at offset {}: expected object field", position)));
			}
			const size_t begin = position++;
			while (!eof() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
				++position;
			}
			return string(input.substr(begin, position - begin));
		}

		report<object> read_string() {
			consume('"');
			string result;
			while (!eof()) {
				const char value = input[position++];
				if (value == '"') {
					return object(result);
				}
				if (value != '\\') {
					result += value;
					continue;
				}
				if (eof()) {
					return fail("unfinished string escape");
				}
				switch (input[position++]) {
				case '"': result += '"'; break;
				case '\\': result += '\\'; break;
				case 'n': result += '\n'; break;
				case 'r': result += '\r'; break;
				case 't': result += '\t'; break;
				default: return fail("unsupported string escape");
				}
			}
			return fail("unterminated string");
		}

		report<object> read_number() {
			const size_t begin = position;
			while (!at_value_end()) {
				++position;
			}
			const string_view token = input.substr(begin, position - begin);
			const bool floating = token.find_first_of(".eE") != string_view::npos;
			if (floating) {
				auto text = string{ token };
				char* end = nullptr;
				errno = 0;
				const f64 value = std::strtod(text.c_str(), &end);
				if (errno == ERANGE || end != text.c_str() + text.size() || !std::isfinite(value)) {
					return fail("invalid floating-point number");
				}
				return object(value);
			}
			if (!token.empty() && token.front() == '-') {
				i64 value = 0;
				const auto [end, result] = std::from_chars(token.data(), token.data() + token.size(), value);
				if (result != std::errc{} || end != token.data() + token.size()) {
					return fail("invalid signed integer");
				}
				return object(value);
			}
			u64 value = 0;
			const auto [end, result] = std::from_chars(token.data(), token.data() + token.size(), value);
			if (result != std::errc{} || end != token.data() + token.size()) {
				return fail("invalid unsigned integer");
			}
			if (value <= static_cast<u64>(std::numeric_limits<i64>::max())) {
				return object(static_cast<i64>(value));
			}
			return object(value);
		}

		report<object> read_word() {
			const size_t begin = position;
			while (!at_value_end()) {
				++position;
			}
			const string_view word = input.substr(begin, position - begin);
			if (word == "true") {
				return object(true);
			}
			if (word == "false") {
				return object(false);
			}
			return fail("expected boolean, number, quoted string, list, or object");
		}

		string_view input;
		size_t position;
	};

	string_parser::string_parser(string_view s) : str(s) {}

	bool string_parser::eof() const {
		return pos >= str.size();
	}

	char string_parser::peek() const {
		if (eof()) {
			return '\0';
		}
		return str[pos];
	}

	void string_parser::skip_whitespace() {
		while (!eof() && std::isspace(peek())) {
			++pos;
		}
	}

	bool string_parser::consume(string_view expected) {
		if (str.substr(pos, expected.size()) == expected) {
			pos += expected.size();
			return true;
		}
		return false;
	}

	i64 string_parser::parse_int() {
		if (!std::isdigit(peek())) {
			throw runtime_exception("expected number");
		}

		i64 val = 0;
		while (!eof() && std::isdigit(peek())) {
			val = val * 10 + (peek() - '0');
			++pos;
		}

		return val;
	}

	string_view string_parser::parse_alpha() {
		size_t start = pos;

		while (!eof() && std::isalpha(peek())) {
			++pos;
		}

		if (start == pos) {
			throw runtime_exception("expected unit");
		}

		return str.substr(start, pos - start);
	}

	i64 string_parser::parse_fraction(i64& digits) {
		i64 frac = 0;
		digits = 0;

		while (!eof() && std::isdigit(peek())) {
			if (digits < 9) {
				frac = frac * 10 + (peek() - '0');
				++digits;
			}
			++pos;
		}

		return frac;
	}

	str::reader::reader(string_view input) : input(input) {}

	error str::reader::read(object& target) {
		auto parser = literal_reader{ input, position };
		auto value = parser.read();
		position = parser.cursor();
		if (!value) {
			return value.error();
		}
		target = std::move(*value);
		return {};
	}

	report<object> str::read_object(string_view input) {
		object target;
		auto source = reader{ input };
		if (auto value_error = source.read(target)) {
			return unexpected(std::move(value_error));
		}
		return target;
	}

} // namespace lf
