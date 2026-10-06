#include "application/rml/system_interface.hpp"

#include "leaf/core/logging.hpp"
#include "leaf/platform/platform.hpp"

namespace lf {
	bool RmlSystem::LogMessage(Rml::Log::Type type, const Rml::String& message) {
		switch (type) {
		case Rml::Log::LT_ERROR:
		case Rml::Log::LT_ASSERT:
			log::Error("[rml] {}", message);
			break;
		case Rml::Log::LT_WARNING:
			log::Warning("[rml] {}", message);
			break;
		case Rml::Log::LT_DEBUG:
			log::Debug("[rml] {}", message);
			break;
		default:
			log::Info("[rml] {}", message);
			break;
		}
		return true;
	}

	void RmlSystem::JoinPath(Rml::String& translated_path, const Rml::String& document_path, const Rml::String& path) {
		if (string_view(path).starts_with('/')) {
			translated_path = path;
			return;
		}
		Rml::SystemInterface::JoinPath(translated_path, document_path, path);
	}

	void RmlSystem::SetClipboardText(const Rml::String& text) {
		platform_clipboard_text(text);
	}

	void RmlSystem::GetClipboardText(Rml::String& text) {
		text = platform_clipboard_text();
	}
} // namespace lf
