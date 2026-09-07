#pragma once

#include <type_traits>
#include <utility>

namespace lf {
	template<typename Function>
	class scope_exit {
	  public:
		explicit scope_exit(Function function) noexcept(std::is_nothrow_move_constructible_v<Function>);
		~scope_exit() noexcept;
		scope_exit(const scope_exit&) = delete;
		scope_exit& operator=(const scope_exit&) = delete;
		void release() noexcept;

	  private:
		Function function;
		bool active{ true };
	};

	template<typename Function>
	scope_exit<Function>::scope_exit(Function function) noexcept(std::is_nothrow_move_constructible_v<Function>)
		: function{ std::move(function) } {}

	template<typename Function>
	scope_exit<Function>::~scope_exit() noexcept {
		if (active) { function(); }
	}

	template<typename Function>
	void scope_exit<Function>::release() noexcept { active = false; }
}
