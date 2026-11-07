#include "application/rml/backend.hpp"
#ifdef LEAF_APPLICATION_EMBEDDED_FONT_HEADER
#include LEAF_APPLICATION_EMBEDDED_FONT_HEADER
#else
#include "application/embed/font.h"
#endif
#include "leaf/core/logging.hpp"
#include "leaf/core/register.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace lf {
	unique_ptr<RmlBackend> rml_backend;

	bool load_interface_font(vector<byte>& storage, const std::filesystem::path& font) {
		if (font.empty()) { return false; }
		std::ifstream file(font, std::ios::binary | std::ios::ate);
		if (!file) { return false; }
		const auto length = file.tellg();
		if (length <= 0) { return false; }
		storage.resize(static_cast<usize>(length));
		file.seekg(0);
		if (!file.read(reinterpret_cast<char*>(storage.data()), static_cast<std::streamsize>(length))) { return false; }
		return Rml::LoadFontFace({ reinterpret_cast<const Rml::byte*>(storage.data()), storage.size() }, "UI Sans", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Auto, false);
	}

	RmlElementRegistration::RmlElementRegistration(string_view tag, RmlElementInstancerFactory factory) {
		Register<RmlBackend>::add([tag = string{ tag }, factory = std::move(factory)](RmlBackend& backend) mutable {
			return backend.register_element(tag, factory());
		});
	}

	error RmlBackend::register_element(string_view tag, unique_ptr<Rml::ElementInstancer> instancer) {
		if (tag.empty() || !instancer) {
			return error(generic_errc::invalid_argument, "Rml element registrations require a tag and instancer");
		}
		Rml::Factory::RegisterElementInstancer(Rml::String(tag), instancer.get());
		instancers.emplace_back(std::move(instancer));
		return {};
	}

	error init_rml(span<string_view> args) {
		if (rml_backend) {
			return error(generic_errc::unknown, "RmlUi is already initialized");
		}

		(void)args;
		log::Debug("[leaf] Starting interface...");
		auto backend = make_unique<RmlBackend>();
		Rml::SetSystemInterface(&backend->system);
		Rml::SetRenderInterface(&backend->renderer);
		Rml::SetFileInterface(&backend->file);
		if (!Rml::Initialise()) {
			Rml::SetRenderInterface(nullptr);
			Rml::SetSystemInterface(nullptr);
			Rml::SetFileInterface(nullptr);
			return error(generic_errc::unknown, "Rml::Initialise failed");
		}

		Rml::Factory::RegisterContextInstancer(&backend->context_instancer);
		backend->cast_shadow_instancer = make_unique<CastShadowInstancer>();
		Rml::Factory::RegisterDecoratorInstancer("cast-shadow", backend->cast_shadow_instancer.get());
		if (error result = Register<RmlBackend>::install(*backend); result) {
			Rml::Shutdown();
			Rml::SetRenderInterface(nullptr);
			Rml::SetSystemInterface(nullptr);
			Rml::SetFileInterface(nullptr);
			return result;
		}
		(void)Rml::LoadFontFace({ reinterpret_cast<const Rml::byte*>(Comic_Sans_MS_ttf), sizeof(Comic_Sans_MS_ttf) }, "Comic Sans MS", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Auto, true);
		const auto* system_root = std::getenv("WINDIR");
		const std::filesystem::path fonts[] = {
			system_root ? std::filesystem::path(system_root) / "Fonts" / "segoeui.ttf" : std::filesystem::path(),
			"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
			"/System/Library/Fonts/Supplemental/Arial.ttf"
		};
		bool sans_loaded = false;
		for (const auto& font : fonts) {
			sans_loaded = load_interface_font(backend->interface_font, font);
			if (sans_loaded) { break; }
		}
		const std::filesystem::path emphasis_fonts[] = {
			system_root ? std::filesystem::path(system_root) / "Fonts" / "seguisb.ttf" : std::filesystem::path(),
			"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
			"/System/Library/Fonts/Supplemental/Arial Bold.ttf"
		};
		for (const auto& font : emphasis_fonts) {
			if (load_interface_font(backend->interface_emphasis_font, font)) { break; }
		}
		if (!sans_loaded) {
			(void)Rml::LoadFontFace({ reinterpret_cast<const Rml::byte*>(Comic_Sans_MS_ttf), sizeof(Comic_Sans_MS_ttf) }, "UI Sans", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Auto, false);
		}

		rml_backend = std::move(backend);
		return {};
	}

	void exit_rml() {
		if (!rml_backend) {
			return;
		}

		Rml::ReleaseRenderManagers();
		Rml::Shutdown();
		Rml::SetRenderInterface(nullptr);
		Rml::SetSystemInterface(nullptr);
		Rml::SetFileInterface(nullptr);
		rml_backend.reset();
	}
} // namespace lf
