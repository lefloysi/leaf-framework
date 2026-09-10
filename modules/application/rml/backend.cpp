#include "application/rml/backend.hpp"
#include "application/embed/font.h"
#include "leaf/core/logging.hpp"
#include "leaf/core/register.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Factory.h>

namespace lf {
	unique_ptr<RmlBackend> rml_backend;

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
		if (error result = Register<RmlBackend>::install(*backend); result) {
			Rml::Shutdown();
			Rml::SetRenderInterface(nullptr);
			Rml::SetSystemInterface(nullptr);
			Rml::SetFileInterface(nullptr);
			return result;
		}
		(void)Rml::LoadFontFace({ reinterpret_cast<const Rml::byte*>(Comic_Sans_MS_ttf), sizeof(Comic_Sans_MS_ttf) }, "Comic Sans MS", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Auto, true);

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
