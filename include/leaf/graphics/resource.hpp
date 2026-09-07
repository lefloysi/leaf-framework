#ifndef LEAF_GRAPHICS_RESOURCE_HPP
#define LEAF_GRAPHICS_RESOURCE_HPP

#include <leaf/core/error.hpp>
#include <leaf/core/filesystem.hpp>
#include <leaf/core/logging.hpp>
#include <leaf/core/span.hpp>
#include <leaf/core/string.hpp>
#include <leaf/core/types.hpp>
#include <leaf/core/vector.hpp>
#include <leaf/core/math/dim.hpp>
#include <leaf/core/math/pos.hpp>

#include <leaf/graphics/enums.hpp>

#include <type_traits>

namespace rt {
	namespace fs = lf::fs;
	namespace log = lf::log;
	using ::f32;
	using ::i32;
	using ::u08;
	using ::u16;
	using ::u32;
	using ::u64;
	using ::usize;
	using lf::byte;
	using lf::dim2;
	using lf::error;
	using lf::error_code;
	using lf::generic_errc;
	using lf::graphics_errc;
	using lf::pos2;
	using lf::runtime_exception;
	using lf::span;
	using lf::string;
	using lf::string_view;
	using lf::vector;

	/*!
	** @brief Opaque Rutile buffer resource.
	*/
	struct buffer;

	/*!
	** @brief Opaque Rutile texture resource.
	*/
	struct texture;

	/*!
	** @brief Opaque Rutile texture view resource.
	*/
	struct texture_view;

	/*!
	** @brief Opaque Rutile graphics program resource.
	*/
	struct program;

	/*!
	** @brief Opaque Rutile command buffer resource.
	*/
	struct command_buffer;

	/*!
	** @brief Opaque Rutile framebuffer resource.
	*/
	struct framebuffer;

	/*!
	** @brief Opaque Rutile queue resource.
	*/
	struct queue;

	struct swapchain;

	struct sampler;

	/*!
	** @brief Rutile shader uniform location.
	*/
	using location = rt_location;

	/*!
	** @brief Rutile synchronization timestamp.
	*/
	using timepoint = rt_timepoint;

	/*!
	** @brief Maps a Leaf resource tag to its native handle and destroy routine.
	*/
	template<typename Resource>
	struct resource_traits;

	namespace detail {

		template<typename NativeHandle>
		constexpr NativeHandle null_handle() {
			if constexpr (std::is_pointer_v<NativeHandle>) {
				return nullptr;
			} else {
				return static_cast<NativeHandle>(RT_NULL_HANDLE);
			}
		}

		template<typename NativeHandle>
		struct const_handle {
			using type = NativeHandle;
		};

		template<typename Value>
		struct const_handle<Value*> {
			using type = const Value*;
		};

		template<typename NativeHandle>
		using const_handle_t = typename const_handle<NativeHandle>::type;

	} // namespace detail

	/*!
	** @brief Owning-compatible raw handle for a Rutile-backed resource.
	**
	** A handle is a nullable native object value. It does not destroy the
	** resource by itself; use unique when RAII ownership is needed.
	*/
	template<typename Resource>
	struct handle {
		static_assert(!std::is_const_v<Resource>);
		/*!
		** @brief Native handle type for the resource.
		*/
		using native_handle = resource_traits<Resource>::native_handle;

		/*!
		** @brief Native resource value.
		*/
		native_handle value = detail::null_handle<native_handle>();

		/*!
		** @brief Checks whether the handle is non-null.
		*/
		explicit operator bool() const {
			return value != detail::null_handle<native_handle>();
		}

		/*!
		** @brief Converts to the native Rutile handle.
		*/
		operator native_handle() const {
			return value;
		}
	};

	/*!
	** @brief Non-owning view of a Rutile-backed resource.
	*/
	template<typename Resource>
	struct view {
		/*!
		** @brief Resource type without const qualification.
		*/
		using base_resource = std::remove_const_t<Resource>;

		/*!
		** @brief Resource trait mapping for the base resource.
		*/
		using traits = resource_traits<base_resource>;

		/*!
		** @brief Mutable native handle type.
		*/
		using base_native_handle = traits::native_handle;

		/*!
		** @brief Native handle type with const propagated where supported.
		*/
		using native_handle = std::conditional_t<std::is_const_v<Resource>, detail::const_handle_t<base_native_handle>, base_native_handle>;

		/*!
		** @brief Native resource value.
		*/
		native_handle value = detail::null_handle<native_handle>();

		view() = default;
		view(const view&) = default;
		view& operator=(const view&) = default;

