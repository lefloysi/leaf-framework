#pragma once

namespace lf {
	template<typename T>
	struct Singleton {
		static T& instance() {
			static T value{};
			return value;
		}
	};
} // namespace lf
