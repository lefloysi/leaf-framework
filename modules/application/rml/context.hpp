#pragma once

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ContextInstancer.h>

namespace lf {
	class RmlContext final : public Rml::Context {
	  public:
		RmlContext(const Rml::String& name, Rml::RenderManager* render_manager, Rml::TextInputHandler* text_input_handler);
	};

	class RmlContextInstancer final : public Rml::ContextInstancer {
	  public:
		Rml::ContextPtr InstanceContext(const Rml::String& name, Rml::RenderManager* render_manager, Rml::TextInputHandler* text_input_handler) override;
		void ReleaseContext(Rml::Context* context) override;
		void Release() override {}
	};
} // namespace lf
