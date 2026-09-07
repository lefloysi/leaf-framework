#pragma once

#include <leaf/core/error.hpp>
#include <leaf/core/span.hpp>
#include <leaf/core/string.hpp>

namespace lf {
	error init_rml(span<string_view> arguments);
	void exit_rml();
}
