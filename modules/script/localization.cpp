#include "leaf/script/localization.hpp"

#include "leaf/core/logging.hpp"
#include "leaf/core/unordered_map.hpp"
#include "leaf/script/settings.hpp"

#include <cctype>
#include <shared_mutex>
#include <sstream>

namespace lf {
	std::shared_mutex& localization_mutex() {
		static std::shared_mutex mutex;
		return mutex;
	}
	static constexpr string_view DefaultLanguage = "en-US";
	using section_map = lf::unordered_map_string<lf::unordered_map_string<string>>;

	section_map& selected_entries() {
		static section_map entries;
		return entries;
	}

	section_map& fallback_entries() {
		static section_map entries;
		return entries;
	}

	string& loaded_language() {
		static string language(DefaultLanguage);
		return language;
	}

	vector<LanguageInfo>& available_languages() {
		static vector<LanguageInfo> languages;
		return languages;
	}

	vector<ModInfo>& cached_locale_mods() {
		static vector<ModInfo> mods;
		return mods;
	}

	string trim(string_view value) {
		size_t begin = 0;
		size_t end = value.size();
		while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) {
			++begin;
		}
		while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
			--end;
		}
		return string(value.substr(begin, end - begin));
	}

	string locale_path_for_language(string_view language) {
		return string(language) + ".cfg";
	}

	error load_locale_file(const fs::path& path, section_map& entries) {
		report<vector<u08>> bytes = fs::read_all(path);
		if (!bytes) {
			return bytes.error();
		}
		string file_text(reinterpret_cast<const char*>(bytes->data()), bytes->size());

		string section;
		string line;
		size_t line_number = 0;
		std::istringstream file(file_text);
		while (std::getline(file, line)) {
			++line_number;
			if (line_number == 1 && line.starts_with("\xEF\xBB\xBF")) {
				line.erase(0, 3);
			}
			string text = trim(line);
			if (text.empty() || text[0] == '#' || text[0] == ';') {
				continue;
			}

			if (text.front() == '[') {
				if (text.back() != ']') {
					return error(generic_errc::parse_error, lf::format("{}:{} invalid locale section", path.text(), line_number));
				}
				section = trim(string_view(text).substr(1, text.size() - 2));
				if (section.empty()) {
					return error(generic_errc::parse_error, lf::format("{}:{} empty locale section", path.text(), line_number));
				}
				continue;
			}

			size_t equals = text.find('=');
			if (equals == string::npos) {
				return error(generic_errc::parse_error, lf::format("{}:{} expected key=value", path.text(), line_number));
			}
			if (section.empty()) {
				return error(generic_errc::parse_error, lf::format("{}:{} locale entry outside a section", path.text(), line_number));
			}

			string key = trim(string_view(text).substr(0, equals));
			string value = trim(string_view(text).substr(equals + 1));
			if (key.empty()) {
				return error(generic_errc::parse_error, lf::format("{}:{} empty locale key", path.text(), line_number));
			}
			entries[section][key] = value;
		}

		if (!file.eof() && file.fail()) {
			return error(generic_errc::io_error, lf::format("failed to read '{}'", path.text()));
		}
		return error::no_error;
	}

	void replace_all(string& value, string_view needle, string_view replacement) {
		if (needle.empty()) {
			return;
		}
		size_t offset = 0;
		while ((offset = value.find(needle, offset)) != string::npos) {
			value.replace(offset, needle.size(), replacement);
			offset += replacement.size();
		}
	}

	string apply_parameters(string value, span<const string> parameters) {
		for (size_t i = 0; i < parameters.size(); ++i) {
			replace_all(value, lf::format("__{}__", i + 1), parameters[i]);
		}
		return value;
	}

	LanguageInfo& ensure_language(string_view id) {
		for (LanguageInfo& language : available_languages()) {
			if (language.id == id) {
				return language;
			}
		}
		vector<LanguageInfo>& languages = available_languages();
		languages.emplace_back();
		LanguageInfo& language = languages.back();
		language.id = string(id);
		language.name = string(id);
		language.native_name = string(id);
		return language;
	}

	void apply_language_metadata(string_view id, const section_map& entries) {
		auto section_it = entries.find("language");
		if (section_it == entries.end()) {
			return;
		}

		LanguageInfo& language = ensure_language(id);
		auto name_it = section_it->second.find("name");
		if (name_it != section_it->second.end() && !name_it->second.empty()) {
			language.name = name_it->second;
		}
		auto native_name_it = section_it->second.find("native-name");
		if (native_name_it != section_it->second.end() && !native_name_it->second.empty()) {
			language.native_name = native_name_it->second;
		}
	}

	error discover_languages(span<const ModInfo> mods) {
		available_languages().clear();
		ensure_language(DefaultLanguage);

		for (const ModInfo& mod : mods) {
			auto locale_dir = mod.location.append("locale");
			if (!fs::exists(locale_dir)) {
				continue;
			}
			report<vector<fs::directory_entry>> entries = fs::list(locale_dir);
			if (!entries) {
				return entries.error();
			}
			for (const fs::directory_entry& entry : *entries) {
				string filename(entry.name());
				if (entry.status().type() != fs::node_type::file || !filename.ends_with(".cfg")) {
					continue;
				}
				string id = filename.substr(0, filename.size() - 4);
				if (id.empty()) {
					continue;
				}
				ensure_language(id);

				section_map metadata;
				auto locale_file = locale_dir.append(filename);
				if (error err = load_locale_file(locale_file, metadata)) {
					log::Warning("{}", lf::format("[locale] ignoring metadata in '{}': {}", locale_file.text(), err.message));
					continue;
				}
				apply_language_metadata(id, metadata);
			}
		}
		return error::no_error;
	}

	error load_language_into(span<const ModInfo> mods, string_view language, section_map& entries) {
		for (const ModInfo& mod : mods) {
			auto locale_path = mod.location.append("locale").append(locale_path_for_language(language));
			if (!fs::exists(locale_path)) {
				continue;
			}
			if (error err = load_locale_file(locale_path, entries)) {
				return err.add_context(lf::format("loading locale '{}'", locale_path.text()));
			}
			log::Trace("{}", lf::format("[locale] loaded: {}/locale/{}", mod.name, locale_path.filename()));
		}
		return error::no_error;
	}

	error load_core_language_into(string_view language, section_map& entries) {
		fs::path locale_path("/core/locale");
		locale_path = locale_path.append(locale_path_for_language(language));
		if (!fs::exists(locale_path)) {
			return error::no_error;
		}
		if (error err = load_locale_file(locale_path, entries)) {
			return err.add_context(lf::format("loading core locale '{}'", locale_path.text()));
		}
		return error::no_error;
	}

	error LoadLocaleFiles(span<const ModInfo> mods, string_view language) {
		std::unique_lock lock(localization_mutex());
		cached_locale_mods().assign(mods.begin(), mods.end());
		selected_entries().clear();
		fallback_entries().clear();
		loaded_language() = language.empty() ? string(DefaultLanguage) : string(language);

		IF_ERROR_RETURN_ERROR(discover_languages(mods));
		IF_ERROR_RETURN_ERROR(load_core_language_into(DefaultLanguage, fallback_entries()));
		if (loaded_language() != DefaultLanguage) {
			IF_ERROR_RETURN_ERROR(load_core_language_into(loaded_language(), selected_entries()));
		}
		IF_ERROR_RETURN_ERROR(load_language_into(mods, DefaultLanguage, fallback_entries()));
		if (loaded_language() != DefaultLanguage) {
			IF_ERROR_RETURN_ERROR(load_language_into(mods, loaded_language(), selected_entries()));
		}
		return error::no_error;
	}

	error ReloadLocaleFiles(string_view language) {
		return LoadLocaleFiles(span<const ModInfo>(cached_locale_mods().data(), cached_locale_mods().size()), language);
	}

	error SetLanguage(string_view language) {
		string selected = language.empty() ? string(DefaultLanguage) : string(language);
		if (error err = SaveSetting("core", "language", object(selected))) {
			return err;
		}
		return ReloadLocaleFiles(selected);
	}

	void ClearLocalization() {
		std::unique_lock lock(localization_mutex());
		selected_entries().clear();
		fallback_entries().clear();
		available_languages().clear();
		cached_locale_mods().clear();
		loaded_language() = string(DefaultLanguage);
	}

	string_view LoadedLanguage() {
		return loaded_language();
	}

	span<const LanguageInfo> AvailableLanguages() {
		return span<const LanguageInfo>(available_languages().data(), available_languages().size());
	}

	string_view AvailableLanguageName(string_view language) {
		for (const LanguageInfo& info : available_languages()) {
			if (info.id == language) {
				return info.name;
			}
		}
		return language;
	}

	string_view AvailableLanguageNativeName(string_view language) {
		for (const LanguageInfo& info : available_languages()) {
			if (info.id == language) {
				return info.native_name;
			}
		}
		return language;
	}

	string Localize(string_view section, string_view key) {
		return Localize(section, key, span<const string>{});
	}

	string Localize(string_view section, string_view key, span<const string> parameters) {
		std::shared_lock lock(localization_mutex());
		if (key.empty()) {
			return {};
		}

		auto section_it = selected_entries().find(string(section));
		if (section_it != selected_entries().end()) {
			auto key_it = section_it->second.find(string(key));
			if (key_it != section_it->second.end()) {
				return apply_parameters(key_it->second, parameters);
			}
		}

		section_it = fallback_entries().find(string(section));
		if (section_it != fallback_entries().end()) {
			auto key_it = section_it->second.find(string(key));
			if (key_it != section_it->second.end()) {
				return apply_parameters(key_it->second, parameters);
			}
		}
		return string(key);
	}

	string Localize(string_view section, const local_string& text) {
		return Localize(section, text.key);
	}

	string Localize(string_view section, const local_string& text, span<const string> parameters) {
		return Localize(section, text.key, parameters);
	}
} // namespace lf