		/*!
		** @brief Creates a view from an owning-compatible handle.
		*/
		view(handle<base_resource> handle) : value(handle.value) {}

		/*!
		** @brief Creates a const view from a mutable view.
		*/
		template<typename OtherResource>
		view(view<OtherResource> view)
			requires(std::is_const_v<Resource> && std::is_same_v<OtherResource, base_resource>)
			: value(view.value) {}

		/*!
		** @brief Checks whether the view is non-null.
		*/
		explicit operator bool() const { return value != detail::null_handle<native_handle>(); }

		/*!
		** @brief Converts to the native Rutile handle.
		*/
		operator native_handle() const { return value; }
	};

	/*!
	** @brief RAII owner for a Leaf resource handle.
	**
	** The owned handle is destroyed through resource_traits when reset or when
	** the unique object is destroyed.
	*/
	template<typename Resource>
	class unique {
	  public:
		using native_handle = resource_traits<Resource>::native_handle;

		/*!
		** @brief Creates an empty owner.
		*/
		unique() = default;
		explicit unique(native_handle value) : resource{ value } {}

		/*!
		** @brief Takes ownership of an existing handle.
		*/
		explicit unique(handle<Resource> handle) : resource(handle) {}
		unique(const unique&) = delete;
		unique& operator=(const unique&) = delete;

		/*!
		** @brief Move-constructs by taking ownership from another unique.
		*/
		unique(unique&& other) noexcept : resource(other.release()) {}

		/*!
		** @brief Move-assigns by replacing the currently owned resource.
		*/
		unique& operator=(unique&& other) noexcept {
			if (this != &other) {
				reset(other.release());
			}
			return *this;
		}

		/*!
		** @brief Destroys the owned resource if one is present.
		*/
		~unique() {
			reset();
		}

		/*!
		** @brief Gets the owned handle without releasing it.
		*/
		handle<Resource> get() const {
			return resource;
		}

		/*!
		** @brief Releases ownership without destroying the resource.
		*/
		handle<Resource> release() {
			handle<Resource> released = resource;
			resource = {};
			return released;
		}

		/*!
		** @brief Replaces the owned resource.
		** @param next New handle to own, or an empty handle.
		*/
		void reset(handle<Resource> next = {}) {
			if (resource) {
				resource_traits<Resource>::destroy(resource.value);
			}
			resource = next;
		}

		/*!
		** @brief Checks whether a resource is currently owned.
		*/
		explicit operator bool() const {
			return static_cast<bool>(resource);
		}

		/*!
		** @brief Converts to a mutable non-owning view.
		*/
		operator view<Resource>() const {
			return view<Resource>(resource);
		}

		/*!
		** @brief Converts to a read-only non-owning view.
		*/
		operator view<const Resource>() const {
			return view<const Resource>(resource);
		}

	  private:
		handle<Resource> resource = {};
	};

	namespace detail {
		void check_rutile_error(string_view context);
	}

#define LEAF_RESOURCE_TRAITS(resource_name, native_name, destroy_func) \
	template<>                                                         \
	struct resource_traits<resource_name> {                            \
		using native_handle = native_name;                             \
		static void destroy(native_handle handle) {                    \
			destroy_func(handle);                                      \
		}                                                              \
	}

	LEAF_RESOURCE_TRAITS(buffer, rt_buffer, rtBufferDestroy);
	LEAF_RESOURCE_TRAITS(texture, rt_texture, rtTextureDestroy);
	LEAF_RESOURCE_TRAITS(texture_view, rt_texture_view, rtTextureViewDestroy);
	LEAF_RESOURCE_TRAITS(program, rt_program, rtProgramDestroy);
	LEAF_RESOURCE_TRAITS(command_buffer, rt_command_buffer, rtCommandBufferDestroy);
	LEAF_RESOURCE_TRAITS(sampler, rt_sampler, rtSamplerDestroy);
	LEAF_RESOURCE_TRAITS(swapchain, rt_swapchain, rtSwapchainDestroy);

#undef LEAF_RESOURCE_TRAITS

	template<>
	struct resource_traits<framebuffer> {
		using native_handle = rt_framebuffer;
		static void destroy(native_handle handle) {
			rtFramebufferDestroy(handle);
		}
	};

	template<>
	struct resource_traits<queue> {
		using native_handle = rt_queue;
		static void destroy(native_handle handle) {
			rtQueueDestroy(handle);
		}
	};

} // namespace rt

#endif /* LEAF_GRAPHICS_RESOURCE_HPP */
