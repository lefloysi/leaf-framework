#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace lf {
	template<typename T>
	using vec2 = glm::vec<2, T, glm::defaultp>;
	template<typename T>
	using vec3 = glm::vec<3, T, glm::defaultp>;
	template<typename T>
	using vec4 = glm::vec<4, T, glm::defaultp>;
} // namespace lf
