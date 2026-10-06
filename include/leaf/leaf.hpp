#pragma once

#include "leaf/core/error.hpp"
#include "leaf/core/progress.hpp"
#include "leaf/core/span.hpp"
#include "leaf/core/string.hpp"
#include <leaf/application/window.hpp>
#include <leaf/graphics/resource.hpp>
#include <leaf/manager/texture_atlas.hpp>
#include <functional>

namespace lf {
	void Init(span<string_view> args, string_view application = {});
	void Run(const std::function<void()>& application);
	bool Update();
	void Exit();
} // namespace lf

