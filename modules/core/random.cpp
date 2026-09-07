#include "leaf/core/random.hpp"

namespace lf {
	u64 random_seed() {
		std::random_device device;
		return detail::random_seed(device);
	}
} // namespace lf
