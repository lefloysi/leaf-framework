#pragma once

#include <leaf/core/error.hpp>
#include <leaf/core/memory.hpp>
#include <leaf/core/span.hpp>
#include <leaf/core/string.hpp>
#include <leaf/graphics/resource.hpp>

#include <functional>

namespace Rml {
	class Element;
	class ElementInstancer;
}

namespace lf {
	using RmlElementInstancerFactory = std::function<unique_ptr<Rml::ElementInstancer>()>;

	// Registers an element factory during static initialization. Leaf installs it
	// when RmlUi starts and owns the resulting instancer for RmlUi's lifetime.
	class RmlElementRegistration {
	  public:
		RmlElementRegistration(string_view tag, RmlElementInstancerFactory factory);
	};

	error init_rml(span<string_view> arguments);
	void exit_rml();
} // namespace lf
