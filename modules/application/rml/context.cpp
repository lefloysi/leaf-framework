#include "application/rml/context.hpp"

namespace lf {
	RmlContext::RmlContext(const Rml::String& name, Rml::RenderManager* render_manager, Rml::TextInputHandler* text_input_handler)
		: Rml::Context(name, render_manager, text_input_handler) {}

	Rml::ContextPtr RmlContextInstancer::InstanceContext(const Rml::String& name, Rml::RenderManager* render_manager, Rml::TextInputHandler* text_input_handler) {
		return Rml::ContextPtr{ new RmlContext{ name, render_manager, text_input_handler } };
	}

	void RmlContextInstancer::ReleaseContext(Rml::Context* context) {
		delete context;
	}
} // namespace lf
