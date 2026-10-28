#pragma once

#include <RmlUi/Core/Decorator.h>

namespace lf {

	class CastShadowInstancer final : public Rml::DecoratorInstancer {
	public:
		CastShadowInstancer();
		Rml::SharedPtr<Rml::Decorator> InstanceDecorator(const Rml::String& name, const Rml::PropertyDictionary& properties, const Rml::DecoratorInstancerInterface& instancer_interface) override;

	private:
		Rml::PropertyId color_id;
		Rml::PropertyId x_id;
		Rml::PropertyId y_id;
		Rml::PropertyId softness_id;
	};

}

