#pragma once

#include <leaf/core/dynamic_object.hpp>
#include <leaf/core/error.hpp>
#include <leaf/core/filesystem.hpp>
#include <leaf/core/string.hpp>
#include <leaf/core/types.hpp>

namespace lf {
	report<object> LoadSetting(string_view mod_name, string_view name, object fallback = {});
	error SaveSetting(string_view mod_name, string_view name, object value);
	error EnsureSetting(string_view mod_name, string_view name, object value);
	report<string> LoadInputSetting(string_view mod_name, string_view action);
	error SaveInputSetting(string_view mod_name, string_view action, string_view key);
	error EnsureInputSetting(string_view mod_name, string_view action, string_view key);
} // namespace lf
