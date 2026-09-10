#pragma once

#include <leaf/core/normalized.hpp>

#include <algorithm>

namespace lf {
	inline normalized<i32> periodic_value_noise(u32 position, u32 extent, u32 wavelength, u64 seed) {
		if (!extent || !wavelength) {
			return {};
		}
		const u32 count = std::max(1u, extent / wavelength);
		const u64 scaled = u64(position % extent) * count;
		const u32 sample = scaled / extent;
		constexpr u64 maximum = normalized<u32>::maximum;
		const u64 fraction = (scaled % extent) * maximum / extent;
		const u64 square = fraction * fraction / maximum;
		const u64 blend = square + 2 * (square * (maximum - fraction) / maximum);
		const auto value_at = [seed, count](u32 index) -> i64 {
			u32 hash = (index % count) ^ u32(seed) ^ u32(seed >> 32);
			hash ^= hash >> 16;
			hash *= 0x7feb352du;
			hash ^= hash >> 15;
			hash *= 0x846ca68bu;
			hash ^= hash >> 16;
			return i64(hash & 0xffffu) * (2 * i64(normalized<i32>::maximum)) / 65535 - normalized<i32>::maximum;
		};
		const i64 left = value_at(sample);
		const i64 right = value_at(sample + 1);
		const u64 difference = left < right ? right - left : left - right;
		const i64 offset = difference * blend / maximum;
		return normalized<i32>{ i32(left + (left < right ? offset : -offset)) };
	}
} // namespace lf
