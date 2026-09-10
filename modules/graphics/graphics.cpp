#include "leaf/graphics/graphics.hpp"

#include "leaf/config.hpp"
#include "leaf/core/logging.hpp"

#include <CLI/CLI.hpp>

#include <rt_glfw_swapchain.h>
#include <rt_swapchain.h>
#include <rutile.h>

#include <cstdlib>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

namespace rt {
	constexpr string_view DefaultGraphicsAPI = "rt-vulkan";

	error parse_graphics_backend(span<string_view> args, string& backend) {
		std::vector<std::string> arguments;
		arguments.reserve(args.size());
		for (string_view argument : args) {
			arguments.emplace_back(argument);
		}

		std::string value;
		CLI::App cli;
		cli.allow_extras();
		cli.add_option("-g,--graphics", value, "Graphics backend");
		try {
			std::reverse(arguments.begin(), arguments.end());
			cli.parse(arguments);
		} catch (const CLI::ParseError& error) {
			return { generic_errc::invalid_argument, error.what() };
		}
		if (!value.empty()) {
			backend = value;
		}
		return {};
	}

	void rutile_log_output(const char* message, void*) {
		if (!message || !message[0]) {
			return;
		}
		string_view text(message);
		while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
			text.remove_suffix(1);
		}
		if (!text.empty()) {
			lf::log::Debug("{}", text);
		}
	}
	static error rutile_error(enum rt_error err, string_view context) {
		if (err == RT_SUCCESS) {
			return error::no_error;
		}

		error_code code;
		switch (err) {
		case RT_OUT_OF_HOST_MEMORY:					  /*********/
			code = graphics_errc::out_of_host_memory; /*****/
			break;
		case RT_OUT_OF_DEVICE_MEMORY:					/********/
			code = graphics_errc::out_of_device_memory; /***/
			break;
		case RT_IMPROPER_USAGE:						/*************/
			code = graphics_errc::invalid_argument; /*******/
			break;
		case RT_PLATFORM_FAILURE:					/************/
			code = graphics_errc::platform_failure; /*******/
			break;
		case RT_DEVICE_LOST:				   /****************/
			code = graphics_errc::device_lost; /************/
			break;
		case RT_ALREADY_INITIALIZED:			  /********/
			code = graphics_errc::already_exists; /*********/
			break;
		case RT_NO_BACKEND:						 /*****************/
			code = graphics_errc::not_supported; /**********/
			break;
		case RT_UNSUPPORTED_PLATFORM:			 /*******/
			code = graphics_errc::not_supported; /**********/
			break;
		case RT_UNSUPPORTED_FEATURE:				   /********/
			code = graphics_errc::unsupported_feature; /****/
			break;
		case RT_INITIALIZATION_FAILED:			  /******/
			code = graphics_errc::initialization; /*********/
			break;
		case RT_LAYER_NOT_PRESENT:					 /**********/
			code = graphics_errc::layer_not_present; /******/
			break;
		case RT_EXTENSION_NOT_PRESENT:					 /******/
			code = graphics_errc::extension_not_present; /**/
			break;
		case RT_INCOMPATIBLE_DRIVER:				   /********/
			code = graphics_errc::incompatible_driver; /****/
			break;
		case RT_SHADER_COMPILATION_FAILED:			  /**/
			code = graphics_errc::shader_compilation; /*****/
			break;
		case RT_SHADER_LINK_FAILED:					  /*********/
			code = graphics_errc::shader_link_failed; /*****/
			break;
		case RT_FEATURE_NOT_SUPPORTED:				   /******/
			code = graphics_errc::unsupported_feature; /****/
			break;
		default: UNREACHABLE(); break;
		}
		const char* message = rtErrorMessage();
		if (!message || !message[0]) {
			message = "no backend error message";
		}
		return error(code, lf::format("{} failed: {}", context, message));
	}
	error rutile_error() {
		return rutile_error(rtError(), "Rutile call");
	}

	error init_graphics(span<string_view> args) {
		lf::log::Info("[leaf] Starting graphics...");
		string graphics_api{DefaultGraphicsAPI};
		if (error err = parse_graphics_backend(args, graphics_api)) {
			return err;
		}
		lf::log::Debug("[leaf] Graphics init for '{}'", graphics_api);
		{
			const char* layers[] = { "rt-validation-layer" };
			if (rtLoad(graphics_api.data(), layers, std::size(layers))) {
				lf::log::Error("[leaf] Failed to load graphics backend '{}'", graphics_api);
				return error(generic_errc::unknown, "rtLoad failed");
			}
		}

		lf::log::Debug("[leaf] Loaded graphics backend '{}'", graphics_api);
		rtSetOutput(rutile_log_output, nullptr);
		const char* features[] = { RT_FEATURE_PRESENTATION };
		rtInit(features, std::size(features));

		error err = rutile_error(rtError(), "rtInit");
		if (err) {
			lf::log::Error("Graphics initialization failed: {}", err.message);
			rtUnload();
			return err;
		}
		return error::no_error;
	}

	error init_graphics_extensions() {
		rtLoadSwapchain();
		if (error err = rutile_error(rtError(), "rtLoadSwapchain")) {
			return err;
		}
		rtLoadGlfwSwapchain();
		if (error err = rutile_error(rtError(), "rtLoadGlfwSwapchain")) {
			return err;
		}
		lf::log::Trace("[leaf] Loaded Rutile swapchain and GLFW presentation extensions");
		return error::no_error;
	}

	void exit_graphics() {
		lf::log::Debug("Shutting down graphics: {}", GraphicsBackendName());
		rtExit();
		rtUnload();
	}

	bool graphics_available() {
		return rtLoaded();
	}

	string_view GraphicsBackendName() { return rtGetName(); }
} // namespace rt


