#pragma once

#include <leaf/core/error.hpp>
#include <leaf/core/singleton.hpp>
#include <leaf/core/vector.hpp>

#include <functional>
#include <utility>

namespace lf {
	template<typename Derived, typename Signature>
	class RegisterInstaller;

	template<typename Derived, typename... Arguments>
	class RegisterInstaller<Derived, error(Arguments...)> {
	  public:
		static error install(Arguments... arguments) {
			for (auto& function : Derived::instance().functions) {
				if (auto err = function(arguments...); err) {
					return err;
				}
			}
			return {};
		}
	};

	template<typename Tag, typename Signature>
	class Register : public Singleton<Register<Tag, Signature>>, public RegisterInstaller<Register<Tag, Signature>, Signature> {
	  public:
		using Function = std::function<Signature>;

		static void add(Function function) {
			Register::instance().functions.emplace_back(std::move(function));
		}

	  private:
		friend class RegisterInstaller<Register<Tag, Signature>, Signature>;

		vector<Function> functions;
	};
} // namespace lf
