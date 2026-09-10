#pragma once

#include "application/rml/file_interface.hpp"
#include "application/rml/context.hpp"
#include "application/rml/render_interface.hpp"
#include "application/rml/system_interface.hpp"
#include "leaf/application/rml.hpp"
#include "leaf/core/error.hpp"
#include "leaf/core/memory.hpp"
#include "leaf/core/span.hpp"
#include "leaf/core/string.hpp"
#include "leaf/core/vector.hpp"

namespace lf {
	struct RmlBackend {
		Renderer renderer;
		RmlContextInstancer context_instancer;
		RmlSystem system;
		RmlFile file;
		vector<unique_ptr<Rml::ElementInstancer>> instancers;

		error register_element(string_view tag, unique_ptr<Rml::ElementInstancer> instancer);
	};

	extern unique_ptr<RmlBackend> rml_backend;

} // namespace lf
