#include "leaf/core/exception.hpp"
#include "leaf/core/error.hpp"

#include "leaf/core/format.hpp"

namespace lf {
	error current_exception_error(string_view context) {
		const std::exception_ptr exception = std::current_exception();
		if (!exception) {
			return { generic_errc::invalid_state, "No active exception" };
		}
		try {
			std::rethrow_exception(exception);
		} catch (const std::exception& failure) {
			return { generic_errc::unknown, context.empty() ? string(failure.what()) : lf::format("{}\n -> {}", context, failure.what()) };
		} catch (...) {
			return { generic_errc::unknown, context.empty() ? string("unknown exception") : lf::format("{}\n -> unknown exception", context) };
		}
	}

	void rethrow_with_context(string_view context) {
		try {
			throw;
		} catch (const std::exception& e) {
			throw std::runtime_error(lf::format("{}\n -> {}", context, e.what()));
		} catch (...) {
			throw std::runtime_error(lf::format("{}\n -> unknown exception", context));
		}
	}
} // namespace lf
