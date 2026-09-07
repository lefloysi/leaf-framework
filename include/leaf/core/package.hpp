#pragma once

#include "leaf/core/filesystem.hpp"
#include "leaf/core/memory.hpp"
#include "leaf/core/string.hpp"

#include <memory>
#include <type_traits>
#include <utility>

namespace lf {
	class package_decoder;

	namespace detail {
		class package_decoder_backend;
		template<typename Backend>
		class package_decoder_backend_model;
	}

	/*! @brief A copyable type-erased decoder that turns a package file into an fs::volume. */
	class package_decoder {
	  public:
		template<typename Backend>
		explicit package_decoder(Backend backend);

		package_decoder(const package_decoder&) noexcept;
		package_decoder(package_decoder&&) noexcept;
		package_decoder& operator=(const package_decoder&) noexcept;
		package_decoder& operator=(package_decoder&&) noexcept;
		~package_decoder();

		/*! @brief Gets the stable format name used for diagnostics. */
		string_view name() const noexcept;
		/*! @brief Decodes a file explicitly; no format probing occurs. */
		report<fs::volume> open(fs::file source) const;

	  private:
		explicit package_decoder(std::shared_ptr<detail::package_decoder_backend> backend);

		std::shared_ptr<detail::package_decoder_backend> backend;
	};

	namespace detail {
		class package_decoder_backend {
		  public:
			virtual ~package_decoder_backend() = default;
			virtual string_view name() const noexcept = 0;
			virtual report<fs::volume> open(fs::file source) const = 0;
		};

		template<typename Backend>
		class package_decoder_backend_model final : public package_decoder_backend {
		  public:
			explicit package_decoder_backend_model(Backend value);
			string_view name() const noexcept override;
			report<fs::volume> open(fs::file source) const override;

		  private:
			Backend implementation;
		};
	}

	template<typename Backend>
	package_decoder::package_decoder(Backend value) : backend(std::make_shared<detail::package_decoder_backend_model<std::decay_t<Backend>>>(std::move(value))) {}

	template<typename Backend>
	detail::package_decoder_backend_model<Backend>::package_decoder_backend_model(Backend value) : implementation(std::move(value)) {}

	template<typename Backend>
	string_view detail::package_decoder_backend_model<Backend>::name() const noexcept { return implementation.name(); }

	template<typename Backend>
	report<fs::volume> detail::package_decoder_backend_model<Backend>::open(fs::file source) const { return implementation.open(std::move(source)); }
} // namespace lf
