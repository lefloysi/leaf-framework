#include "leaf/core/messages.hpp"

namespace lf {
	string make_missing_field_error_message(string_view field_name) {
		return lf::format("missing required field '{}'", field_name);
	}

	string make_type_mismatch_error_message(string_view expected, string_view actual) {
		return lf::format("expected type '{}', but got type '{}'", expected, actual);
	}

	namespace log {
		bool IsStructuredFieldKey(string_view field) {
			auto is_ident_start = [](char c) {
				return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
			};
			auto is_ident_continue = [&](char c) {
				return is_ident_start(c) || (c >= '0' && c <= '9');
			};

			if (field.empty() || !is_ident_start(field.front())) {
				return false;
			}
			bool expect_ident_start = false;
			for (size_t i = 1; i < field.size(); ++i) {
				const char c = field[i];
				if (expect_ident_start) {
					if (!is_ident_start(c)) {
						return false;
					}
					expect_ident_start = false;
					continue;
				}
				if (c == '.') {
					if (i + 1 == field.size()) {
						return false;
					}
					expect_ident_start = true;
					continue;
				}
				if (!is_ident_continue(c)) {
					return false;
				}
			}
			return !expect_ident_start;
		}

		string EscapeAnnotationText(string_view text) {
			string escaped;
			escaped.reserve(text.size());
			for (char c : text) {
				switch (c) {
				case '\\':
				case '{':
				case '}':
				case '|':
					escaped += '\\';
					escaped += c;
					break;
				default:
					escaped += c;
					break;
				}
			}
			return escaped;
		}

		string Field(string_view field, string_view display) {
			if (field.empty()) {
				return EscapeAnnotationText(display);
			}
			return lf::format("{{{}|{}}}", EscapeAnnotationText(display), EscapeAnnotationText(field));
		}

		string Field(string_view field, const char* display) {
			return Field(field, string_view(display ? display : ""));
		}

		string Field(string_view field, const string& display) {
			return Field(field, string_view(display));
		}

		static bool is_escaped(string_view text, size_t index) {
			size_t slashes = 0;
			while (index > slashes && text[index - slashes - 1] == '\\') {
				++slashes;
			}
			return slashes % 2 == 1;
		}

		static void append_unescaped(string& out, string_view text) {
			for (size_t i = 0; i < text.size(); ++i) {
				if (text[i] == '\\' && i + 1 < text.size()) {
					const char next = text[i + 1];
					if (next == '{' || next == '}' || next == '|' || next == '\\') {
						out += next;
						++i;
						continue;
					}
				}
				out += text[i];
			}
		}

		static void push_text_segment(vector<TextSegment>& segments, string_view text) {
			if (text.empty()) {
				return;
			}
			if (!segments.empty() && !segments.back().annotated) {
				append_unescaped(segments.back().text, text);
				return;
			}
			TextSegment segment;
			append_unescaped(segment.text, text);
			segments.push_back(std::move(segment));
		}

		vector<TextSegment> ParseAnnotatedText(string_view text) {
			vector<TextSegment> segments;
			size_t cursor = 0;
			while (cursor < text.size()) {
				const size_t open = text.find('{', cursor);
				if (open == string_view::npos) {
					push_text_segment(segments, text.substr(cursor));
					break;
				}
				if (is_escaped(text, open)) {
					push_text_segment(segments, text.substr(cursor, open - cursor + 1));
					cursor = open + 1;
					continue;
				}

				push_text_segment(segments, text.substr(cursor, open - cursor));
				size_t close = open + 1;
				bool nested_open = false;
				for (; close < text.size(); ++close) {
					if (text[close] == '{' && !is_escaped(text, close)) {
						nested_open = true;
						break;
					}
					if (text[close] == '}' && !is_escaped(text, close)) {
						break;
					}
				}
				if (close >= text.size() || nested_open) {
					push_text_segment(segments, text.substr(open, nested_open ? close - open + 1 : text.size() - open));
					cursor = nested_open ? close + 1 : text.size();
					continue;
				}

				const string_view body = text.substr(open + 1, close - open - 1);
				size_t separator = string_view::npos;
				for (size_t i = 0; i < body.size(); ++i) {
					if (body[i] == '|' && !is_escaped(body, i)) {
						separator = i;
						break;
					}
				}
				if (separator == string_view::npos || separator == 0 || separator + 1 >= body.size()) {
					push_text_segment(segments, text.substr(open, close - open + 1));
					cursor = close + 1;
					continue;
				}

				TextSegment segment;
				append_unescaped(segment.text, body.substr(0, separator));
				append_unescaped(segment.field, body.substr(separator + 1));
				segment.annotated = true;
				segment.structured_field = IsStructuredFieldKey(segment.field);
				segments.push_back(std::move(segment));
				cursor = close + 1;
			}
			return segments;
		}

		string PlainText(string_view text) {
			string plain;
			for (const TextSegment& segment : ParseAnnotatedText(text)) {
				plain += segment.text;
			}
			return plain;
		}
	} // namespace log
} // namespace lf
