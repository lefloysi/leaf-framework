#include "leaf/graphics/resource.hpp"

#include "leaf/core/exception.hpp"
#include "leaf/core/logging.hpp"
#include "leaf/graphics/graphics.hpp"

namespace rt {

	void detail::check_rutile_error(string_view context) {

		error err = rutile_error();
		if (!err) {
			return;
		}
		rtClearError();
		lf::log::Error("error : {}", err.message);
		throw runtime_exception(lf::format("{} : {}", context, err.message));
	}

} // namespace rt
