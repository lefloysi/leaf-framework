#pragma once

#include "leaf/core/types.hpp"

#include <random>

namespace lf {
	namespace detail {
		template<std::uniform_random_bit_generator Generator>
		u64 random_seed(Generator& generator) {
			return std::uniform_int_distribution<u64>{}(generator);
		}
	} // namespace detail

	// Returns a process-local seed sourced from std::random_device. The standard
	// does not guarantee that random_device is backed by physical entropy.
	u64 random_seed();
} // namespace lf
