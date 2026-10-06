#pragma once

#include "leaf/core/types.hpp"

#include <random>

namespace lf {
	// Stateless SplitMix64 mixer for deterministic procedural samples.
	constexpr u64 random_hash(u64 value) {
		value += 0x9e3779b97f4a7c15ULL;
		value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
		value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
		return value ^ (value >> 31);
	}

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
