#include "leaf/core/package.hpp"

namespace lf {
	package_decoder::package_decoder(std::shared_ptr<detail::package_decoder_backend> value) : backend(std::move(value)) {}
	package_decoder::package_decoder(const package_decoder&) noexcept = default;
	package_decoder::package_decoder(package_decoder&&) noexcept = default;
	package_decoder& package_decoder::operator=(const package_decoder&) noexcept = default;
	package_decoder& package_decoder::operator=(package_decoder&&) noexcept = default;
	package_decoder::~package_decoder() = default;

	string_view package_decoder::name() const noexcept {
		if (!backend) {
			return "";
		}
		return backend->name();
	}

	report<fs::volume> package_decoder::open(fs::file source) const {
		if (!backend) {
			return unexpected(error(fs::error_code::unsupported_operation, "package decoder is empty"));
		}
		return backend->open(std::move(source));
	}
} // namespace lf
