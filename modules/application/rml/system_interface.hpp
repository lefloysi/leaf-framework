#pragma once

#include <RmlUi/Core/SystemInterface.h>

namespace lf {
	class RmlSystem final : public Rml::SystemInterface {
	  public:
		bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
		void JoinPath(Rml::String& translated_path, const Rml::String& document_path, const Rml::String& path) override;
		void SetClipboardText(const Rml::String& text) override;
		void GetClipboardText(Rml::String& text) override;
	};
} // namespace lf
